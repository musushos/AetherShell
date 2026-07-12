#ifndef UI_VIEW_DIALOGS_H
#define UI_VIEW_DIALOGS_H

#include <gtk/gtk.h>
#include <glib.h>

// Show calculator result dialog
void view_dialog_show_math(const gchar *expr, const gchar *result);

// Show file search results dialog
void view_dialog_show_file_search(GList *results);

#endif
