#include "core/search_ctrl.h"
#include <gio/gio.h>
#include <string.h>

static GList *app_cache = NULL;

static AppCategory parse_category(const gchar *categories) {
    if (!categories) return CAT_OTHER;
    
    gchar *lower = g_ascii_strdown(categories, -1);
    AppCategory cat = CAT_OTHER;
    
    if (strstr(lower, "development") || strstr(lower, "ide") || 
        strstr(lower, "programming") || strstr(lower, "debugger")) {
        cat = CAT_DEVELOPMENT;
    } else if (strstr(lower, "system") || strstr(lower, "settings") || 
               strstr(lower, "hardware") || strstr(lower, "monitor")) {
        cat = CAT_SYSTEM;
    } else if (strstr(lower, "network") || strstr(lower, "internet") || 
               strstr(lower, "web") || strstr(lower, "browser") ||
               strstr(lower, "email") || strstr(lower, "chat")) {
        cat = CAT_INTERNET;
    } else if (strstr(lower, "utility") || strstr(lower, "accessories") ||
               strstr(lower, "texteditor") || strstr(lower, "filemanager") ||
               strstr(lower, "archiving") || strstr(lower, "calculator")) {
        cat = CAT_UTILITY;
    }
    
    g_free(lower);
    return cat;
}

static void free_app_entry(gpointer data) {
    AppEntry *app = (AppEntry *)data;
    if (app) {
        g_free(app->name);
        g_free(app->exec);
        g_free(app->icon);
        g_free(app->desktop_file);
        g_free(app);
    }
}

static gint compare_apps(gconstpointer a, gconstpointer b) {
    const AppEntry *app_a = (const AppEntry *)a;
    const AppEntry *app_b = (const AppEntry *)b;
    return g_utf8_collate(app_a->name, app_b->name);
}

void search_ctrl_init(void) {
    if (app_cache) return;
    
    const gchar *dirs[] = {
        "/usr/share/applications",
        "/usr/local/share/applications",
        g_build_filename(g_get_home_dir(), ".local/share/applications", NULL),
        NULL
    };
    
    for (int d = 0; dirs[d] != NULL; d++) {
        GDir *dir = g_dir_open(dirs[d], 0, NULL);
        if (!dir) continue;
        
        const gchar *filename;
        while ((filename = g_dir_read_name(dir)) != NULL) {
            if (!g_str_has_suffix(filename, ".desktop")) continue;
            
            gchar *filepath = g_build_filename(dirs[d], filename, NULL);
            GKeyFile *kf = g_key_file_new();
            
            if (g_key_file_load_from_file(kf, filepath, G_KEY_FILE_NONE, NULL)) {
                gboolean no_display = g_key_file_get_boolean(kf, "Desktop Entry", "NoDisplay", NULL);
                gboolean hidden = g_key_file_get_boolean(kf, "Desktop Entry", "Hidden", NULL);
                
                if (!no_display && !hidden) {
                    gchar *name = g_key_file_get_locale_string(kf, "Desktop Entry", "Name", NULL, NULL);
                    if (name) {
                        gboolean exists = FALSE;
                        for (GList *l = app_cache; l != NULL; l = l->next) {
                            AppEntry *existing = (AppEntry *)l->data;
                            if (g_strcmp0(existing->name, name) == 0) {
                                exists = TRUE;
                                break;
                            }
                        }
                        
                        if (!exists) {
                            AppEntry *app = g_new0(AppEntry, 1);
                            app->name = name;
                            app->exec = g_key_file_get_string(kf, "Desktop Entry", "Exec", NULL);
                            app->icon = g_key_file_get_string(kf, "Desktop Entry", "Icon", NULL);
                            app->desktop_file = g_strdup(filepath);
                            
                            gchar *cats = g_key_file_get_string(kf, "Desktop Entry", "Categories", NULL);
                            app->category = parse_category(cats);
                            g_free(cats);
                            
                            app_cache = g_list_append(app_cache, app);
                        } else {
                            g_free(name);
                        }
                    }
                }
            }
            
            g_key_file_free(kf);
            g_free(filepath);
        }
        g_dir_close(dir);
    }
    
    app_cache = g_list_sort(app_cache, compare_apps);
    g_print("🔍 MVC: Loaded %d applications into core cache.\n", g_list_length(app_cache));
}

void search_ctrl_cleanup(void) {
    g_list_free_full(app_cache, free_app_entry);
    app_cache = NULL;
}

GList* search_ctrl_get_filtered_apps(AppCategory category, const gchar *query) {
    GList *results = NULL;
    gchar *lower_query = (query && query[0] != '\0') ? g_utf8_strdown(query, -1) : NULL;

    for (GList *l = app_cache; l != NULL; l = l->next) {
        AppEntry *app = (AppEntry *)l->data;

        // Filter by category
        if (category != CAT_ALL && app->category != category) {
            continue;
        }

        // Filter by query
        if (lower_query) {
            gchar *lower_name = g_utf8_strdown(app->name, -1);
            gboolean match = strstr(lower_name, lower_query) != NULL;
            g_free(lower_name);
            if (!match) continue;
        }

        // Add to results
        results = g_list_append(results, app);
    }

    g_free(lower_query);
    return results;
}
