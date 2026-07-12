#include "core/cmd_ctrl.h"
#include <stdio.h>
#include <string.h>
#include <gio/gio.h>

static gchar* execute_sync(const gchar *cmd) {
    FILE *fp = popen(cmd, "r");
    if (!fp) return g_strdup("Error");
    
    GString *output = g_string_new("");
    char buf[1024];
    while (fgets(buf, sizeof(buf), fp)) {
        g_string_append(output, buf);
    }
    pclose(fp);
    return g_string_free(output, FALSE);
}

CmdType cmd_ctrl_parse(const gchar *query, gchar **out_payload) {
    if (!query) return CMD_TYPE_UNKNOWN;

    if (g_str_has_prefix(query, "vater:")) {
        *out_payload = g_strdup(query + 6);
        return CMD_TYPE_VATER;
    } else if (g_str_has_prefix(query, "!=") || g_str_has_prefix(query, "!:")) {
        *out_payload = g_strdup(query + 2);
        return CMD_TYPE_MATH;
    } else if (g_str_has_prefix(query, "vafile:")) {
        *out_payload = g_strdup(query + 7);
        return CMD_TYPE_FILE;
    } else if (g_str_has_prefix(query, "g:")) {
        *out_payload = g_strdup(query + 2);
        return CMD_TYPE_WEB_GOOGLE;
    } else if (g_str_has_prefix(query, "s:")) { // Just fallback
        *out_payload = g_strdup(query + 2);
        return CMD_TYPE_WEB_GOOGLE;
    } else if (g_str_has_prefix(query, "gh:")) {
        *out_payload = g_strdup(query + 3);
        return CMD_TYPE_WEB_GITHUB;
    } else if (g_str_has_prefix(query, "y:")) {
        *out_payload = g_strdup(query + 2);
        return CMD_TYPE_WEB_YOUTUBE;
    } else if (g_str_has_prefix(query, "ai:")) {
        *out_payload = g_strdup(query + 3);
        return CMD_TYPE_AI;
    }
    
    *out_payload = g_strdup(query);
    return CMD_TYPE_UNKNOWN;
}

gchar* cmd_ctrl_exec_math(const gchar *expr) {
    if (!expr || strlen(expr) == 0) return g_strdup("");
    gchar *cmd = g_strdup_printf("echo \"%s\" | bc -l 2>&1", expr);
    gchar *result = execute_sync(cmd);
    g_strstrip(result);
    g_free(cmd);
    return result;
}

GList* cmd_ctrl_exec_file_search(const gchar *term) {
    if (!term || strlen(term) == 0) return NULL;
    
    gchar *cmd = g_strdup_printf("find ~ -iname \"*%s*\" -type f 2>/dev/null | head -20", term);
    gchar *output = execute_sync(cmd);
    g_free(cmd);
    
    GList *results = NULL;
    gchar **lines = g_strsplit(output, "\n", -1);
    for (int i = 0; lines[i] && strlen(lines[i]) > 0; i++) {
        results = g_list_append(results, g_strdup(lines[i]));
    }
    g_strfreev(lines);
    g_free(output);
    return results;
}

void cmd_ctrl_exec_vater(const gchar *cmd) {
    if (!cmd || strlen(cmd) == 0) return;
    
    gchar *vater_path = g_find_program_in_path("vater");
    gchar *term_cmd = NULL;

    if (vater_path) {
        term_cmd = g_strdup_printf("%s -e bash -c '%s; echo; echo Press Enter to close...; read'", vater_path, cmd);
        g_free(vater_path);
    } else {
        term_cmd = g_strdup_printf("x-terminal-emulator -e bash -c '%s; echo; echo Press Enter to close...; read'", cmd);
    }
    
    g_spawn_command_line_async(term_cmd, NULL);
    g_free(term_cmd);
}
