#include <gtk/gtk.h>

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), "Test < a");
    printf("Text: %s\n", gtk_label_get_text(GTK_LABEL(label)));
    return 0;
}
