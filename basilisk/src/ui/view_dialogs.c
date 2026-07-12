#include "ui/view_dialogs.h"
#include <gdk/gdkx.h>
#include <gdk/gdkwayland.h>

static void on_dialog_realize_disable_decorations(GtkWidget *widget, gpointer data) {
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

void view_dialog_show_math(const gchar *expr, const gchar *result) {
    GtkWidget *dialog = gtk_message_dialog_new(
        NULL,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_INFO,
        GTK_BUTTONS_CLOSE,
        "%s\n= %s", expr, result);
    
    gtk_window_set_title(GTK_WINDOW(dialog), "Calculator");
    gtk_window_set_decorated(GTK_WINDOW(dialog), FALSE);
    g_signal_connect(dialog, "realize",
                     G_CALLBACK(on_dialog_realize_disable_decorations), NULL);
    
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,
        "dialog { background: rgba(30, 30, 35, 0.95); }"
        "dialog label { color: #ffffff; font-size: 18px; }"
        "dialog button { color: #ffffff; }", -1, NULL);
    gtk_style_context_add_provider(
        gtk_widget_get_style_context(dialog),
        GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);
    
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static void on_file_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer dialog) {
    (void)box;
    GtkWidget *child = gtk_bin_get_child(GTK_BIN(row));
    const gchar *path = g_object_get_data(G_OBJECT(child), "path");
    
    if (path) {
        gchar *dir = g_path_get_dirname(path);
        gchar *uri = g_filename_to_uri(dir, NULL, NULL);
        if (uri) {
            g_app_info_launch_default_for_uri(uri, NULL, NULL);
            g_free(uri);
        }
        g_free(dir);
    }
    
    gtk_widget_destroy(GTK_WIDGET(dialog));
}

void view_dialog_show_file_search(GList *results) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "File Search", NULL,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "_Close", GTK_RESPONSE_CLOSE, NULL);
    
    gtk_window_set_default_size(GTK_WINDOW(dialog), 600, 400);
    gtk_window_set_decorated(GTK_WINDOW(dialog), FALSE);
    g_signal_connect(dialog, "realize",
                     G_CALLBACK(on_dialog_realize_disable_decorations), NULL);
    
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,
        "dialog { background: rgba(30, 30, 35, 0.95); }"
        "dialog label { color: #ffffff; }"
        "dialog button { color: #ffffff; }"
        "list { background: transparent; }"
        "row { padding: 8px; }"
        "row:hover { background: rgba(0, 212, 255, 0.2); }", -1, NULL);
    gtk_style_context_add_provider(
        gtk_widget_get_style_context(dialog),
        GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);
    
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_container_add(GTK_CONTAINER(content), scroll);
    
    GtkWidget *listbox = gtk_list_box_new();
    gtk_container_add(GTK_CONTAINER(scroll), listbox);
    g_signal_connect(listbox, "row-activated", G_CALLBACK(on_file_row_activated), dialog);
    
    for (GList *l = results; l != NULL; l = l->next) {
        const gchar *path = (const gchar *)l->data;
        GtkWidget *label = gtk_label_new(path);
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        g_object_set_data_full(G_OBJECT(label), "path", g_strdup(path), g_free);
        gtk_container_add(GTK_CONTAINER(listbox), label);
    }
    
    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}
