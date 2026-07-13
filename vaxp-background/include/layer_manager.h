#ifndef LAYER_MANAGER_H
#define LAYER_MANAGER_H

#include <gtk/gtk.h>

void layer_manager_init(GtkWidget *main_window);
void layer_manager_show_image(GtkWidget *image_widget);
void layer_manager_show_video(GtkWidget *video_widget);

#endif /* LAYER_MANAGER_H */
