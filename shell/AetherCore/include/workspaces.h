#ifndef WORKSPACES_H
#define WORKSPACES_H

#include <gtk/gtk.h>

GtkWidget* create_workspaces_widget(void);
void workspaces_set_orientation(GtkWidget *widget, GtkOrientation orientation);

#endif // WORKSPACES_H
