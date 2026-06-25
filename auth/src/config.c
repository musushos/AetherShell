#include "config.h"
#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void get_color(GKeyFile *kf, const char *group, const char *key, char *out, size_t out_len) {
    gchar *val = g_key_file_get_string(kf, group, key, NULL);
    if (val) {
        g_strlcpy(out, val, out_len);
        g_free(val);
    }
}

void config_load(AuthConfig *config) {
    // Set general defaults
    config->theme = THEME_POLKIT;
    
    // Set minimal defaults
    g_strlcpy(config->minimal.bg, "rgba(0, 0, 0, 0.3)", sizeof(config->minimal.bg));
    g_strlcpy(config->minimal.text, "rgba(255, 255, 255, 1)", sizeof(config->minimal.text));
    g_strlcpy(config->minimal.input_bg, "rgba(0, 0, 0, 0.3)", sizeof(config->minimal.input_bg));
    g_strlcpy(config->minimal.input_text, "rgba(255, 255, 255, 1.0)", sizeof(config->minimal.input_text));
    g_strlcpy(config->minimal.input_focus, "rgba(21, 23, 28, 1.0)", sizeof(config->minimal.input_focus));
    g_strlcpy(config->minimal.btn_primary, "rgba(0, 0, 0, 0.3)", sizeof(config->minimal.btn_primary));
    g_strlcpy(config->minimal.btn_primary_hover, "rgba(0, 0, 0, 1.0)", sizeof(config->minimal.btn_primary_hover));
    g_strlcpy(config->minimal.btn_secondary, "rgba(241, 242, 245, 1.0)", sizeof(config->minimal.btn_secondary));
    g_strlcpy(config->minimal.btn_secondary_hover, "rgba(232, 234, 239, 1.0)", sizeof(config->minimal.btn_secondary_hover));
    g_strlcpy(config->minimal.ring, "rgba(21, 23, 28, 1.0)", sizeof(config->minimal.ring));

    // Set polkit defaults
    g_strlcpy(config->polkit.bg, "rgba(0, 0, 0, 0.3)", sizeof(config->polkit.bg));
    g_strlcpy(config->polkit.border, "rgba(0, 0, 0, 0.4)", sizeof(config->polkit.border));
    g_strlcpy(config->polkit.text, "rgba(255, 255, 255, 1.0)", sizeof(config->polkit.text));
    g_strlcpy(config->polkit.accent, "rgba(21, 23, 28, 1.0)", sizeof(config->polkit.accent));
    g_strlcpy(config->polkit.accent_hover, "rgba(0, 0, 0, 1.0)", sizeof(config->polkit.accent_hover));
    g_strlcpy(config->polkit.user_row_bg, "rgba(0, 0, 0, 0.3)", sizeof(config->polkit.user_row_bg));
    g_strlcpy(config->polkit.input_bg, "rgba(0, 0, 0, 0.3)", sizeof(config->polkit.input_bg));
    g_strlcpy(config->polkit.input_text, "rgba(255, 255, 255, 1.0)", sizeof(config->polkit.input_text));
    g_strlcpy(config->polkit.btn_bg, "rgba(241, 242, 245, 1.0)", sizeof(config->polkit.btn_bg));
    g_strlcpy(config->polkit.btn_hover, "rgba(232, 234, 239, 1.0)", sizeof(config->polkit.btn_hover));

    // Set terminal defaults
    g_strlcpy(config->terminal.bg, "rgba(0, 0, 0, 0.3)", sizeof(config->terminal.bg));
    g_strlcpy(config->terminal.border, "rgba(0, 0, 0, 0.4)", sizeof(config->terminal.border));
    g_strlcpy(config->terminal.bar, "rgba(0, 0, 0, 0)", sizeof(config->terminal.bar));
    g_strlcpy(config->terminal.text, "rgba(0, 0, 0, 0.4)", sizeof(config->terminal.text));
    g_strlcpy(config->terminal.prompt, "rgba(88, 224, 140, 1.0)", sizeof(config->terminal.prompt));
    g_strlcpy(config->terminal.input_bg, "rgba(0, 0, 0, 0.4)", sizeof(config->terminal.input_bg));
    g_strlcpy(config->terminal.input_text, "rgba(255, 255, 255, 1.0)", sizeof(config->terminal.input_text));
    g_strlcpy(config->terminal.hint, "rgba(90, 96, 110, 1.0)", sizeof(config->terminal.hint));
    g_strlcpy(config->terminal.warn, "rgba(255, 180, 84, 1.0)", sizeof(config->terminal.warn));
    g_strlcpy(config->terminal.cmd, "rgba(232, 233, 236, 1.0)", sizeof(config->terminal.cmd));
    g_strlcpy(config->terminal.path, "rgba(255, 255, 255, 0.91)", sizeof(config->terminal.path));
    g_strlcpy(config->terminal.dot_red, "rgba(255, 95, 87, 1.0)", sizeof(config->terminal.dot_red));
    g_strlcpy(config->terminal.dot_yellow, "rgba(255, 189, 46, 1.0)", sizeof(config->terminal.dot_yellow));
    g_strlcpy(config->terminal.dot_green, "rgba(40, 200, 64, 1.0)", sizeof(config->terminal.dot_green));

    const char *home = g_getenv("HOME");
    if (!home) return;

    gchar *config_path = g_build_filename(home, ".config", "vaxp", "auth", "auth.vaxp", NULL);
    GKeyFile *key_file = g_key_file_new();
    GError *error = NULL;

    if (!g_file_test(config_path, G_FILE_TEST_EXISTS)) {
        gchar *config_dir = g_path_get_dirname(config_path);
        g_mkdir_with_parents(config_dir, 0755);
        g_free(config_dir);

        FILE *f = fopen(config_path, "w");
        if (f) {
            fprintf(f, "[General]\nTheme=polkit\n\n");
            fprintf(f, "[Theme.Minimal]\n");
            fprintf(f, "BackgroundColor=%s\n", config->minimal.bg);
            fprintf(f, "TextColor=%s\n", config->minimal.text);
            fprintf(f, "InputBackground=%s\n", config->minimal.input_bg);
            fprintf(f, "InputTextColor=%s\n", config->minimal.input_text);
            fprintf(f, "InputFocusBorder=%s\n", config->minimal.input_focus);
            fprintf(f, "PrimaryButton=%s\n", config->minimal.btn_primary);
            fprintf(f, "PrimaryButtonHover=%s\n", config->minimal.btn_primary_hover);
            fprintf(f, "SecondaryButton=%s\n", config->minimal.btn_secondary);
            fprintf(f, "SecondaryButtonHover=%s\n", config->minimal.btn_secondary_hover);
            fprintf(f, "AvatarRing=%s\n\n", config->minimal.ring);

            fprintf(f, "[Theme.Polkit]\n");
            fprintf(f, "BackgroundColor=%s\n", config->polkit.bg);
            fprintf(f, "BorderColor=%s\n", config->polkit.border);
            fprintf(f, "TextColor=%s\n", config->polkit.text);
            fprintf(f, "AccentColor=%s\n", config->polkit.accent);
            fprintf(f, "AccentColorHover=%s\n", config->polkit.accent_hover);
            fprintf(f, "UserRowBackground=%s\n", config->polkit.user_row_bg);
            fprintf(f, "InputBackground=%s\n", config->polkit.input_bg);
            fprintf(f, "InputTextColor=%s\n", config->polkit.input_text);
            fprintf(f, "ButtonBackground=%s\n", config->polkit.btn_bg);
            fprintf(f, "ButtonHover=%s\n\n", config->polkit.btn_hover);

            fprintf(f, "[Theme.Terminal]\n");
            fprintf(f, "BackgroundColor=%s\n", config->terminal.bg);
            fprintf(f, "BorderColor=%s\n", config->terminal.border);
            fprintf(f, "TitleBarColor=%s\n", config->terminal.bar);
            fprintf(f, "TextColor=%s\n", config->terminal.text);
            fprintf(f, "PromptColor=%s\n", config->terminal.prompt);
            fprintf(f, "InputBackground=%s\n", config->terminal.input_bg);
            fprintf(f, "InputTextColor=%s\n", config->terminal.input_text);
            fprintf(f, "HintColor=%s\n", config->terminal.hint);
            fprintf(f, "WarningColor=%s\n", config->terminal.warn);
            fprintf(f, "CommandColor=%s\n", config->terminal.cmd);
            fprintf(f, "PathColor=%s\n", config->terminal.path);
            fprintf(f, "DotRed=%s\n", config->terminal.dot_red);
            fprintf(f, "DotYellow=%s\n", config->terminal.dot_yellow);
            fprintf(f, "DotGreen=%s\n", config->terminal.dot_green);
            fclose(f);
        }
    }

    if (g_key_file_load_from_file(key_file, config_path, G_KEY_FILE_NONE, &error)) {
        gchar *theme_str = g_key_file_get_string(key_file, "General", "Theme", NULL);
        if (theme_str) {
            if (g_ascii_strcasecmp(theme_str, "minimal") == 0) config->theme = THEME_MINIMAL;
            else if (g_ascii_strcasecmp(theme_str, "terminal") == 0) config->theme = THEME_TERMINAL;
            else if (g_ascii_strcasecmp(theme_str, "polkit") == 0) config->theme = THEME_POLKIT;
            g_free(theme_str);
        }

        // Minimal
        get_color(key_file, "Theme.Minimal", "BackgroundColor", config->minimal.bg, sizeof(config->minimal.bg));
        get_color(key_file, "Theme.Minimal", "TextColor", config->minimal.text, sizeof(config->minimal.text));
        get_color(key_file, "Theme.Minimal", "InputBackground", config->minimal.input_bg, sizeof(config->minimal.input_bg));
        get_color(key_file, "Theme.Minimal", "InputTextColor", config->minimal.input_text, sizeof(config->minimal.input_text));
        get_color(key_file, "Theme.Minimal", "InputFocusBorder", config->minimal.input_focus, sizeof(config->minimal.input_focus));
        get_color(key_file, "Theme.Minimal", "PrimaryButton", config->minimal.btn_primary, sizeof(config->minimal.btn_primary));
        get_color(key_file, "Theme.Minimal", "PrimaryButtonHover", config->minimal.btn_primary_hover, sizeof(config->minimal.btn_primary_hover));
        get_color(key_file, "Theme.Minimal", "SecondaryButton", config->minimal.btn_secondary, sizeof(config->minimal.btn_secondary));
        get_color(key_file, "Theme.Minimal", "SecondaryButtonHover", config->minimal.btn_secondary_hover, sizeof(config->minimal.btn_secondary_hover));
        get_color(key_file, "Theme.Minimal", "AvatarRing", config->minimal.ring, sizeof(config->minimal.ring));

        // Polkit
        get_color(key_file, "Theme.Polkit", "BackgroundColor", config->polkit.bg, sizeof(config->polkit.bg));
        get_color(key_file, "Theme.Polkit", "BorderColor", config->polkit.border, sizeof(config->polkit.border));
        get_color(key_file, "Theme.Polkit", "TextColor", config->polkit.text, sizeof(config->polkit.text));
        get_color(key_file, "Theme.Polkit", "AccentColor", config->polkit.accent, sizeof(config->polkit.accent));
        get_color(key_file, "Theme.Polkit", "AccentColorHover", config->polkit.accent_hover, sizeof(config->polkit.accent_hover));
        get_color(key_file, "Theme.Polkit", "UserRowBackground", config->polkit.user_row_bg, sizeof(config->polkit.user_row_bg));
        get_color(key_file, "Theme.Polkit", "InputBackground", config->polkit.input_bg, sizeof(config->polkit.input_bg));
        get_color(key_file, "Theme.Polkit", "InputTextColor", config->polkit.input_text, sizeof(config->polkit.input_text));
        get_color(key_file, "Theme.Polkit", "ButtonBackground", config->polkit.btn_bg, sizeof(config->polkit.btn_bg));
        get_color(key_file, "Theme.Polkit", "ButtonHover", config->polkit.btn_hover, sizeof(config->polkit.btn_hover));

        // Terminal
        get_color(key_file, "Theme.Terminal", "BackgroundColor", config->terminal.bg, sizeof(config->terminal.bg));
        get_color(key_file, "Theme.Terminal", "BorderColor", config->terminal.border, sizeof(config->terminal.border));
        get_color(key_file, "Theme.Terminal", "TitleBarColor", config->terminal.bar, sizeof(config->terminal.bar));
        get_color(key_file, "Theme.Terminal", "TextColor", config->terminal.text, sizeof(config->terminal.text));
        get_color(key_file, "Theme.Terminal", "PromptColor", config->terminal.prompt, sizeof(config->terminal.prompt));
        get_color(key_file, "Theme.Terminal", "InputBackground", config->terminal.input_bg, sizeof(config->terminal.input_bg));
        get_color(key_file, "Theme.Terminal", "InputTextColor", config->terminal.input_text, sizeof(config->terminal.input_text));
        get_color(key_file, "Theme.Terminal", "HintColor", config->terminal.hint, sizeof(config->terminal.hint));
        get_color(key_file, "Theme.Terminal", "WarningColor", config->terminal.warn, sizeof(config->terminal.warn));
        get_color(key_file, "Theme.Terminal", "CommandColor", config->terminal.cmd, sizeof(config->terminal.cmd));
        get_color(key_file, "Theme.Terminal", "PathColor", config->terminal.path, sizeof(config->terminal.path));
        get_color(key_file, "Theme.Terminal", "DotRed", config->terminal.dot_red, sizeof(config->terminal.dot_red));
        get_color(key_file, "Theme.Terminal", "DotYellow", config->terminal.dot_yellow, sizeof(config->terminal.dot_yellow));
        get_color(key_file, "Theme.Terminal", "DotGreen", config->terminal.dot_green, sizeof(config->terminal.dot_green));
    }

    g_key_file_free(key_file);
    g_free(config_path);
}
