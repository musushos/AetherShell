#include "ui/app_tracker.h"

gchar *normalize_app_id(const gchar *app_id) {
    gchar *trimmed;
    gchar *normalized;
    size_t len;

    if (!app_id || !*app_id) {
        return g_strdup("unknown");
    }

    trimmed = g_strdup(app_id);
    g_strstrip(trimmed);
    len = strlen(trimmed);
    if (len > 8 && g_str_has_suffix(trimmed, ".desktop")) {
        trimmed[len - 8] = '\0';
    }

    normalized = g_ascii_strdown(trimmed, -1);
    g_free(trimmed);
    return normalized;
}

gchar *normalize_match_key(const gchar *value) {
    gchar *normalized;
    gchar *base;
    gchar *compact;
    gsize len = 0;

    if (!value || !*value) {
        return g_strdup("");
    }

    normalized = normalize_app_id(value);
    base = g_path_get_basename(normalized);

    compact = g_malloc0(strlen(base) + 1);
    for (const gchar *p = base; *p != '\0'; p++) {
        if (g_ascii_isalnum(*p)) {
            compact[len++] = g_ascii_tolower(*p);
        }
    }

    g_free(base);
    g_free(normalized);
    return compact;
}

gboolean app_id_equals(const gchar *lhs, const gchar *rhs) {
    gchar *left_norm;
    gchar *right_norm;
    gboolean equal;

    if (lhs == NULL || rhs == NULL) {
        return FALSE;
    }

    left_norm = normalize_app_id(lhs);
    right_norm = normalize_app_id(rhs);
    equal = g_strcmp0(left_norm, right_norm) == 0;
    g_free(left_norm);
    g_free(right_norm);
    return equal;
}

gint score_app_id_candidate(const gchar *app_id, const gchar *candidate) {
    gchar *normalized_app_id;
    gchar *normalized_candidate;
    gchar *compact_app_id;
    gchar *compact_candidate;
    gint score = 0;

    if (!app_id || !*app_id || !candidate || !*candidate) {
        return 0;
    }

    normalized_app_id = normalize_app_id(app_id);
    normalized_candidate = normalize_app_id(candidate);
    compact_app_id = normalize_match_key(app_id);
    compact_candidate = normalize_match_key(candidate);

    if (g_strcmp0(normalized_app_id, normalized_candidate) == 0) {
        score = 100;
    } else if (*compact_app_id != '\0' && g_strcmp0(compact_app_id, compact_candidate) == 0) {
        score = 90;
    } else if (*normalized_app_id != '\0' &&
               (g_str_has_suffix(normalized_app_id, normalized_candidate) ||
                g_str_has_suffix(normalized_candidate, normalized_app_id))) {
        score = 75;
    } else if (*compact_app_id != '\0' &&
               (g_str_has_suffix(compact_app_id, compact_candidate) ||
                g_str_has_suffix(compact_candidate, compact_app_id))) {
        score = 70;
    } else if (*compact_app_id != '\0' &&
               (strstr(compact_app_id, compact_candidate) != NULL ||
                strstr(compact_candidate, compact_app_id) != NULL)) {
        score = 50;
    }

    g_free(normalized_app_id);
    g_free(normalized_candidate);
    g_free(compact_app_id);
    g_free(compact_candidate);
    return score;
}

gboolean pinned_app_contains(const gchar *app_id) {
    for (GList *l = pinned_apps; l != NULL; l = l->next) {
        if (app_id_equals((const gchar *)l->data, app_id)) {
            return TRUE;
        }
    }

    return FALSE;
}

gchar *resolve_desktop_file_path_for_app_id(const gchar *app_id) {
    static GHashTable *resolved_paths = NULL;
    gchar *normalized_app_id;
    const gchar *cached_value;
    GDesktopAppInfo *app_info = NULL;
    gchar *desktop_id;
    gchar *resolved_path = NULL;

    if (!app_id || !*app_id) {
        return NULL;
    }

    if (resolved_paths == NULL) {
        resolved_paths = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    }

    normalized_app_id = normalize_app_id(app_id);
    cached_value = g_hash_table_lookup(resolved_paths, normalized_app_id);
    if (cached_value != NULL) {
        gchar *result = (*cached_value != '\0') ? g_strdup(cached_value) : NULL;
        g_free(normalized_app_id);
        return result;
    }

    desktop_id = g_strdup_printf("%s.desktop", normalized_app_id);
    app_info = g_desktop_app_info_new(desktop_id);
    g_free(desktop_id);

    if (app_info == NULL) {
        desktop_id = g_strdup_printf("%s.desktop", app_id);
        app_info = g_desktop_app_info_new(desktop_id);
        g_free(desktop_id);
    }

    if (app_info != NULL) {
        resolved_path = g_strdup(g_desktop_app_info_get_filename(app_info));
        g_object_unref(app_info);
    } else {
        GList *all_apps = g_app_info_get_all();
        gint best_score = 0;

        for (GList *l = all_apps; l != NULL; l = l->next) {
            GAppInfo *candidate = G_APP_INFO(l->data);
            gint score = 0;

            if (!G_IS_DESKTOP_APP_INFO(candidate) || !g_app_info_should_show(candidate)) {
                continue;
            }

            const gchar *filename = g_desktop_app_info_get_filename(G_DESKTOP_APP_INFO(candidate));
            const gchar *startup_wm_class = g_desktop_app_info_get_startup_wm_class(G_DESKTOP_APP_INFO(candidate));
            const gchar *executable = g_app_info_get_executable(candidate);

            score = MAX(score, score_app_id_candidate(normalized_app_id, filename));
            score = MAX(score, score_app_id_candidate(normalized_app_id, startup_wm_class));
            score = MAX(score, score_app_id_candidate(normalized_app_id, executable));
            score = MAX(score, score_app_id_candidate(normalized_app_id, g_app_info_get_id(candidate)));
            score = MAX(score, score_app_id_candidate(normalized_app_id, g_app_info_get_name(candidate)));

            if (score > best_score) {
                g_free(resolved_path);
                resolved_path = g_strdup(filename);
                best_score = score;
                if (best_score >= 100) {
                    break;
                }
            }
        }

        g_list_free_full(all_apps, g_object_unref);
    }

    g_hash_table_insert(resolved_paths,
                        g_strdup(normalized_app_id),
                        g_strdup(resolved_path != NULL ? resolved_path : ""));
    g_free(normalized_app_id);
    return resolved_path;
}

gchar *desktop_file_path_from_app_id(const gchar *app_id) {
    return resolve_desktop_file_path_for_app_id(app_id);
}

GdkPixbuf *icon_from_app_id(const gchar *app_id) {
    GtkIconTheme *icon_theme = gtk_icon_theme_get_default();
    GDesktopAppInfo *app_info = NULL;
    GdkPixbuf *pixbuf = NULL;
    gchar *normalized_app_id;
    gchar *desktop_file_path;

    if (!app_id || !*app_id) {
        return gtk_icon_theme_load_icon(icon_theme, "application-x-executable", 48,
                                        GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
    }

    normalized_app_id = normalize_app_id(app_id);
    desktop_file_path = resolve_desktop_file_path_for_app_id(app_id);

    if (desktop_file_path != NULL) {
        app_info = g_desktop_app_info_new_from_filename(desktop_file_path);
    }

    if (app_info != NULL) {
        gchar *icon_name = g_desktop_app_info_get_string(app_info, "Icon");
        if (icon_name != NULL) {
            if (g_path_is_absolute(icon_name)) {
                pixbuf = gdk_pixbuf_new_from_file_at_scale(icon_name, 48, 48, TRUE, NULL);
            } else {
                pixbuf = gtk_icon_theme_load_icon(icon_theme, icon_name, 48,
                                                  GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
            }
            g_free(icon_name);
        }
        g_object_unref(app_info);
    }

    if (pixbuf == NULL) {
        pixbuf = gtk_icon_theme_load_icon(icon_theme, normalized_app_id, 48,
                                          GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
    }

    if (pixbuf == NULL) {
        pixbuf = gtk_icon_theme_load_icon(icon_theme, app_id, 48,
                                          GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
    }

    if (pixbuf == NULL && desktop_file_path != NULL) {
        gchar *desktop_basename = g_path_get_basename(desktop_file_path);
        pixbuf = gtk_icon_theme_load_icon(icon_theme, desktop_basename, 48,
                                          GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
        g_free(desktop_basename);
    }

    if (pixbuf == NULL) {
        pixbuf = gtk_icon_theme_load_icon(icon_theme, "application-x-executable", 48,
                                          GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
    }

    g_free(desktop_file_path);
    g_free(normalized_app_id);

    return pixbuf;
}

void load_pinned_apps(void) {
    gchar *config_dir = g_build_filename(g_get_user_config_dir(), "vaxp/dock", NULL);
    gchar *config_file = g_build_filename(config_dir, "pinned-apps", NULL);
    
    g_mkdir_with_parents(config_dir, 0755);
    
    GError *error = NULL;
    gchar *contents = NULL;
    
    if (g_file_get_contents(config_file, &contents, NULL, &error)) {
        gchar **lines = g_strsplit(contents, "\n", -1);
        for (int i = 0; lines[i] != NULL; i++) {
            g_strstrip(lines[i]);
            if (strlen(lines[i]) > 0) {
                gchar *normalized = normalize_app_id(lines[i]);
                if (!pinned_app_contains(normalized)) {
                    pinned_apps = g_list_append(pinned_apps, normalized);
                } else {
                    g_free(normalized);
                }
            }
        }
        g_strfreev(lines);
        g_free(contents);
    }
    
    g_free(config_file);
    g_free(config_dir);
}

void save_pinned_apps(void) {
    gchar *config_dir = g_build_filename(g_get_user_config_dir(), "vaxp/dock", NULL);
    gchar *config_file = g_build_filename(config_dir, "pinned-apps", NULL);
    
    g_mkdir_with_parents(config_dir, 0755);
    
    GString *contents = g_string_new("");
    for (GList *l = pinned_apps; l != NULL; l = l->next) {
        g_string_append(contents, (gchar *)l->data);
        g_string_append_c(contents, '\n');
    }
    
    GError *error = NULL;
    if (!g_file_set_contents(config_file, contents->str, -1, &error)) {
        g_warning("Failed to save pinned apps: %s", error->message);
        g_error_free(error);
    }
    
    g_string_free(contents, TRUE);
    g_free(config_file);
    g_free(config_dir);
}
