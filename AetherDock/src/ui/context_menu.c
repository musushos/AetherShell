#include "ui/context_menu.h"
#include "ui/app_tracker.h"
#include "ui/dock_ui.h"

typedef struct {
    WindowGroup *group;
    void (*action)(GtkWidget *, gpointer);
} ContextMenuActionData;

static void free_context_menu_action_data(gpointer data, GClosure *closure) {
    (void)closure;
    g_free(data);
}

static void close_context_menu(void) {
    if (context_menu_grab_seat) {
        gdk_seat_ungrab(context_menu_grab_seat);
        context_menu_grab_seat = NULL;
    }

    if (context_menu_window) {
        gtk_widget_destroy(context_menu_window);
        context_menu_window = NULL;
    }
}

static gboolean on_context_menu_focus_out(GtkWidget *widget, GdkEventFocus *event, gpointer data) {
    (void)widget;
    (void)event;
    (void)data;
    close_context_menu();
    return FALSE;
}

static gboolean on_context_menu_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data) {
    (void)widget;
    (void)data;
    if (event->keyval == GDK_KEY_Escape) {
        close_context_menu();
        return TRUE;
    }
    return FALSE;
}

static void on_context_menu_window_active_notify(GObject *object, GParamSpec *pspec, gpointer data) {
    (void)pspec;
    (void)data;

    if (!gtk_window_is_active(GTK_WINDOW(object))) {
        close_context_menu();
    }
}

static gboolean on_context_menu_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    GtkAllocation allocation;

    (void)data;
    gtk_widget_get_allocation(widget, &allocation);

    if (event->x < 0 || event->y < 0 ||
        event->x >= allocation.width || event->y >= allocation.height) {
        close_context_menu();
        return TRUE;
    }

    return FALSE;
}

static gboolean on_context_menu_grab_broken(GtkWidget *widget, GdkEventGrabBroken *event, gpointer data) {
    (void)widget;
    (void)event;
    (void)data;
    context_menu_grab_seat = NULL;
    close_context_menu();
    return FALSE;
}

static void on_context_menu_item_clicked(GtkWidget *widget, gpointer data) {
    ContextMenuActionData *action_data = (ContextMenuActionData *)data;
    close_context_menu();
    action_data->action(widget, action_data->group);
}

static GtkWidget *create_context_menu_item(const gchar *label, WindowGroup *group, void (*action)(GtkWidget *, gpointer)) {
    GtkWidget *button = gtk_button_new_with_label(label);
    ContextMenuActionData *action_data = g_new0(ContextMenuActionData, 1);

    gtk_widget_set_name(button, "context-menu-item");
    gtk_widget_set_halign(button, GTK_ALIGN_FILL);

    action_data->group = group;
    action_data->action = action;
    g_signal_connect_data(button,
                          "clicked",
                          G_CALLBACK(on_context_menu_item_clicked),
                          action_data,
                          free_context_menu_action_data,
                          0);

    return button;
}

static GtkWidget *create_context_menu_separator(void) {
    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_name(separator, "context-menu-separator");
    return separator;
}

static GtkWidget *create_wayland_menu_item(const gchar *label, WindowGroup *group, void (*action)(GtkWidget *, gpointer)) {
    GtkWidget *item = gtk_menu_item_new_with_label(label);
    ContextMenuActionData *action_data = g_new0(ContextMenuActionData, 1);
    GtkStyleContext *context = gtk_widget_get_style_context(item);

    gtk_widget_set_name(item, "dock-context-menu-item");
    gtk_style_context_add_class(context, "dock-context-menu-item");
    action_data->group = group;
    action_data->action = action;
    g_signal_connect_data(item,
                          "activate",
                          G_CALLBACK(on_context_menu_item_clicked),
                          action_data,
                          free_context_menu_action_data,
                          0);
    return item;
}

static void position_context_menu(GtkWidget *menu_window, GdkEventButton *event) {
    GtkRequisition min_req = {0};
    GtkRequisition nat_req = {0};
    GdkDisplay *display;
    GdkMonitor *monitor;
    GdkRectangle geometry = {0};
    gint menu_width;
    gint menu_height;
    gint x;
    gint y;
    const gint gap = 8;
    const gint margin = 8;

    gtk_widget_get_preferred_size(menu_window, &min_req, &nat_req);
    menu_width = nat_req.width > 0 ? nat_req.width : min_req.width;
    menu_height = nat_req.height > 0 ? nat_req.height : min_req.height;

    display = gtk_widget_get_display(menu_window);
    monitor = gdk_display_get_monitor_at_point(display, (gint)event->x_root, (gint)event->y_root);
    if (!monitor) {
        monitor = gdk_display_get_primary_monitor(display);
    }
    if (!monitor) {
        gtk_window_move(GTK_WINDOW(menu_window), (gint)event->x_root, (gint)event->y_root);
        return;
    }

    gdk_monitor_get_geometry(monitor, &geometry);

    x = (gint)event->x_root - (menu_width / 2);
    y = (gint)event->y_root - menu_height - gap;

    if (y < geometry.y + margin) {
        y = (gint)event->y_root + gap;
    }

    if (x + menu_width > geometry.x + geometry.width - margin) {
        x = geometry.x + geometry.width - menu_width - margin;
    }
    if (x < geometry.x + margin) {
        x = geometry.x + margin;
    }

    if (y + menu_height > geometry.y + geometry.height - margin) {
        y = geometry.y + geometry.height - menu_height - margin;
    }
    if (y < geometry.y + margin) {
        y = geometry.y + margin;
    }

    gtk_window_move(GTK_WINDOW(menu_window), x, y);
}

static void on_pin_clicked(GtkWidget *menuitem, gpointer data) {
    (void)menuitem;
    WindowGroup *group = (WindowGroup *)data;
    
    if (group->is_pinned) {
        /* Unpin */
        group->is_pinned = FALSE;
        
        /* Find and remove from pinned list */
        for (GList *l = pinned_apps; l != NULL; l = l->next) {
            if (app_id_equals((const gchar *)l->data, group->wm_class)) {
                g_free(l->data);
                pinned_apps = g_list_delete_link(pinned_apps, l);
                break;
            }
        }
    } else {
        /* Pin */
        group->is_pinned = TRUE;
        if (!pinned_app_contains(group->wm_class)) {
            pinned_apps = g_list_append(pinned_apps, normalize_app_id(group->wm_class));
        }
    }
    
    save_pinned_apps();
    update_window_list();
}

static void on_new_window_clicked(GtkWidget *menuitem, gpointer data) {
    (void)menuitem;
    WindowGroup *group = (WindowGroup *)data;
    
    if (group->desktop_file_path != NULL) {
        GDesktopAppInfo *app_info = g_desktop_app_info_new_from_filename(group->desktop_file_path);
        if (app_info != NULL) {
            GError *error = NULL;
            GdkAppLaunchContext *context = gdk_display_get_app_launch_context(gdk_display_get_default());
            
            if (!g_app_info_launch(G_APP_INFO(app_info), NULL, G_APP_LAUNCH_CONTEXT(context), &error)) {
                g_warning("Failed to launch app: %s", error->message);
                g_error_free(error);
            }
            
            g_object_unref(context);
            g_object_unref(app_info);
        }
    }
}

static void on_close_all_clicked(GtkWidget *menuitem, gpointer data) {
    (void)menuitem;
    WindowGroup *group = (WindowGroup *)data;
    
    for (GList *l = group->windows; l != NULL; l = l->next) {
        WaylandToplevel *item = (WaylandToplevel *)l->data;
        zwlr_foreign_toplevel_handle_v1_close(item->handle);
    }

    if (wl_display_conn) {
        wl_display_flush(wl_display_conn);
    }
}

static void on_run_with_gpu_clicked(GtkWidget *menuitem, gpointer data) {
    (void)menuitem;
    WindowGroup *group = (WindowGroup *)data;
    
    if (group->desktop_file_path != NULL) {
        GDesktopAppInfo *app_info = g_desktop_app_info_new_from_filename(group->desktop_file_path);
        if (app_info != NULL) {
            /* Get the Exec line from desktop file */
            gchar *exec = g_desktop_app_info_get_string(app_info, "Exec");
            
            if (exec != NULL) {
                /* Remove field codes like %U, %F, etc. */
                gchar *clean_exec = g_strdup(exec);
                gchar *percent = g_strstr_len(clean_exec, -1, " %");
                if (percent != NULL) {
                    *percent = '\0';
                }
                
                /* Build command with GPU environment variables (no & needed - async by default) */
                gchar *gpu_command = g_strdup_printf("env DRI_PRIME=1 __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia %s", clean_exec);
                
                GError *error = NULL;
                if (!g_spawn_command_line_async(gpu_command, &error)) {
                    g_warning("Failed to launch with GPU: %s", error->message);
                    g_error_free(error);
                }
                
                g_free(gpu_command);
                g_free(clean_exec);
                g_free(exec);
            }
            
            g_object_unref(app_info);
        }
    }
}

void show_context_menu(GtkWidget *widget, GdkEventButton *event, WindowGroup *group) {
    (void)widget;
    GtkWidget *menu_box;
    GtkWidget *frame_box;
    close_context_menu();

    if (is_wayland_session) {
        GtkWidget *menu = gtk_menu_new();
        GtkStyleContext *context = gtk_widget_get_style_context(menu);

        context_menu_window = menu;
        gtk_widget_set_name(menu, "dock-context-menu");
        gtk_widget_set_app_paintable(menu, TRUE);
        gtk_container_set_border_width(GTK_CONTAINER(menu), 0);
        gtk_menu_set_reserve_toggle_size(GTK_MENU(menu), FALSE);
        gtk_style_context_add_class(context, "dock-context-menu");
        gtk_menu_shell_append(GTK_MENU_SHELL(menu),
                              create_wayland_menu_item(group->is_pinned ? "Unpin from Dock" : "Pin to Dock",
                                                       group, on_pin_clicked));
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

        if (group->desktop_file_path != NULL) {
            gtk_menu_shell_append(GTK_MENU_SHELL(menu),
                                  create_wayland_menu_item("New Window", group, on_new_window_clicked));
        }

        if (group->windows != NULL && g_list_length(group->windows) > 0) {
            gchar *label = g_strdup_printf("Close All (%d)", g_list_length(group->windows));
            gtk_menu_shell_append(GTK_MENU_SHELL(menu),
                                  create_wayland_menu_item(label, group, on_close_all_clicked));
            g_free(label);
        }

        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

        if (group->desktop_file_path != NULL) {
            gtk_menu_shell_append(GTK_MENU_SHELL(menu),
                                  create_wayland_menu_item("Run with Dedicated GPU", group, on_run_with_gpu_clicked));
        }

        gtk_widget_show_all(menu);
        gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);
        return;
    }

    context_menu_window = gtk_window_new(GTK_WINDOW_POPUP);
    gtk_widget_set_name(context_menu_window, "context-menu-window");
    gtk_widget_set_app_paintable(context_menu_window, TRUE);
    gtk_window_set_decorated(GTK_WINDOW(context_menu_window), FALSE);
    gtk_window_set_resizable(GTK_WINDOW(context_menu_window), FALSE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(context_menu_window), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(context_menu_window), TRUE);
    gtk_window_set_type_hint(GTK_WINDOW(context_menu_window), GDK_WINDOW_TYPE_HINT_POPUP_MENU);
    gtk_window_set_accept_focus(GTK_WINDOW(context_menu_window), TRUE);
    gtk_window_set_focus_on_map(GTK_WINDOW(context_menu_window), TRUE);
    gtk_widget_add_events(context_menu_window,
                          GDK_BUTTON_PRESS_MASK |
                          GDK_BUTTON_RELEASE_MASK |
                          GDK_FOCUS_CHANGE_MASK);

    {
        GdkScreen *screen = gtk_widget_get_screen(context_menu_window);
        GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
        if (visual && gdk_screen_is_composited(screen)) {
            gtk_widget_set_visual(context_menu_window, visual);
        }
    }

    frame_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(frame_box, "context-menu-frame");
    gtk_container_add(GTK_CONTAINER(context_menu_window), frame_box);

    menu_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_name(menu_box, "context-menu-box");
    gtk_container_add(GTK_CONTAINER(frame_box), menu_box);

    gtk_box_pack_start(GTK_BOX(menu_box),
                       create_context_menu_item(group->is_pinned ? "Unpin from Dock" : "Pin to Dock",
                                                group, on_pin_clicked),
                       FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(menu_box), create_context_menu_separator(), FALSE, FALSE, 4);

    if (group->desktop_file_path != NULL) {
        gtk_box_pack_start(GTK_BOX(menu_box),
                           create_context_menu_item("New Window", group, on_new_window_clicked),
                           FALSE, FALSE, 0);
    }

    if (group->windows != NULL && g_list_length(group->windows) > 0) {
        gchar *label = g_strdup_printf("Close All (%d)", g_list_length(group->windows));
        gtk_box_pack_start(GTK_BOX(menu_box),
                           create_context_menu_item(label, group, on_close_all_clicked),
                           FALSE, FALSE, 0);
        g_free(label);
    }

    gtk_box_pack_start(GTK_BOX(menu_box), create_context_menu_separator(), FALSE, FALSE, 4);

    if (group->desktop_file_path != NULL) {
        gtk_box_pack_start(GTK_BOX(menu_box),
                           create_context_menu_item("Run with Dedicated GPU", group, on_run_with_gpu_clicked),
                           FALSE, FALSE, 0);
    }

    g_signal_connect(context_menu_window, "focus-out-event", G_CALLBACK(on_context_menu_focus_out), NULL);
    g_signal_connect(context_menu_window, "key-press-event", G_CALLBACK(on_context_menu_key_press), NULL);
    g_signal_connect(context_menu_window, "button-press-event", G_CALLBACK(on_context_menu_button_press), NULL);
    g_signal_connect(context_menu_window, "grab-broken-event", G_CALLBACK(on_context_menu_grab_broken), NULL);
    g_signal_connect(context_menu_window, "notify::is-active", G_CALLBACK(on_context_menu_window_active_notify), NULL);
    g_signal_connect(context_menu_window, "destroy", G_CALLBACK(gtk_widget_destroyed), &context_menu_window);

    gtk_widget_show_all(context_menu_window);
    position_context_menu(context_menu_window, event);
    gtk_window_present(GTK_WINDOW(context_menu_window));
    gtk_widget_grab_focus(context_menu_window);

    {
        GdkDisplay *display = gtk_widget_get_display(context_menu_window);
        GdkSeat *seat = gdk_display_get_default_seat(display);
        GdkWindow *window = gtk_widget_get_window(context_menu_window);
        GdkGrabStatus grab_status = GDK_GRAB_NOT_VIEWABLE;


        if (seat && window) {
            grab_status = gdk_seat_grab(seat,
                                        window,
                                        GDK_SEAT_CAPABILITY_ALL_POINTING | GDK_SEAT_CAPABILITY_KEYBOARD,
                                        TRUE,
                                        NULL,
                                        (GdkEvent *)event,
                                        NULL,
                                        NULL);
        }

        if (grab_status == GDK_GRAB_SUCCESS) {
            context_menu_grab_seat = seat;
        }
    }
}
