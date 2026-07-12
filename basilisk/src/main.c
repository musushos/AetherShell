#include <gtk/gtk.h>
#include <gio/gio.h>
#include "core/search_ctrl.h"
#include "core/cmd_ctrl.h"
#include "ui/view_spot.h"
#include "ui/view_grid.h"
#include "ui/view_ai.h"
#include "ui/view_dialogs.h"

#define DBUS_NAME "org.vaxp.Basilisk"
#define DBUS_PATH "/org/vaxp/Basilisk"
#define DBUS_INTERFACE "org.vaxp.Basilisk"

static const gchar introspection_xml[] =
    "<node>"
    "  <interface name='org.vaxp.Basilisk'>"
    "    <method name='Show'/>"
    "    <method name='Hide'/>"
    "    <method name='Toggle'/>"
    "    <method name='Search'>"
    "      <arg type='s' name='query' direction='in'/>"
    "    </method>"
    "  </interface>"
    "</node>";

// Global Orchestrator Function
void cmd_ctrl_execute_all(const gchar *query) {
    if (!query || query[0] == '\0') return;
    
    gchar *payload = NULL;
    CmdType type = cmd_ctrl_parse(query, &payload);
    
    switch (type) {
        case CMD_TYPE_MATH: {
            gchar *result = cmd_ctrl_exec_math(payload);
            GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
            gtk_clipboard_set_text(clipboard, result, -1);
            g_free(result);
            break;
        }
        case CMD_TYPE_FILE: {
            GList *results = cmd_ctrl_exec_file_search(payload);
            if (results && results->data) {
                gchar *path = (gchar *)results->data;
                gchar *uri = g_filename_to_uri(path, NULL, NULL);
                if (uri) {
                    g_app_info_launch_default_for_uri(uri, NULL, NULL);
                    g_free(uri);
                }
                g_list_free_full(results, g_free);
            }
            break;
        }
        case CMD_TYPE_VATER:
            cmd_ctrl_exec_vater(payload);
            break;
        case CMD_TYPE_WEB_GOOGLE: {
            gchar *encoded = g_uri_escape_string(payload, NULL, TRUE);
            gchar *url = g_strdup_printf("https://www.google.com/search?q=%s", encoded);
            g_app_info_launch_default_for_uri(url, NULL, NULL);
            g_free(url);
            g_free(encoded);
            break;
        }
        case CMD_TYPE_WEB_GITHUB: {
            gchar *encoded = g_uri_escape_string(payload, NULL, TRUE);
            gchar *url = g_strdup_printf("https://github.com/search?q=%s", encoded);
            g_app_info_launch_default_for_uri(url, NULL, NULL);
            g_free(url);
            g_free(encoded);
            break;
        }
        case CMD_TYPE_WEB_YOUTUBE: {
            gchar *encoded = g_uri_escape_string(payload, NULL, TRUE);
            gchar *url = g_strdup_printf("https://www.youtube.com/results?search_query=%s", encoded);
            g_app_info_launch_default_for_uri(url, NULL, NULL);
            g_free(url);
            g_free(encoded);
            break;
        }
        case CMD_TYPE_AI:
            view_ai_show(payload);
            break;
        case CMD_TYPE_UNKNOWN:
        default:
            if (g_str_has_prefix(query, "app:")) {
                view_grid_show_with_search(query + 4);
            } else {
                view_grid_show_with_search(payload);
            }
            break;
    }
    g_free(payload);
}

static void handle_dbus_method(GDBusConnection *conn, const gchar *sender,
                               const gchar *obj_path, const gchar *iface,
                               const gchar *method, GVariant *params,
                               GDBusMethodInvocation *invocation, gpointer data) {
    (void)conn; (void)sender; (void)obj_path; (void)iface; (void)data;
    
    if (g_strcmp0(method, "Show") == 0) {
        view_spot_show();
        g_dbus_method_invocation_return_value(invocation, NULL);
    } else if (g_strcmp0(method, "Hide") == 0) {
        view_spot_hide();
        view_grid_hide();
        g_dbus_method_invocation_return_value(invocation, NULL);
    } else if (g_strcmp0(method, "Toggle") == 0) {
        view_spot_toggle();
        g_dbus_method_invocation_return_value(invocation, NULL);
    } else if (g_strcmp0(method, "Search") == 0) {
        const gchar *query;
        g_variant_get(params, "(&s)", &query);
        cmd_ctrl_execute_all(query);
        g_dbus_method_invocation_return_value(invocation, NULL);
    }
}

static void on_bus_acquired(GDBusConnection *conn, const gchar *name, gpointer data) {
    (void)name; (void)data;
    GDBusNodeInfo *node_info = g_dbus_node_info_new_for_xml(introspection_xml, NULL);
    
    static const GDBusInterfaceVTable vtable = {
        handle_dbus_method, NULL, NULL, {0}
    };
    
    g_dbus_connection_register_object(conn, DBUS_PATH, node_info->interfaces[0],
                                      &vtable, NULL, NULL, NULL);
    g_dbus_node_info_unref(node_info);
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    search_ctrl_init();
    view_spot_init();
    view_grid_init();

    g_bus_own_name(G_BUS_TYPE_SESSION, DBUS_NAME,
                   G_BUS_NAME_OWNER_FLAGS_NONE,
                   on_bus_acquired, NULL, NULL, NULL, NULL);

    gtk_main();

    search_ctrl_cleanup();
    return 0;
}
