#include "weather.h"
#include "aetherlock.h"
#include <gio/gio.h>
#include <string.h>
#include <unistd.h>

static void on_weather_fetch_done(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    struct aetherlock_state *state = user_data;
    GSubprocess *subprocess = G_SUBPROCESS(source_object);
    GError *error = NULL;
    gchar *stdout_buf = NULL;
    
    if (g_subprocess_communicate_utf8_finish(subprocess, res, &stdout_buf, NULL, &error)) {
        if (stdout_buf) {
            char **lines = g_strsplit(stdout_buf, "\n", 4);
            if (lines && lines[0] && lines[1] && lines[2]) {
                // Get the first part of the location string (before the comma)
                char *comma = strchr(lines[0], ',');
                if (comma) {
                    *comma = '\0';
                }
                g_strlcpy(state->weather.location, lines[0], sizeof(state->weather.location));
                g_strlcpy(state->weather.condition, lines[1], sizeof(state->weather.condition));
                g_strlcpy(state->weather.temperature, lines[2], sizeof(state->weather.temperature));
                state->weather_fetched = true;
                damage_state(state);
            }
            g_strfreev(lines);
            g_free(stdout_buf);
        }
    } else {
        g_error_free(error);
    }
    
    g_object_unref(subprocess);
}

void weather_fetch(struct aetherlock_state *state) {
    GError *error = NULL;
    
    // Check for custom location config
    gchar *config_dir = g_build_filename(g_get_user_config_dir(), "vaxp", "aetherlock", NULL);
    gchar *config_file = g_build_filename(config_dir, "weather.vaxp", NULL);
    
    gchar *custom_location = NULL;
    
    if (g_file_test(config_file, G_FILE_TEST_EXISTS)) {
        gchar *content = NULL;
        if (g_file_get_contents(config_file, &content, NULL, NULL)) {
            char **lines = g_strsplit(content, "\n", -1);
            for (int i = 0; lines && lines[i]; i++) {
                gchar *line = g_strstrip(lines[i]);
                if (strlen(line) > 0 && line[0] != '#') {
                    custom_location = g_strdup(line);
                    break;
                }
            }
            g_strfreev(lines);
            g_free(content);
        }
    } else {
        // Create the file with default content
        g_mkdir_with_parents(config_dir, 0755);
        
        // Try to get default location from timezone
        gchar *default_loc = NULL;
        char tz_buf[512] = {0};
        ssize_t len = readlink("/etc/localtime", tz_buf, sizeof(tz_buf)-1);
        if (len != -1) {
            char *city = strrchr(tz_buf, '/');
            if (city) {
                // Replace underscores with spaces (e.g. New_York -> New York)
                default_loc = g_strdup(city + 1);
                g_strdelimit(default_loc, "_", ' ');
            }
        }
        
        gchar *default_content;
        if (default_loc) {
            default_content = g_strdup_printf("# Enter your city here (e.g., Karbala)\n%s\n", default_loc);
            custom_location = g_strdup(default_loc); // Use it immediately as well!
            g_free(default_loc);
        } else {
            default_content = g_strdup("# Enter your city here (e.g., Karbala)\n# Leave empty for auto-detect based on IP.\n\n");
        }
        g_file_set_contents(config_file, default_content, -1, NULL);
        g_free(default_content);
    }
    
    gchar *url;
    if (custom_location && strlen(custom_location) > 0) {
        // g_uri_escape_string to handle spaces in city names
        gchar *escaped_loc = g_uri_escape_string(custom_location, NULL, TRUE);
        url = g_strdup_printf("wttr.in/%s?format=%%l\\n%%C\\n%%t", escaped_loc);
        g_free(escaped_loc);
    } else {
        url = g_strdup("wttr.in/?format=%l\\n%C\\n%t");
    }
    
    GSubprocess *subprocess = g_subprocess_new(
        G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE,
        &error,
        "curl", "-s", url, NULL
    );
    
    g_free(url);
    g_free(custom_location);
    g_free(config_file);
    g_free(config_dir);
    
    if (error) {
        g_error_free(error);
        return;
    }
    
    g_subprocess_communicate_utf8_async(subprocess, NULL, NULL, on_weather_fetch_done, state);
}

void weather_init(struct aetherlock_state *state) {
    state->weather_fetched = false;
    g_strlcpy(state->weather.location, "Loading...", sizeof(state->weather.location));
    g_strlcpy(state->weather.condition, "Loading...", sizeof(state->weather.condition));
    g_strlcpy(state->weather.temperature, "", sizeof(state->weather.temperature));
    weather_fetch(state);
}
