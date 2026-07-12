#ifndef CORE_CMD_CTRL_H
#define CORE_CMD_CTRL_H

#include <glib.h>

// Types of commands
typedef enum {
    CMD_TYPE_MATH,
    CMD_TYPE_FILE,
    CMD_TYPE_VATER,
    CMD_TYPE_WEB_GITHUB,
    CMD_TYPE_WEB_GOOGLE,
    CMD_TYPE_WEB_YOUTUBE,
    CMD_TYPE_AI,
    CMD_TYPE_UNKNOWN
} CmdType;

// Parse the query prefix and return the type and the cleaned payload
CmdType cmd_ctrl_parse(const gchar *query, gchar **out_payload);

// Execute a math command and return the calculated string. Must be freed by caller.
gchar* cmd_ctrl_exec_math(const gchar *expr);

// Execute a file search and return a list of paths (strings). Must be freed with g_list_free_full(list, g_free).
GList* cmd_ctrl_exec_file_search(const gchar *term);

// Execute a terminal command
void cmd_ctrl_exec_vater(const gchar *cmd);

#endif
