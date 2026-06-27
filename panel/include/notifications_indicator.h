#ifndef NOTIFICATIONS_INDICATOR_H
#define NOTIFICATIONS_INDICATOR_H

#include <gtk/gtk.h>

/*
 * Returns a GtkButton whose face is a fully custom Cairo-drawn indicator:
 *   – A bell silhouette
 *   – The notification count rendered as a number inside/above the bell
 *     (hidden / shown as "0" when empty, coloured accent when non-zero)
 *
 * Clicking the button toggles the notifications popup window.
 * The indicator redraws automatically every time the notification list changes.
 */
GtkWidget *create_notifications_indicator_widget(GtkWidget *notif_window);

/*
 * Called by notifications_ui.c inside on_notifications_updated().
 * Pushes the new count to the indicator so it redraws itself.
 */
void notifications_indicator_set_count(guint count);

#endif /* NOTIFICATIONS_INDICATOR_H */
