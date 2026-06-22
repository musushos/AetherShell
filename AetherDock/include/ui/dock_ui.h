#ifndef AETHERDOCK_DOCK_UI_H
#define AETHERDOCK_DOCK_UI_H

#include "ui/globals.h"

void update_window_list(void);
gboolean on_window_draw(GtkWidget *widget, cairo_t *cr, gpointer data);
void on_window_size_allocate(GtkWidget *widget, GtkAllocation *allocation, gpointer data);
gboolean on_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data);
void on_button_clicked(GtkWidget *widget, gpointer data);

#endif // AETHERDOCK_DOCK_UI_H
