/*
 * video_wallpaper.c
 * Video wallpaper engine using libmpv's OpenGL render API in GtkGLArea.
 */

#include "video_wallpaper.h"
#include "desktop_config.h"

#ifdef HAVE_MPV
#include <mpv/client.h>
#include <mpv/render_gl.h>
#include <epoxy/gl.h>
#include <epoxy/egl.h>
#include <string.h>
#include <stdio.h>
#include <locale.h>
#include <gdk/gdkwayland.h>
#include <malloc.h>

/* ── Module-private state ───────────────────────────────────────── */
#include "wallpaper.h"
#include "layer_manager.h"

static mpv_handle         *g_mpv      = NULL;
static mpv_render_context *g_mpv_ctx  = NULL;
static GtkWidget          *g_gl_area  = NULL;
static gboolean            g_active   = FALSE;
static char               *g_cur_path = NULL;
static volatile gint       g_pending  = 0;

extern GtkWidget *icon_layout;

static int read_saved_volume(void);

static void *get_proc_address_mpv(void *ctx, const char *name) {
    (void)ctx;
    return (void *)epoxy_eglGetProcAddress(name);
}

static gboolean do_update_in_main_thread(gpointer data) {
    (void)data;
    
    if (!g_mpv_ctx || !g_gl_area || !g_active) {
        g_atomic_int_set(&g_pending, 0);
        return G_SOURCE_REMOVE;
    }

    uint64_t flags = mpv_render_context_update(g_mpv_ctx);
    if (flags & MPV_RENDER_UPDATE_FRAME) {
        gtk_widget_queue_draw(g_gl_area);
    }
    
    g_atomic_int_set(&g_pending, 0);
    return G_SOURCE_REMOVE;
}

/* ── mpv update callback (mpv thread → schedule idle) ────────────── */
static void on_mpv_update(void *ctx) {
    (void)ctx;
    if (!g_mpv_ctx || !g_gl_area || !g_active) return;
    
    if (g_atomic_int_compare_and_exchange(&g_pending, 0, 1)) {
        g_idle_add(do_update_in_main_thread, NULL);
    }
}

static void on_gl_area_realize(GtkWidget *widget, gpointer user_data) {
    (void)user_data;
    gtk_gl_area_make_current(GTK_GL_AREA(widget));
    if (gtk_gl_area_get_error(GTK_GL_AREA(widget)) != NULL) {
        g_warning("[VideoWallpaper] GtkGLArea failed to realize");
        return;
    }

    const char *renderer = (const char *)glGetString(GL_RENDERER);
    g_print("[VideoWallpaper] OpenGL Renderer: %s\n", renderer ? renderer : "Unknown");

    if (g_mpv && !g_mpv_ctx) {
        mpv_opengl_init_params gl_init_params = {
            .get_proc_address = get_proc_address_mpv,
        };
        
        GdkDisplay *display = gdk_display_get_default();
        struct wl_display *wl_disp = NULL;
        if (GDK_IS_WAYLAND_DISPLAY(display)) {
            wl_disp = gdk_wayland_display_get_wl_display(display);
        }

        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_API_TYPE, MPV_RENDER_API_TYPE_OPENGL},
            {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init_params},
            {MPV_RENDER_PARAM_WL_DISPLAY, wl_disp},
            {MPV_RENDER_PARAM_INVALID, NULL}
        };

        if (mpv_render_context_create(&g_mpv_ctx, g_mpv, params) == 0) {
            mpv_render_context_set_update_callback(g_mpv_ctx, on_mpv_update, NULL);
            g_print("[VideoWallpaper] OpenGL render context ready\n");
        }
    }
}

static void on_gl_area_unrealize(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    (void)user_data;
    if (g_mpv_ctx) {
        gtk_gl_area_make_current(GTK_GL_AREA(widget));
        mpv_render_context_free(g_mpv_ctx);
        g_mpv_ctx = NULL;
    }
}

static gboolean on_gl_area_render(GtkGLArea *area, GdkGLContext *context, gpointer user_data) {
    (void)context;
    (void)user_data;

    if (!g_mpv_ctx || !g_active) {
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        return TRUE;
    }

    int w = gtk_widget_get_allocated_width(GTK_WIDGET(area));
    int h = gtk_widget_get_allocated_height(GTK_WIDGET(area));
    int scale = gtk_widget_get_scale_factor(GTK_WIDGET(area));
    w *= scale;
    h *= scale;

    int fbo = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fbo);

    mpv_opengl_fbo mpfbo = {
        .fbo = fbo,
        .w = w,
        .h = h,
        .internal_format = 0,
    };
    int flip_y = 1;

    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_OPENGL_FBO, &mpfbo},
        {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
        {MPV_RENDER_PARAM_INVALID, NULL}
    };

    mpv_render_context_render(g_mpv_ctx, params);
    return TRUE;
}

/* ── Read saved volume ────────────────────────────────────────────── */
static int read_saved_volume(void)
{
    int vol = 0;
    char *main_config = get_vaxp_main_config_path();
    GKeyFile *kf = g_key_file_new();
    if (g_key_file_load_from_file(kf, main_config, G_KEY_FILE_NONE, NULL)) {
        GError *err = NULL;
        int v = g_key_file_get_integer(kf, "Desktop", "VideoVolume", &err);
        if (!err) vol = CLAMP(v, 0, 100);
        else g_error_free(err);
    }
    g_key_file_free(kf);
    g_free(main_config);
    return vol;
}

/* ── Public API ───────────────────────────────────────────────────── */

gboolean is_video_file(const char *path)
{
    if (!path || !*path) return FALSE;
    static const char * const exts[] = {
        ".mp4", ".mkv", ".webm", ".avi", ".mov",
        ".flv", ".wmv", ".m4v",  ".ogv", ".ts",
        ".m2ts",".mpg", ".mpeg", ".3gp", ".hevc", NULL
    };
    char *lo = g_ascii_strdown(path, -1);
    gboolean ok = FALSE;
    for (int i = 0; exts[i]; i++)
        if (g_str_has_suffix(lo, exts[i])) { ok = TRUE; break; }
    g_free(lo);
    return ok;
}

gboolean video_wallpaper_init(GtkWidget *window) {
    (void)window;
    setlocale(LC_NUMERIC, "C");
    return TRUE;
}

void video_wallpaper_load(const char *path)
{
    if (!path || !*path) return;

    if (g_active && g_cur_path && g_strcmp0(g_cur_path, path) == 0)
        return;

    g_active = TRUE;
    g_free(g_cur_path);
    g_cur_path = g_strdup(path);

    /* Ensure GtkGLArea exists */
    if (!g_gl_area) {
        g_gl_area = gtk_gl_area_new();
        gtk_gl_area_set_has_alpha(GTK_GL_AREA(g_gl_area), FALSE);
        gtk_widget_set_hexpand(g_gl_area, TRUE);
        gtk_widget_set_vexpand(g_gl_area, TRUE);

        g_signal_connect(g_gl_area, "realize", G_CALLBACK(on_gl_area_realize), NULL);
        g_signal_connect(g_gl_area, "unrealize", G_CALLBACK(on_gl_area_unrealize), NULL);
        g_signal_connect(g_gl_area, "render", G_CALLBACK(on_gl_area_render), NULL);
    }

    /* Ensure mpv is initialized FIRST before realizing the widget */
    if (!g_mpv) {
        setlocale(LC_NUMERIC, "C");
        g_mpv = mpv_create();
        if (g_mpv) {
            mpv_set_option_string(g_mpv, "hwdec", "auto-safe");
            mpv_set_option_string(g_mpv, "gpu-context", "wayland");
            mpv_set_option_string(g_mpv, "vo", "libmpv");
            mpv_set_option_string(g_mpv, "loop", "inf");
            mpv_set_option_string(g_mpv, "really-quiet", "yes");
            mpv_set_option_string(g_mpv, "no-audio-display", "yes");
            char vol_str[16];
            snprintf(vol_str, sizeof(vol_str), "%d", read_saved_volume());
            mpv_set_option_string(g_mpv, "volume", vol_str);

            mpv_initialize(g_mpv);
        }
    }

    layer_manager_show_video(g_gl_area);



    const char *cmd[] = { "loadfile", path, "replace", NULL };
    if (g_mpv) {
        mpv_command(g_mpv, cmd);
    }

    g_print("[VideoWallpaper] Loaded: %s\n", path);
}

void video_wallpaper_stop(void)
{
    if (!g_active) return;
    g_active = FALSE;

    g_free(g_cur_path);
    g_cur_path = NULL;

    /* 1. Destroy GtkGLArea and render context FIRST! 
       libmpv requires the render context (g_mpv_ctx) to be freed BEFORE the mpv handle (g_mpv) is destroyed.
       layer_manager_show_image destroys the video widget, triggering on_gl_area_unrealize, which frees g_mpv_ctx. */
    layer_manager_show_image(wallpaper_get_widget());
    g_gl_area = NULL;

    /* 2. Now safely destroy the mpv handle */
    if (g_mpv) {
        mpv_terminate_destroy(g_mpv);
        g_mpv = NULL;
    }
    g_gl_area = NULL;

    g_print("[VideoWallpaper] Stopped and completely destroyed\n");
    malloc_trim(0);
}

gboolean video_wallpaper_is_active(void)
{
    return g_active;
}

void video_wallpaper_set_volume(int volume)
{
    if (!g_mpv) return;
    volume = CLAMP(volume, 0, 100);

    char vs[16];
    snprintf(vs, sizeof(vs), "%d", volume);

    mpv_set_property_string(g_mpv, "volume", vs);

    ensure_config_dir();
    char *main_config = get_vaxp_main_config_path();
    GKeyFile *kf = g_key_file_new();
    g_key_file_load_from_file(kf, main_config, G_KEY_FILE_NONE, NULL);
    g_key_file_set_integer(kf, "Desktop", "VideoVolume", volume);
    g_key_file_save_to_file(kf, main_config, NULL);
    g_key_file_free(kf);
    g_free(main_config);
}

#else /* HAVE_MPV */

#include <glib.h>

gboolean is_video_file(const char *path) {
    if (!path || !*path) return FALSE;
    static const char * const exts[] = {
        ".mp4", ".mkv", ".webm", ".avi", ".mov",
        ".flv", ".wmv", ".m4v",  ".ogv", ".ts",
        ".m2ts",".mpg", ".mpeg", ".3gp", ".hevc", NULL
    };
    char *lo = g_ascii_strdown(path, -1);
    gboolean ok = FALSE;
    for (int i = 0; exts[i]; i++)
        if (g_str_has_suffix(lo, exts[i])) { ok = TRUE; break; }
    g_free(lo);
    return ok;
}

gboolean video_wallpaper_init(GtkWidget *gl_area) {
    (void)gl_area;
    g_warning("[VideoWallpaper] Video wallpaper support is disabled (compiled without libmpv-dev)");
    return FALSE;
}

void video_wallpaper_load(const char *path) {
    g_warning("[VideoWallpaper] Cannot play '%s': Video wallpaper support is disabled", path);
}

void video_wallpaper_stop(void) {
}

gboolean video_wallpaper_is_active(void) {
    return FALSE;
}

void video_wallpaper_set_volume(int volume) {
    (void)volume;
}

#endif /* HAVE_MPV */
