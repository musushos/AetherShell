#include "auth_ui.h"
#include <gtk/gtk.h>
#include <gtk-layer-shell.h>
#include <gdk/gdkkeysyms.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>

#define DIALOG_WIDTH_POLKIT 400
#define DIALOG_HEIGHT_POLKIT 250

#define DIALOG_WIDTH_MINIMAL 360
#define DIALOG_HEIGHT_MINIMAL 350

#define DIALOG_WIDTH_TERMINAL 440
#define DIALOG_HEIGHT_TERMINAL 200

typedef struct {
    GMainLoop *loop;
    GtkWidget *window;
    GtkWidget *entry;
    char *password_out;
    int max_length;
    int result;
} AuthDialogState;

static gboolean ui_ready = FALSE;
static GtkCssProvider *current_provider = NULL;
static AuthConfig current_config;

static void secure_clear(void *ptr, size_t len) {
    volatile unsigned char *p = ptr;
    while (len-- > 0) {
        *p++ = 0;
    }
}

static void clear_entry_text(GtkWidget *entry) {
    if (!GTK_IS_ENTRY(entry)) return;
    GtkEntryBuffer *buffer = gtk_entry_get_buffer(GTK_ENTRY(entry));
    if (!buffer) return;
    guint len = gtk_entry_buffer_get_length(buffer);
    if (len == 0) return;
    char *overwrite = g_malloc0((gsize)len + 1);
    memset(overwrite, ' ', len);
    gtk_entry_buffer_set_text(buffer, overwrite, (gint)len);
    gtk_entry_buffer_set_text(buffer, "", 0);
    secure_clear(overwrite, len);
    g_free(overwrite);
}

static gboolean is_wayland_session(void) {
    const char *session_type = g_getenv("XDG_SESSION_TYPE");
    const char *wayland_display = g_getenv("WAYLAND_DISPLAY");
    if (wayland_display && *wayland_display) return TRUE;
    return session_type && g_strcmp0(session_type, "wayland") == 0;
}

static char* find_user_avatar(const char *username) {
    if (username) {
        struct passwd *pw = getpwnam(username);
        if (pw && pw->pw_dir) {
            char *face_path = g_build_filename(pw->pw_dir, ".face", NULL);
            if (g_file_test(face_path, G_FILE_TEST_EXISTS)) return face_path;
            g_free(face_path);
            
            face_path = g_build_filename(pw->pw_dir, ".face.icon", NULL);
            if (g_file_test(face_path, G_FILE_TEST_EXISTS)) return face_path;
            g_free(face_path);
        }
        
        char *as_path = g_build_filename("/var/lib/AccountsService/icons", username, NULL);
        if (g_file_test(as_path, G_FILE_TEST_EXISTS)) return as_path;
        g_free(as_path);
    }
    return g_strdup("vaxp-logo.png");
}

static GtkWidget* create_avatar_widget(const char *username, int size) {
    char *avatar_path = find_user_avatar(username);
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_scale(avatar_path, size, size, TRUE, NULL);
    g_free(avatar_path);
    if (pixbuf) {
        GtkWidget *image = gtk_image_new_from_pixbuf(pixbuf);
        g_object_unref(pixbuf);
        return image;
    }
    return gtk_image_new_from_icon_name("avatar-default", GTK_ICON_SIZE_DIALOG);
}

static void load_css(void) {
    if (current_provider) {
        GdkScreen *screen = gdk_screen_get_default();
        if (screen) {
            gtk_style_context_remove_provider_for_screen(screen, GTK_STYLE_PROVIDER(current_provider));
        }
        g_object_unref(current_provider);
        current_provider = NULL;
    }

    gchar *css = NULL;

    if (current_config.theme == THEME_MINIMAL) {
        css = g_strdup_printf(
            "/* Main dialog window background and padding */\n"
            ".min-dlg { background: %s; border-radius: 20px; padding: 32px 28px; }\n"
            "/* Title text (e.g. 'Confirm Identity') */\n"
            ".min-title { font-size: 16px; font-weight: bold; color: %s; margin-bottom: 6px; }\n"
            "/* Subtitle text */\n"
            ".min-sub { font-size: 13px; color: %s; margin-bottom: 22px; }\n"
            "/* Dynamic request description text */\n"
            ".min-desc { font-size: 13px; color: %s; margin-bottom: 10px; }\n"
            "/* Password input field background and text */\n"
            ".min-input { background: %s; color: %s; border: none; border-radius: 12px; padding: 10px 14px; }\n"
            "/* Password input field when focused */\n"
            ".min-input:focus { background: %s; border: 2px solid %s; }\n"
            "/* Confirm/Authenticate button */\n"
            ".min-btn-primary { background: %s; color: %s; border-radius: 12px; padding: 10px; font-weight: bold; }\n"
            "/* Confirm/Authenticate button on hover */\n"
            ".min-btn-primary:hover { background: %s; }\n"
            "/* Cancel button */\n"
            ".min-btn-secondary { background: %s; color: %s; border-radius: 12px; padding: 10px; }\n"
            "/* Cancel button on hover */\n"
            ".min-btn-secondary:hover { background: %s; }\n"
            "/* Avatar ring container */\n"
            ".min-ring { background: %s; border-radius: 32px; min-width: 64px; min-height: 64px; }",
            current_config.minimal.bg, current_config.minimal.text,
            current_config.minimal.text, current_config.minimal.text,
            current_config.minimal.input_bg, current_config.minimal.input_text,
            current_config.minimal.input_bg, current_config.minimal.input_focus,
            current_config.minimal.btn_primary, current_config.minimal.text,
            current_config.minimal.btn_primary_hover,
            current_config.minimal.btn_secondary, current_config.minimal.input_focus,
            current_config.minimal.btn_secondary_hover,
            current_config.minimal.ring
        );
    } else if (current_config.theme == THEME_TERMINAL) {
        css = g_strdup_printf(
            "/* Main dialog window background and border */\n"
            ".term-dlg { background: %s; border-radius: 10px; border: 1px solid %s; font-family: monospace; }\n"
            "/* Top title bar (fake window controls area) */\n"
            ".term-bar { background: %s; padding: 8px 12px; }\n"
            "/* Fake window control button base class */\n"
            ".term-dot { border-radius: 10px; min-width: 12px; min-height: 12px; }\n"
            "/* Close button color */\n"
            ".term-dot-red { background-color: %s; }\n"
            "/* Minimize button color */\n"
            ".term-dot-yellow { background-color: %s; }\n"
            "/* Maximize button color */\n"
            ".term-dot-green { background-color: %s; }\n"
            "/* Top bar title/path text */\n"
            ".term-path { color: %s; font-size: 11px; }\n"
            "/* Main terminal body container */\n"
            ".term-body { padding: 20px; font-size: 13px; color: %s; }\n"
            "/* Prompt text (e.g. user@host $) */\n"
            ".term-prompt { color: %s; font-weight: bold; }\n"
            "/* Dynamic request description text */\n"
            ".term-cmd { color: %s; }\n"
            "/* Warning text (if any) */\n"
            ".term-warn { color: %s; }\n"
            "/* Password input row container */\n"
            ".term-input-row { background: %s; border: 1px solid %s; border-radius: 6px; padding: 6px; }\n"
            "/* Password input row when focused */\n"
            ".term-input-row:focus-within { border-color: %s; }\n"
            "/* 'password:' prefix text */\n"
            ".term-pfx { color: %s; }\n"
            "/* Password input field */\n"
            ".term-input { background: transparent; color: %s; border: none; }\n"
            "/* Bottom hint text (e.g. Press Enter...) */\n"
            ".term-hint { color: %s; font-size: 11px; margin-top: 10px; }",
            current_config.terminal.bg, current_config.terminal.border, current_config.terminal.bar,
            current_config.terminal.dot_red, current_config.terminal.dot_yellow, current_config.terminal.dot_green,
            current_config.terminal.path, current_config.terminal.text, current_config.terminal.prompt,
            current_config.terminal.cmd, current_config.terminal.warn,
            current_config.terminal.input_bg, current_config.terminal.border, current_config.terminal.prompt,
            current_config.terminal.prompt, current_config.terminal.input_text, current_config.terminal.hint
        );
    } else {
        css = g_strdup_printf(
            "/* Main dialog window background and border */\n"
            ".pk-dlg { background: %s; border-radius: 10px; border: 1px solid %s; }\n"
            "/* Header section padding */\n"
            ".pk-head { padding: 18px 20px 0; }\n"
            "/* Lock icon background container */\n"
            ".pk-lock { background: %s; border-radius: 10px; min-width: 44px; min-height: 44px; }\n"
            "/* Main title text */\n"
            ".pk-title { font-size: 14px; font-weight: bold; color: %s; }\n"
            "/* Subtitle/App description text */\n"
            ".pk-app { font-size: 12px; color: %s; }\n"
            "/* Dynamic request description text */\n"
            ".pk-desc { font-size: 12px; color: %s; }\n"
            "/* Horizontal divider line */\n"
            ".pk-divider { background: %s; min-height: 1px; }\n"
            "/* Main body container padding */\n"
            ".pk-body { padding: 0 20px 20px; }\n"
            "/* User information row background */\n"
            ".pk-user-row { background: %s; border-radius: 8px; padding: 8px 10px; }\n"
            "/* User avatar background/border */\n"
            ".pk-avatar { background: %s; border-radius: 14px; min-width: 28px; min-height: 28px; }\n"
            "/* User name text */\n"
            ".pk-user-label { font-size: 12px; color: %s; }\n"
            "/* Password input field */\n"
            ".pk-input { background: %s; color: %s; border: none; border-radius: 8px; padding: 8px 12px; }\n"
            "/* Password input field when focused */\n"
            ".pk-input:focus { background: %s; border: 2px solid %s; }\n"
            "/* Bottom actions bar background */\n"
            ".pk-actions { background: transparent; padding: 16px 20px; }\n"
            "/* Standard button (Cancel) */\n"
            ".pk-btn { background: %s; color: %s; border: none; border-radius: 8px; padding: 8px; }\n"
            "/* Standard button on hover */\n"
            ".pk-btn:hover { background: %s; }\n"
            "/* Primary button (Authenticate) */\n"
            ".pk-btn-primary { background: %s; color: %s; border: none; font-weight: bold; }\n"
            "/* Primary button on hover */\n"
            ".pk-btn-primary:hover { background: %s; }",
            current_config.polkit.bg, current_config.polkit.border,
            current_config.polkit.accent,
            current_config.polkit.text, current_config.polkit.text, current_config.polkit.text,
            current_config.polkit.accent,
            current_config.polkit.user_row_bg,
            current_config.polkit.accent, current_config.polkit.text,
            current_config.polkit.input_bg, current_config.polkit.input_text,
            current_config.polkit.input_bg, current_config.polkit.accent,
            current_config.polkit.btn_bg, current_config.polkit.text, current_config.polkit.btn_hover,
            current_config.polkit.accent, current_config.polkit.text, current_config.polkit.accent_hover
        );
    }

    current_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(current_provider, css, -1, NULL);
    g_free(css);

    GdkScreen *screen = gdk_screen_get_default();
    if (screen) {
        gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER(current_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
}

void auth_ui_set_theme(AuthConfig *config) {
    if (memcmp(&current_config, config, sizeof(AuthConfig)) != 0 || !current_provider) {
        memcpy(&current_config, config, sizeof(AuthConfig));
        load_css();
    }
}

static void center_layer_shell_window(GtkWindow *window, int w, int h) {
    GdkDisplay *display = gdk_display_get_default();
    GdkMonitor *monitor = display ? gdk_display_get_primary_monitor(display) : NULL;
    if (!monitor) return;
    GdkRectangle workarea = {0};
    gdk_monitor_get_workarea(monitor, &workarea);
    gtk_layer_set_monitor(window, monitor);
    gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_RIGHT, FALSE);
    gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_BOTTOM, FALSE);
    gtk_layer_set_margin(window, GTK_LAYER_SHELL_EDGE_LEFT, workarea.x + (workarea.width - w) / 2);
    gtk_layer_set_margin(window, GTK_LAYER_SHELL_EDGE_TOP, workarea.y + (workarea.height - h) / 2);
}

static gboolean focus_entry_idle(gpointer user_data) {
    GtkWidget *entry = GTK_WIDGET(user_data);
    if (!GTK_IS_WIDGET(entry)) return G_SOURCE_REMOVE;
    gtk_widget_grab_focus(entry);
    return G_SOURCE_REMOVE;
}

static void finish_dialog(AuthDialogState *state, int result) {
    if (!state) return;
    state->result = result;
    if (state->loop && g_main_loop_is_running(state->loop)) {
        g_main_loop_quit(state->loop);
    }
}

static void accept_dialog(GtkButton *button, gpointer user_data) {
    (void)button;
    AuthDialogState *state = user_data;
    const char *text = gtk_entry_get_text(GTK_ENTRY(state->entry));
    size_t len = strlen(text);
    if (len == 0 || len >= (size_t)state->max_length) return;
    g_strlcpy(state->password_out, text, (gsize)state->max_length);
    finish_dialog(state, UI_RESULT_SUCCESS);
}

static void cancel_dialog(GtkButton *button, gpointer user_data) {
    (void)button;
    finish_dialog((AuthDialogState *)user_data, UI_RESULT_CANCEL);
}

static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    (void)widget;
    if (event->keyval == GDK_KEY_Escape) {
        finish_dialog((AuthDialogState *)user_data, UI_RESULT_CANCEL);
        return TRUE;
    }
    return FALSE;
}

static gboolean on_window_delete(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
    (void)widget; (void)event;
    finish_dialog((AuthDialogState *)user_data, UI_RESULT_CANCEL);
    return TRUE;
}

int auth_ui_init(AuthConfig *config) {
    if (ui_ready) return 0;
    if (!gtk_init_check(NULL, NULL)) return -1;
    ui_ready = TRUE;
    gtk_window_set_default_icon_name("dialog-password");
    auth_ui_set_theme(config);
    return 0;
}

void auth_ui_cleanup(void) {}

static void build_minimal_ui(AuthDialogState *state, const char *message, const char *username) {
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(card), "min-dlg");
    gtk_container_add(GTK_CONTAINER(state->window), card);

    GtkWidget *ring = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(ring), "min-ring");
    gtk_widget_set_halign(ring, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_bottom(ring, 18);
    gtk_box_pack_start(GTK_BOX(card), ring, FALSE, FALSE, 0);
    
    GtkWidget *avatar = create_avatar_widget(username, 64);
    gtk_box_pack_start(GTK_BOX(ring), avatar, TRUE, TRUE, 0);

    GtkWidget *title = gtk_label_new("Confirm Identity");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "min-title");
    gtk_box_pack_start(GTK_BOX(card), title, FALSE, FALSE, 0);

    GtkWidget *desc = gtk_label_new(message ? message : "Authentication required");
    gtk_style_context_add_class(gtk_widget_get_style_context(desc), "min-desc");
    gtk_label_set_line_wrap(GTK_LABEL(desc), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(desc), 30);
    gtk_box_pack_start(GTK_BOX(card), desc, FALSE, FALSE, 0);

    state->entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(state->entry), FALSE);
    gtk_entry_set_invisible_char(GTK_ENTRY(state->entry), 0x2022);
    gtk_entry_set_activates_default(GTK_ENTRY(state->entry), TRUE);
    gtk_entry_set_placeholder_text(GTK_ENTRY(state->entry), "Password");
    gtk_style_context_add_class(gtk_widget_get_style_context(state->entry), "min-input");
    gtk_box_pack_start(GTK_BOX(card), state->entry, FALSE, FALSE, 8);

    GtkWidget *actions = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_top(actions, 14);
    gtk_box_pack_start(GTK_BOX(card), actions, FALSE, FALSE, 0);

    GtkWidget *ok = gtk_button_new_with_label("Confirm");
    gtk_style_context_add_class(gtk_widget_get_style_context(ok), "min-btn-primary");
    gtk_widget_set_can_default(ok, TRUE);
    gtk_box_pack_start(GTK_BOX(actions), ok, FALSE, FALSE, 0);

    GtkWidget *cancel = gtk_button_new_with_label("Cancel");
    gtk_style_context_add_class(gtk_widget_get_style_context(cancel), "min-btn-secondary");
    gtk_box_pack_start(GTK_BOX(actions), cancel, FALSE, FALSE, 0);

    g_signal_connect(ok, "clicked", G_CALLBACK(accept_dialog), state);
    g_signal_connect(cancel, "clicked", G_CALLBACK(cancel_dialog), state);
    g_signal_connect(state->entry, "activate", G_CALLBACK(accept_dialog), state);
    gtk_widget_grab_default(ok);
}

static void build_terminal_ui(AuthDialogState *state, const char *message, const char *username) {
    GtkWidget *dlg = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(dlg), "term-dlg");
    gtk_container_add(GTK_CONTAINER(state->window), dlg);

    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_style_context_add_class(gtk_widget_get_style_context(bar), "term-bar");
    gtk_box_pack_start(GTK_BOX(dlg), bar, FALSE, FALSE, 0);

    GtkWidget *d1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(d1), "term-dot");
    gtk_style_context_add_class(gtk_widget_get_style_context(d1), "term-dot-red");
    gtk_box_pack_start(GTK_BOX(bar), d1, FALSE, FALSE, 0);

    GtkWidget *d2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(d2), "term-dot");
    gtk_style_context_add_class(gtk_widget_get_style_context(d2), "term-dot-yellow");
    gtk_box_pack_start(GTK_BOX(bar), d2, FALSE, FALSE, 0);

    GtkWidget *d3 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(d3), "term-dot");
    gtk_style_context_add_class(gtk_widget_get_style_context(d3), "term-dot-green");
    gtk_box_pack_start(GTK_BOX(bar), d3, FALSE, FALSE, 0);

    GtkWidget *path = gtk_label_new("auth — password prompt");
    gtk_style_context_add_class(gtk_widget_get_style_context(path), "term-path");
    gtk_widget_set_halign(path, GTK_ALIGN_END);
    gtk_widget_set_valign(path, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(path, TRUE);
    gtk_box_pack_start(GTK_BOX(bar), path, TRUE, TRUE, 0);

    GtkWidget *body = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_style_context_add_class(gtk_widget_get_style_context(body), "term-body");
    gtk_box_pack_start(GTK_BOX(dlg), body, TRUE, TRUE, 0);

    GtkWidget *cmd_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    char prompt_str[256];
    snprintf(prompt_str, sizeof(prompt_str), "%s@%s $", username ? username : "user", g_get_host_name());
    GtkWidget *prompt = gtk_label_new(prompt_str);
    gtk_style_context_add_class(gtk_widget_get_style_context(prompt), "term-prompt");
    gtk_widget_set_valign(prompt, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(cmd_row), prompt, FALSE, FALSE, 0);
    
    GtkWidget *cmd = gtk_label_new(message ? message : "Authentication required");
    gtk_label_set_line_wrap(GTK_LABEL(cmd), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(cmd), 40);
    gtk_style_context_add_class(gtk_widget_get_style_context(cmd), "term-cmd");
    gtk_box_pack_start(GTK_BOX(cmd_row), cmd, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(body), cmd_row, FALSE, FALSE, 0);

    GtkWidget *input_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_style_context_add_class(gtk_widget_get_style_context(input_row), "term-input-row");
    gtk_widget_set_margin_top(input_row, 10);
    gtk_box_pack_start(GTK_BOX(body), input_row, FALSE, FALSE, 0);

    GtkWidget *pfx = gtk_label_new("password:");
    gtk_style_context_add_class(gtk_widget_get_style_context(pfx), "term-pfx");
    gtk_box_pack_start(GTK_BOX(input_row), pfx, FALSE, FALSE, 0);

    state->entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(state->entry), FALSE);
    gtk_entry_set_has_frame(GTK_ENTRY(state->entry), FALSE);
    gtk_entry_set_activates_default(GTK_ENTRY(state->entry), TRUE);
    gtk_style_context_add_class(gtk_widget_get_style_context(state->entry), "term-input");
    gtk_box_pack_start(GTK_BOX(input_row), state->entry, TRUE, TRUE, 0);

    GtkWidget *hint = gtk_label_new("Press Enter to authenticate · Esc to cancel");
    gtk_widget_set_halign(hint, GTK_ALIGN_START);
    gtk_style_context_add_class(gtk_widget_get_style_context(hint), "term-hint");
    gtk_box_pack_start(GTK_BOX(body), hint, FALSE, FALSE, 0);

    g_signal_connect(state->entry, "activate", G_CALLBACK(accept_dialog), state);
}

static void build_polkit_ui(AuthDialogState *state, const char *message, const char *username) {
    GtkWidget *dlg = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(dlg), "pk-dlg");
    gtk_container_add(GTK_CONTAINER(state->window), dlg);

    GtkWidget *head = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
    gtk_style_context_add_class(gtk_widget_get_style_context(head), "pk-head");
    gtk_box_pack_start(GTK_BOX(dlg), head, FALSE, FALSE, 0);

    GtkWidget *lock_bg = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(lock_bg), "pk-lock");
    gtk_widget_set_halign(lock_bg, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(lock_bg, GTK_ALIGN_CENTER);
    GtkWidget *lock_icon = gtk_image_new_from_icon_name("dialog-password", GTK_ICON_SIZE_MENU);
    gtk_box_pack_start(GTK_BOX(lock_bg), lock_icon, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(head), lock_bg, FALSE, FALSE, 0);

    GtkWidget *head_text = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_box_pack_start(GTK_BOX(head), head_text, TRUE, TRUE, 0);
    GtkWidget *title = gtk_label_new("Administrative privileges required");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "pk-title");
    gtk_box_pack_start(GTK_BOX(head_text), title, FALSE, FALSE, 0);
    GtkWidget *app = gtk_label_new(message ? message : "An application is attempting to perform an action that requires privileges.");
    gtk_widget_set_halign(app, GTK_ALIGN_START);
    gtk_label_set_line_wrap(GTK_LABEL(app), TRUE);
    gtk_style_context_add_class(gtk_widget_get_style_context(app), "pk-app");
    gtk_box_pack_start(GTK_BOX(head_text), app, FALSE, FALSE, 0);

    GtkWidget *div = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(div), "pk-divider");
    gtk_widget_set_margin_top(div, 18);
    gtk_widget_set_margin_bottom(div, 16);
    gtk_box_pack_start(GTK_BOX(dlg), div, FALSE, FALSE, 0);

    GtkWidget *body = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_style_context_add_class(gtk_widget_get_style_context(body), "pk-body");
    gtk_box_pack_start(GTK_BOX(dlg), body, FALSE, FALSE, 0);

    GtkWidget *user_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_style_context_add_class(gtk_widget_get_style_context(user_row), "pk-user-row");
    gtk_box_pack_start(GTK_BOX(body), user_row, FALSE, FALSE, 0);
    
    GtkWidget *avatar = create_avatar_widget(username, 28);
    gtk_style_context_add_class(gtk_widget_get_style_context(avatar), "pk-avatar");
    gtk_box_pack_start(GTK_BOX(user_row), avatar, FALSE, FALSE, 0);
    
    char auth_str[256];
    snprintf(auth_str, sizeof(auth_str), "Authenticating as: %s", username ? username : "User");
    GtkWidget *user_label = gtk_label_new(auth_str);
    
    gtk_style_context_add_class(gtk_widget_get_style_context(user_label), "pk-user-label");
    gtk_box_pack_start(GTK_BOX(user_row), user_label, FALSE, FALSE, 0);

    state->entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(state->entry), FALSE);
    gtk_entry_set_invisible_char(GTK_ENTRY(state->entry), 0x2022);
    gtk_entry_set_activates_default(GTK_ENTRY(state->entry), TRUE);
    gtk_entry_set_placeholder_text(GTK_ENTRY(state->entry), "Password");
    gtk_style_context_add_class(gtk_widget_get_style_context(state->entry), "pk-input");
    gtk_box_pack_start(GTK_BOX(body), state->entry, FALSE, FALSE, 0);

    GtkWidget *actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_style_context_add_class(gtk_widget_get_style_context(actions), "pk-actions");
    gtk_box_set_homogeneous(GTK_BOX(actions), TRUE);
    gtk_box_pack_end(GTK_BOX(dlg), actions, FALSE, FALSE, 0);

    GtkWidget *cancel = gtk_button_new_with_label("Cancel");
    gtk_style_context_add_class(gtk_widget_get_style_context(cancel), "pk-btn");
    gtk_box_pack_start(GTK_BOX(actions), cancel, TRUE, TRUE, 0);

    GtkWidget *ok = gtk_button_new_with_label("Authenticate");
    gtk_style_context_add_class(gtk_widget_get_style_context(ok), "pk-btn");
    gtk_style_context_add_class(gtk_widget_get_style_context(ok), "pk-btn-primary");
    gtk_widget_set_can_default(ok, TRUE);
    gtk_box_pack_start(GTK_BOX(actions), ok, TRUE, TRUE, 0);

    g_signal_connect(ok, "clicked", G_CALLBACK(accept_dialog), state);
    g_signal_connect(cancel, "clicked", G_CALLBACK(cancel_dialog), state);
    g_signal_connect(state->entry, "activate", G_CALLBACK(accept_dialog), state);
    gtk_widget_grab_default(ok);
}

int auth_ui_get_password(const char *message, const char *username, char *password_out, int max_length) {
    if (!ui_ready || !password_out || max_length <= 1) return UI_RESULT_ERROR;
    password_out[0] = '\0';

    AuthDialogState state = {
        .loop = g_main_loop_new(NULL, FALSE),
        .window = gtk_window_new(GTK_WINDOW_TOPLEVEL),
        .entry = NULL,
        .password_out = password_out,
        .max_length = max_length,
        .result = UI_RESULT_CANCEL,
    };

    if (!state.loop || !state.window) {
        if (state.loop) g_main_loop_unref(state.loop);
        if (state.window) gtk_widget_destroy(state.window);
        return UI_RESULT_ERROR;
    }

    int w = DIALOG_WIDTH_POLKIT, h = DIALOG_HEIGHT_POLKIT;
    if (current_config.theme == THEME_MINIMAL) { w = DIALOG_WIDTH_MINIMAL; h = DIALOG_HEIGHT_MINIMAL; }
    else if (current_config.theme == THEME_TERMINAL) { w = DIALOG_WIDTH_TERMINAL; h = DIALOG_HEIGHT_TERMINAL; }

    gtk_window_set_title(GTK_WINDOW(state.window), "Authentication Required");
    gtk_window_set_default_size(GTK_WINDOW(state.window), w, h);
    gtk_window_set_resizable(GTK_WINDOW(state.window), FALSE);
    gtk_window_set_decorated(GTK_WINDOW(state.window), FALSE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(state.window), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(state.window), TRUE);
    gtk_window_set_keep_above(GTK_WINDOW(state.window), TRUE);
    gtk_window_set_modal(GTK_WINDOW(state.window), TRUE);
    gtk_widget_set_size_request(state.window, w, h);

    // Transparency for rounded corners
    GdkVisual *visual = gdk_screen_get_rgba_visual(gtk_window_get_screen(GTK_WINDOW(state.window)));
    if (visual) gtk_widget_set_visual(state.window, visual);
    gtk_widget_set_app_paintable(state.window, TRUE);

    if (is_wayland_session() && gtk_layer_is_supported()) {
        gtk_layer_init_for_window(GTK_WINDOW(state.window));
        gtk_layer_set_namespace(GTK_WINDOW(state.window), "vaxp-auth");
        gtk_layer_set_layer(GTK_WINDOW(state.window), GTK_LAYER_SHELL_LAYER_OVERLAY);
        gtk_layer_set_keyboard_mode(GTK_WINDOW(state.window), GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);
        gtk_layer_set_exclusive_zone(GTK_WINDOW(state.window), 0);
        center_layer_shell_window(GTK_WINDOW(state.window), w, h);
    } else {
        gtk_window_set_position(GTK_WINDOW(state.window), GTK_WIN_POS_CENTER_ALWAYS);
    }

    if (current_config.theme == THEME_MINIMAL) build_minimal_ui(&state, message, username);
    else if (current_config.theme == THEME_TERMINAL) build_terminal_ui(&state, message, username);
    else build_polkit_ui(&state, message, username);

    g_signal_connect(state.window, "delete-event", G_CALLBACK(on_window_delete), &state);
    g_signal_connect(state.window, "key-press-event", G_CALLBACK(on_key_press), &state);

    gtk_widget_show_all(state.window);
    gtk_window_present(GTK_WINDOW(state.window));
    g_idle_add(focus_entry_idle, state.entry);

    g_main_loop_run(state.loop);

    clear_entry_text(state.entry);
    if (GTK_IS_WIDGET(state.window)) gtk_widget_destroy(state.window);
    g_main_loop_unref(state.loop);

    if (state.result != UI_RESULT_SUCCESS) {
        secure_clear(password_out, (size_t)max_length);
    }
    return state.result;
}
