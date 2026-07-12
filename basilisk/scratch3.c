#include <gtk/gtk.h>

static void on_copy(GtkButton *btn, gpointer user_data) {
    gtk_button_set_label(btn, "Copied");
}

int main(int argc, char **argv) {
    gtk_init(&argc, &argv);
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    GtkWidget *tv = gtk_text_view_new();
    gtk_container_add(GTK_CONTAINER(win), tv);
    
    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv));
    gtk_text_buffer_set_text(buf, "Here is some code:\n", -1);
    
    GtkTextIter iter;
    gtk_text_buffer_get_end_iter(buf, &iter);
    GtkTextChildAnchor *anchor = gtk_text_buffer_create_child_anchor(buf, &iter);
    
    GtkWidget *btn = gtk_button_new_with_label("Copy");
    gtk_text_view_add_child_at_anchor(GTK_TEXT_VIEW(tv), btn, anchor);
    
    gtk_text_buffer_get_end_iter(buf, &iter);
    gtk_text_buffer_insert(buf, &iter, "\nint x = 0;\n", -1);
    
    gtk_widget_show_all(win);
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    gtk_main();
    return 0;
}
