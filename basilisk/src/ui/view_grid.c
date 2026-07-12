#include "ui/view_grid.h"
#include "core/search_ctrl.h"
#include "core/cmd_ctrl.h"
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkwayland.h>
#include <gio/gdesktopappinfo.h>

// Dimensions
#define WINDOW_WIDTH 650
#define WINDOW_HEIGHT 500
#define GRID_COLS 5
#define GRID_ROWS 3
#define ICON_SIZE 64

typedef struct {
    GtkWidget *window;
    GtkWidget *search_entry;
    GtkWidget *app_grid;
    GtkWidget *category_bar;
    GtkWidget *scroll_window;
    
    gboolean visible;
    gboolean search_hint_visible;
    AppCategory current_category;
} ViewGridState;

static ViewGridState S = {0};
static GtkWidget *category_buttons[CAT_COUNT];
static const gchar *search_hint_text = "BasiliskSearch ";

static void on_window_realize_disable_decorations(GtkWidget *widget, gpointer data) {
    GdkWindow *gdk_window;
    (void)data;
    gdk_window = gtk_widget_get_window(widget);
    if (!gdk_window) return;
    if (GDK_IS_WAYLAND_WINDOW(gdk_window)) {
        gdk_wayland_window_announce_csd(gdk_window);
    } else {
        gdk_window_set_decorations(gdk_window, 0);
        gdk_window_set_functions(gdk_window, 0);
    }
}

static void apply_css(void) {
    const gchar *css_data =
        "* { padding: 0; margin: 0; }"
        "window { background-color: transparent; }"
        "box { background-color: transparent; }"
        "grid { background-color: transparent; }"
        "viewport { background-color: transparent; }"
        "scrolledwindow { background-color: transparent; }"
        "#app-shell { background-color: rgba(0, 0, 0, 0.32); border-radius: 20px; border: 1px solid rgba(0, 0, 0, 0.38); padding: 0px; }"
        "#title-bar { background-color: transparent; padding: 16px 18px 8px 18px; }"
        "#app-title { color: rgba(255, 255, 255, 0.9); font-size: 20px; font-weight: 700; }"
        "#title-icon { color: rgba(255, 255, 255, 0.8); font-size: 18px; font-weight: 700; margin-right: 6px; }"
        "#more-button { background-color: transparent; border: none; border-radius: 50%; color: rgba(0, 0, 0, 0.6); padding: 4px 8px; }"
        "#more-button:hover { background-color: rgba(0, 0, 0, 0.12); }"
        "#category-bar { background-color: transparent; padding: 0px 18px 10px 18px; }"
        ".category-pill { background-color: rgba(190,200,225,0.55); border: 1px solid rgba(255,255,255,0.60); border-radius: 20px; color: rgba(255, 255, 255, 0.75); font-size: 12px; font-weight: 500; padding: 4px 12px; margin-right: 6px; }"
        ".category-pill:hover { background-color: rgba(170,185,215,0.75); color: rgba(255, 255, 255, 0.9); }"
        ".category-pill.active { background-color: rgba(255,255,255,0.80); border-color: rgba(255,255,255,0.90); color: rgba(255, 255, 255, 0.95); font-weight: 600; }"
        "#cat-separator { background-color: rgba(150,165,200,0.30); min-height: 1px; }"
        "#scroll-area { background-color: transparent; }"
        "#app-grid-inner { background-color: transparent; padding: 16px 18px 16px 18px; }"
        ".app-button { background-color: transparent; border: none; border-radius: 16px; padding: 10px 8px 8px 8px; min-width: 100px; min-height: 100px; }"
        ".app-button:hover { background-color: rgba(150,165,210,0.22); }"
        ".app-button:active { background-color: rgba(120,140,190,0.35); }"
        ".app-icon { margin-bottom: 6px; }"
        ".app-name { color: rgba(255, 255, 255, 1); font-size: 11px; font-weight: 400; }"
        "#search-entry { background-color: rgba(200,210,235,0.60); border: 1px solid rgba(255,255,255,0.55); border-radius: 10px; color: rgba(255, 255, 255, 0.9); font-size: 14px; padding: 8px 12px; margin: 0px 18px 10px 18px; }"
        "#search-entry:focus { border-color: rgba(0, 0, 0, 0.6); background-color: rgba(0, 0, 0, 0.54); }";
        
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, css_data, -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

static void on_app_clicked(GtkButton *button, gpointer data) {
    (void)button;
    const gchar *desktop_file = (const gchar *)data;
    GDesktopAppInfo *app_info = g_desktop_app_info_new_from_filename(desktop_file);
    if (app_info) {
        g_app_info_launch(G_APP_INFO(app_info), NULL, NULL, NULL);
        g_object_unref(app_info);
    }
    view_grid_hide();
}

static GtkWidget* create_app_button(AppEntry *app) {
    GtkWidget *button = gtk_button_new();
    GtkStyleContext *ctx = gtk_widget_get_style_context(button);
    gtk_style_context_add_class(ctx, "app-button");

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_halign(vbox, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(vbox, GTK_ALIGN_CENTER);
    gtk_container_add(GTK_CONTAINER(button), vbox);

    GtkWidget *icon = NULL;
    if (app->icon) {
        GtkIconTheme *theme = gtk_icon_theme_get_default();
        if (g_path_is_absolute(app->icon)) {
            GdkPixbuf *pb = gdk_pixbuf_new_from_file_at_scale(app->icon, ICON_SIZE, ICON_SIZE, TRUE, NULL);
            if (pb) { icon = gtk_image_new_from_pixbuf(pb); g_object_unref(pb); }
        } else {
            GdkPixbuf *pb = gtk_icon_theme_load_icon(theme, app->icon, ICON_SIZE, GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
            if (pb) { icon = gtk_image_new_from_pixbuf(pb); g_object_unref(pb); }
        }
    }
    if (!icon) {
        icon = gtk_image_new_from_icon_name("application-x-executable", GTK_ICON_SIZE_DIALOG);
        gtk_image_set_pixel_size(GTK_IMAGE(icon), ICON_SIZE);
    }
    gtk_style_context_add_class(gtk_widget_get_style_context(icon), "app-icon");
    gtk_box_pack_start(GTK_BOX(vbox), icon, FALSE, FALSE, 0);

    GtkWidget *label = gtk_label_new(app->name);
    gtk_style_context_add_class(gtk_widget_get_style_context(label), "app-name");
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(label), 11);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
    gtk_widget_set_margin_top(label, 6);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

    g_signal_connect(button, "clicked", G_CALLBACK(on_app_clicked), app->desktop_file);
    return button;
}

static void refresh_grid(void) {
    GList *children = gtk_container_get_children(GTK_CONTAINER(S.app_grid));
    for (GList *l = children; l != NULL; l = l->next)
        gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(children);

    const gchar *search_text = gtk_entry_get_text(GTK_ENTRY(S.search_entry));
    const gchar *query = (!S.search_hint_visible && search_text && strlen(search_text) > 0) ? search_text : NULL;

    GList *filtered_apps = search_ctrl_get_filtered_apps(S.current_category, query);
    
    int count = 0;
    int max_apps = GRID_COLS * GRID_ROWS;

    for (GList *l = filtered_apps; l != NULL && count < max_apps; l = l->next) {
        AppEntry *app = (AppEntry *)l->data;
        GtkWidget *btn = create_app_button(app);
        gtk_grid_attach(GTK_GRID(S.app_grid), btn, count % GRID_COLS, count / GRID_COLS, 1, 1);
        count++;
    }

    g_list_free(filtered_apps);
    gtk_widget_show_all(S.app_grid);
}

static void on_category_clicked(GtkButton *button, gpointer data) {
    AppCategory cat = GPOINTER_TO_INT(data);
    S.current_category = cat;

    for (int i = 0; i < CAT_COUNT; i++) {
        GtkStyleContext *ctx = gtk_widget_get_style_context(category_buttons[i]);
        if (i == (int)cat) gtk_style_context_add_class(ctx, "active");
        else gtk_style_context_remove_class(ctx, "active");
    }

    refresh_grid();
    (void)button;
}

extern void cmd_ctrl_execute_all(const gchar *query); // To be added in cmd_ctrl.h

static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data) {
    (void)widget; (void)data;
    if (event->keyval == GDK_KEY_Escape) {
        view_grid_hide();
        return TRUE;
    }
    if (event->keyval == GDK_KEY_Return) {
        const gchar *text = gtk_entry_get_text(GTK_ENTRY(S.search_entry));
        if (!S.search_hint_visible && text[0]) {
            cmd_ctrl_execute_all(text); // Controller handles execution
            return TRUE;
        }
    }
    return FALSE;
}

static gboolean on_window_delete(GtkWidget *widget, GdkEvent *event, gpointer data) {
    (void)widget; (void)event; (void)data;
    view_grid_hide();
    return TRUE;
}

static void set_search_hint_visible(gboolean visible) {
    if (!S.search_entry) return;
    GtkStyleContext *ctx = gtk_widget_get_style_context(S.search_entry);
    S.search_hint_visible = visible;
    if (visible) {
        gtk_style_context_add_class(ctx, "search-hint");
        gtk_entry_set_text(GTK_ENTRY(S.search_entry), search_hint_text);
        gtk_editable_set_position(GTK_EDITABLE(S.search_entry), 0);
    } else {
        gtk_style_context_remove_class(ctx, "search-hint");
        gtk_entry_set_text(GTK_ENTRY(S.search_entry), "");
    }
}

static gboolean on_search_focus_out(GtkWidget *widget, GdkEventFocus *event, gpointer data) {
    (void)widget; (void)event; (void)data;
    const gchar *text = gtk_entry_get_text(GTK_ENTRY(S.search_entry));
    if (!text || text[0] == '\0') set_search_hint_visible(TRUE);
    return FALSE;
}

static gboolean on_search_entry_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data) {
    (void)widget; (void)data;
    if (!S.search_hint_visible) return FALSE;
    gboolean printable = g_unichar_isprint(gdk_keyval_to_unicode(event->keyval));
    if (printable || event->keyval == GDK_KEY_BackSpace || event->keyval == GDK_KEY_Delete)
        set_search_hint_visible(FALSE);
    return FALSE;
}

static void on_search_changed(GtkEditable *editable, gpointer data) {
    (void)editable; (void)data;
    refresh_grid();
}

void view_grid_init(void) {
    apply_css();

    S.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(S.window), "Applications");
    gtk_window_set_decorated(GTK_WINDOW(S.window), FALSE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(S.window), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(S.window), TRUE);
    gtk_window_set_type_hint(GTK_WINDOW(S.window), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_keep_above(GTK_WINDOW(S.window), TRUE);
    gtk_widget_set_app_paintable(S.window, TRUE);
    gtk_window_set_default_size(GTK_WINDOW(S.window), WINDOW_WIDTH, WINDOW_HEIGHT);
    gtk_window_set_resizable(GTK_WINDOW(S.window), FALSE);
    g_signal_connect(S.window, "realize", G_CALLBACK(on_window_realize_disable_decorations), NULL);

    GdkScreen *screen = gtk_widget_get_screen(S.window);
    GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
    if (visual) gtk_widget_set_visual(S.window, visual);

    GtkWidget *shell = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(shell, "app-shell");
    gtk_container_add(GTK_CONTAINER(S.window), shell);

    GtkWidget *title_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_name(title_bar, "title-bar");
    gtk_box_pack_start(GTK_BOX(shell), title_bar, FALSE, FALSE, 0);

    GtkWidget *icon_lbl = gtk_label_new("✦");
    gtk_widget_set_name(icon_lbl, "title-icon");
    gtk_box_pack_start(GTK_BOX(title_bar), icon_lbl, FALSE, FALSE, 0);

    GtkWidget *title_lbl = gtk_label_new("Applications");
    gtk_widget_set_name(title_lbl, "app-title");
    gtk_box_pack_start(GTK_BOX(title_bar), title_lbl, FALSE, FALSE, 0);

    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_pack_start(GTK_BOX(title_bar), spacer, TRUE, TRUE, 0);

    GtkWidget *more_btn = gtk_button_new_with_label("···");
    gtk_widget_set_name(more_btn, "more-button");
    gtk_box_pack_end(GTK_BOX(title_bar), more_btn, FALSE, FALSE, 0);

    S.category_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_name(S.category_bar, "category-bar");
    gtk_widget_set_halign(S.category_bar, GTK_ALIGN_FILL);
    const gchar *names[] = {"All", "Development", "System", "Internet", "Utility", "Other"};
    for (int i = 0; i < CAT_COUNT; i++) {
        GtkWidget *btn = gtk_button_new_with_label(names[i]);
        gtk_style_context_add_class(gtk_widget_get_style_context(btn), "category-pill");
        if (i == 0) gtk_style_context_add_class(gtk_widget_get_style_context(btn), "active");
        g_signal_connect(btn, "clicked", G_CALLBACK(on_category_clicked), GINT_TO_POINTER(i));
        gtk_box_pack_start(GTK_BOX(S.category_bar), btn, FALSE, FALSE, 0);
        category_buttons[i] = btn;
    }
    gtk_box_pack_start(GTK_BOX(shell), S.category_bar, FALSE, FALSE, 0);

    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_name(sep, "cat-separator");
    gtk_box_pack_start(GTK_BOX(shell), sep, FALSE, FALSE, 0);

    S.search_entry = gtk_entry_new();
    gtk_widget_set_name(S.search_entry, "search-entry");
    gtk_entry_set_placeholder_text(GTK_ENTRY(S.search_entry), "Search Applications...");
    gtk_widget_set_no_show_all(S.search_entry, FALSE);
    gtk_box_pack_start(GTK_BOX(shell), S.search_entry, FALSE, FALSE, 0);

    S.scroll_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_name(S.scroll_window, "scroll-area");
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(S.scroll_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(S.scroll_window, TRUE);
    gtk_box_pack_start(GTK_BOX(shell), S.scroll_window, TRUE, TRUE, 0);

    GtkWidget *grid_wrap = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(grid_wrap, "app-grid-inner");
    gtk_container_add(GTK_CONTAINER(S.scroll_window), grid_wrap);

    S.app_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(S.app_grid), 4);
    gtk_grid_set_column_spacing(GTK_GRID(S.app_grid), 4);
    gtk_widget_set_halign(S.app_grid, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(S.app_grid, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(grid_wrap), S.app_grid, TRUE, TRUE, 0);

    g_signal_connect(S.window, "key-press-event", G_CALLBACK(on_key_press), NULL);
    g_signal_connect(S.search_entry, "changed", G_CALLBACK(on_search_changed), NULL);
    g_signal_connect(S.search_entry, "focus-out-event", G_CALLBACK(on_search_focus_out), NULL);
    g_signal_connect(S.search_entry, "key-press-event", G_CALLBACK(on_search_entry_key_press), NULL);
    g_signal_connect(S.window, "delete-event", G_CALLBACK(on_window_delete), NULL);

    S.visible = FALSE;
    S.search_hint_visible = FALSE;
    S.current_category = CAT_ALL;
    set_search_hint_visible(TRUE);
}

void view_grid_show(void) { g_print("view_grid_show called! S.visible = %d\n", S.visible);
    if (!S.visible) {
        set_search_hint_visible(TRUE);
        S.current_category = CAT_ALL;
        for (int i = 0; i < CAT_COUNT; i++) {
            if (i == 0) gtk_style_context_add_class(gtk_widget_get_style_context(category_buttons[i]), "active");
            else gtk_style_context_remove_class(gtk_widget_get_style_context(category_buttons[i]), "active");
        }
        refresh_grid();

        GdkDisplay *display = gdk_display_get_default();
        GdkMonitor *monitor = gdk_display_get_primary_monitor(display);
        if (!monitor && gdk_display_get_n_monitors(display) > 0)
            monitor = gdk_display_get_monitor(display, 0);

        gint x = 0, y = 0;
        if (monitor) {
            GdkRectangle geometry;
            gdk_monitor_get_geometry(monitor, &geometry);
            x = geometry.x + (geometry.width  - WINDOW_WIDTH)  / 2;
            y = geometry.y + (geometry.height - WINDOW_HEIGHT) / 2;
        } else {
            gtk_window_set_position(GTK_WINDOW(S.window), GTK_WIN_POS_CENTER);
        }

        gtk_window_move(GTK_WINDOW(S.window), x, y);
        gtk_widget_show_all(S.window);
        gtk_window_present(GTK_WINDOW(S.window));
        g_timeout_add(50, (GSourceFunc)gtk_widget_grab_focus, S.search_entry);

        S.visible = TRUE;
    }
}

void view_grid_hide(void) {
    if (S.visible) {
        gtk_widget_hide(S.window);
        S.visible = FALSE;
    }
}

void view_grid_show_with_search(const gchar *query) {
    view_grid_show();
    if (query && query[0] != '\0') {
        set_search_hint_visible(FALSE);
        gtk_entry_set_text(GTK_ENTRY(S.search_entry), query);
        refresh_grid();
    }
}

void view_grid_toggle(void) {
    if (S.visible) view_grid_hide();
    else view_grid_show();
}
