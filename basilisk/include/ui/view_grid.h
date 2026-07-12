#ifndef UI_VIEW_GRID_H
#define UI_VIEW_GRID_H

#include <glib.h>

void view_grid_init(void);
void view_grid_show(void);
void view_grid_hide(void);
void view_grid_toggle(void);
void view_grid_show_with_search(const gchar *query);

#endif
