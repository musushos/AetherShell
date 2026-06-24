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
    
    gchar *url;
    if (state->weather.custom_location && strlen(state->weather.custom_location) > 0) {
        // g_uri_escape_string to handle spaces in city names
        gchar *escaped_loc = g_uri_escape_string(state->weather.custom_location, NULL, TRUE);
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
