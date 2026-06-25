#ifndef VAXP_AUTH_CONFIG_H
#define VAXP_AUTH_CONFIG_H

typedef enum {
    THEME_MINIMAL,
    THEME_POLKIT,
    THEME_TERMINAL
} AuthTheme;

typedef struct {
    char bg[32];
    char text[32];
    char input_bg[32];
    char input_text[32];
    char input_focus[32];
    char btn_primary[32];
    char btn_primary_hover[32];
    char btn_secondary[32];
    char btn_secondary_hover[32];
    char ring[32];
} MinimalColors;

typedef struct {
    char bg[32];
    char border[32];
    char text[32];
    char accent[32];
    char accent_hover[32];
    char user_row_bg[32];
    char input_bg[32];
    char input_text[32];
    char btn_bg[32];
    char btn_hover[32];
} PolkitColors;

typedef struct {
    char bg[32];
    char border[32];
    char bar[32];
    char text[32];
    char prompt[32];
    char input_bg[32];
    char input_text[32];
    char hint[32];
    char warn[32];
    char cmd[32];
    char path[32];
    char dot_red[32];
    char dot_yellow[32];
    char dot_green[32];
} TerminalColors;

typedef struct {
    AuthTheme theme;
    MinimalColors minimal;
    PolkitColors polkit;
    TerminalColors terminal;
} AuthConfig;

// Loads the configuration from ~/.config/vaxp/auth/auth.vaxp
// Falls back to THEME_POLKIT if not found or invalid
void config_load(AuthConfig *config);

#endif // VAXP_AUTH_CONFIG_H
