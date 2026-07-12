#ifndef CORE_SEARCH_CTRL_H
#define CORE_SEARCH_CTRL_H

#include <glib.h>
#include "state.h"

// Initialize app cache
void search_ctrl_init(void);

// Free app cache
void search_ctrl_cleanup(void);

// Get filtered list of applications (returns a newly allocated GList containing pointers to the cached AppEntries).
// The caller must free the GList using g_list_free, but NOT the AppEntries themselves.
GList* search_ctrl_get_filtered_apps(AppCategory category, const gchar *query);

#endif
