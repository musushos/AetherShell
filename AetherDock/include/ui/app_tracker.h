#ifndef AETHERDOCK_APP_TRACKER_H
#define AETHERDOCK_APP_TRACKER_H

#include "ui/globals.h"

gchar *normalize_app_id(const gchar *app_id);
gchar *normalize_match_key(const gchar *value);
gboolean app_id_equals(const gchar *lhs, const gchar *rhs);
gboolean pinned_app_contains(const gchar *app_id);
gint score_app_id_candidate(const gchar *app_id, const gchar *candidate);
gchar *resolve_desktop_file_path_for_app_id(const gchar *app_id);
gchar *desktop_file_path_from_app_id(const gchar *app_id);
GdkPixbuf *icon_from_app_id(const gchar *app_id);
void load_pinned_apps(void);
void save_pinned_apps(void);

#endif // AETHERDOCK_APP_TRACKER_H
