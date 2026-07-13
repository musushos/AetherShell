#include "layer_manager.h"

static GtkWidget *g_layer_manager = NULL;

void layer_manager_init(GtkWidget *main_window) {
    if (g_layer_manager) return;

    /* Create the Layer Manager (GtkStack) */
    g_layer_manager = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(g_layer_manager), GTK_STACK_TRANSITION_TYPE_NONE);
    gtk_widget_show(g_layer_manager);

    /* Attach the layer manager to the main window */
    gtk_container_add(GTK_CONTAINER(main_window), g_layer_manager);
}

void layer_manager_show_image(GtkWidget *image_widget) {
    if (!g_layer_manager || !image_widget) return;

    /* If the image widget isn't already in the stack, add it */
    if (gtk_widget_get_parent(image_widget) != g_layer_manager) {
        gtk_stack_add_named(GTK_STACK(g_layer_manager), image_widget, "image_layer");
    }

    /* Switch the stack to show the image layer */
    gtk_stack_set_visible_child_name(GTK_STACK(g_layer_manager), "image_layer");

    /* Find and completely destroy the video layer to free its memory */
    GtkWidget *vid_child = gtk_stack_get_child_by_name(GTK_STACK(g_layer_manager), "video_layer");
    if (vid_child) {
        gtk_widget_destroy(vid_child);
    }
}

void layer_manager_show_video(GtkWidget *video_widget) {
    if (!g_layer_manager || !video_widget) return;

    /* Ensure any old video widget is destroyed if it's different */
    GtkWidget *old_vid = gtk_stack_get_child_by_name(GTK_STACK(g_layer_manager), "video_layer");
    if (old_vid && old_vid != video_widget) {
        gtk_widget_destroy(old_vid);
    }

    /* Add the new video widget to the stack */
    if (gtk_widget_get_parent(video_widget) != g_layer_manager) {
        gtk_stack_add_named(GTK_STACK(g_layer_manager), video_widget, "video_layer");
    }

    gtk_widget_show_all(video_widget);

    /* Switch the stack to show the video layer */
    gtk_stack_set_visible_child_name(GTK_STACK(g_layer_manager), "video_layer");
}
