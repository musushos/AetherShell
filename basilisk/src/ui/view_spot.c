#include "ui/view_spot.h"
#include <gtk/gtk.h>
#include <cairo/cairo.h>
#include <math.h>
#include <string.h>
#include <gdk/gdkx.h>
#include <gdk/gdkwayland.h>

extern void cmd_ctrl_execute_all(const gchar *query);
extern gchar *cmd_ctrl_exec_math(const gchar *expr);
extern GList *cmd_ctrl_exec_file_search(const gchar *query);

typedef enum {
    SPOT_MODE_SEARCH = 0,
    SPOT_MODE_APPS,
    SPOT_MODE_AI,
    SPOT_MODE_COMMANDS,
    SPOT_MODE_COUNT
} SpotMode;

#define SP_WIDTH      620
#define SP_HEIGHT      62
#define SP_RADIUS      36.0
#define SP_ICON_R      22.0
#define SP_ICON_GAP    10.0
#define SP_SEARCH_W   380.0
#define SP_PAD         20.0
#define SP_FONT       "Inter"

static struct {
    GtkWidget     *window;
    GtkWidget     *canvas;
    GtkWidget     *entry;
    GtkWidget     *revealer;
    GtkWidget     *listbox;
    guint          debounce_timer_id;
    gchar         *debounce_query;
    SpotMode       mode;
    gboolean       visible;
    int            hover;
    gboolean       cursor_visible;
    guint          blink_timer_id;
    double         icon_cx[SPOT_MODE_COUNT];
    double         icon_cy[SPOT_MODE_COUNT];
} S;

static void pill(cairo_t *cr, double x, double y, double w, double h, double r) {
    cairo_new_sub_path(cr);
    cairo_arc(cr, x+w-r, y+r,   r, -G_PI/2, 0);
    cairo_arc(cr, x+w-r, y+h-r, r,  0,       G_PI/2);
    cairo_arc(cr, x+r,   y+h-r, r,  G_PI/2,  G_PI);
    cairo_arc(cr, x+r,   y+r,   r,  G_PI,   -G_PI/2);
    cairo_close_path(cr);
}

static void text_at(cairo_t *cr, const char *font, double size, const char *txt, double cx, double cy, double r, double g, double b, double a) {
    cairo_save(cr);
    cairo_select_font_face(cr, font, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, size);
    cairo_text_extents_t e;
    cairo_text_extents(cr, txt, &e);
    cairo_set_source_rgba(cr, r, g, b, a);
    cairo_move_to(cr, cx - e.width/2 - e.x_bearing, cy - e.height/2 - e.y_bearing);
    cairo_show_text(cr, txt);
    cairo_restore(cr);
}

static void draw_lens(cairo_t *cr, double cx, double cy) {
    cairo_save(cr);
    cairo_set_line_width(cr, 2.2);
    cairo_set_source_rgba(cr, 1, 1, 1, 0.6);
    cairo_new_path(cr);
    cairo_arc(cr, cx-2, cy-2, 8.5, 0, 2*G_PI);
    cairo_stroke(cr);
    double a = G_PI/4;
    cairo_move_to(cr, cx-2 + 7.5*cos(a), cy-2 + 7.5*sin(a));
    cairo_line_to(cr, cx-2 + 13*cos(a),  cy-2 + 13*sin(a));
    cairo_stroke(cr);
    cairo_restore(cr);
}

static void draw_icon(cairo_t *cr, int idx, double cx, double cy, gboolean active, gboolean hov) {
    cairo_new_path(cr);
    cairo_arc(cr, cx, cy, SP_ICON_R, 0, 2*G_PI);
    if (active) {
        cairo_set_source_rgba(cr, 0, 0, 0, 0.600);
        cairo_fill_preserve(cr);
        cairo_set_source_rgba(cr, 0, 0, 0, 0.700);
        cairo_set_line_width(cr, 1.4);
        cairo_stroke(cr);
    } else if (hov) {
        cairo_set_source_rgba(cr, 0, 0, 0, 0.450);
        cairo_fill_preserve(cr);
        cairo_set_source_rgba(cr, 0, 0, 0, 0.500);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);
    } else {
        cairo_set_source_rgba(cr, 0, 0, 0, 0.300);
        cairo_fill_preserve(cr);
        cairo_set_source_rgba(cr, 0, 0, 0, 0.350);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);
    }

    double alpha = active ? 0.95 : (hov ? 0.80 : 0.55);
    const char *nf[] = { "󰍉", "󰀻", "󰭻", "󰘳" };
    const char *fb[] = { "SRC", "APP", "AI", "CMD" };

    cairo_save(cr);
    cairo_select_font_face(cr, "Symbols Nerd Font", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 18);
    cairo_text_extents_t e;
    cairo_text_extents(cr, nf[idx], &e);
    if (e.width > 3) {
        cairo_set_source_rgba(cr, 1, 1, 1, alpha);
        cairo_move_to(cr, cx - e.width/2 - e.x_bearing, cy - e.height/2 - e.y_bearing);
        cairo_show_text(cr, nf[idx]);
    } else {
        text_at(cr, SP_FONT, 9, fb[idx], cx, cy, 1, 1, 1, alpha);
    }
    cairo_restore(cr);
}

void view_spot_redraw(void) {
    if (S.canvas) gtk_widget_queue_draw(S.canvas);
}

static gboolean on_blink_timer(gpointer data) {
    (void)data;
    if (S.visible) {
        S.cursor_visible = !S.cursor_visible;
        view_spot_redraw();
    }
    return TRUE;
}

static void reset_blink_timer(void) {
    if (S.blink_timer_id > 0) g_source_remove(S.blink_timer_id);
    S.cursor_visible = TRUE;
    S.blink_timer_id = g_timeout_add(600, on_blink_timer, NULL);
    view_spot_redraw();
}

static gboolean on_draw(GtkWidget *w, cairo_t *cr, gpointer d) {
    (void)w; (void)d;
    double H = SP_HEIGHT;
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    double bar_w = SP_SEARCH_W;
    double bar_h = H - SP_PAD;
    double bar_y = SP_PAD / 2.0;
    pill(cr, SP_PAD/2, bar_y, bar_w, bar_h, bar_h / 2.0);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.300);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.350);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    draw_lens(cr, SP_PAD/2 + SP_PAD + 4, H/2);

    const gchar *txt = gtk_entry_get_text(GTK_ENTRY(S.entry));
    double tx = SP_PAD/2 + SP_PAD*2 + 12;
    double ty = H/2;

    if (txt && txt[0]) {
        cairo_save(cr);
        cairo_select_font_face(cr, SP_FONT, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 18);
        cairo_text_extents_t e;
        cairo_text_extents(cr, txt, &e);
        cairo_set_source_rgba(cr, 1, 1, 1, 0.95);
        cairo_move_to(cr, tx, ty - e.height/2 - e.y_bearing);
        cairo_show_text(cr, txt);
        if (S.cursor_visible) {
            cairo_set_source_rgba(cr, 1, 1, 1, 0.85);
            cairo_set_line_width(cr, 1.5);
            double cx2 = tx + e.x_advance + 1;
            cairo_move_to(cr, cx2, ty-11);
            cairo_line_to(cr, cx2, ty+11);
            cairo_stroke(cr);
        }
        cairo_restore(cr);
    } else {
        const char *ph[] = {
            "Basilisk Search",
            "Browse Applications...",
            "Ask AI anything...",
            "Type a command..."
        };
        cairo_save(cr);
        cairo_select_font_face(cr, SP_FONT, CAIRO_FONT_SLANT_ITALIC, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 17);
        cairo_text_extents_t e;
        cairo_text_extents(cr, ph[S.mode], &e);
        cairo_set_source_rgba(cr, 1, 1, 1, 0.35);
        cairo_move_to(cr, tx, ty - e.height/2 - e.y_bearing);
        cairo_show_text(cr, ph[S.mode]);
        if (S.cursor_visible) {
            cairo_set_source_rgba(cr, 1, 1, 1, 0.85);
            cairo_set_line_width(cr, 1.5);
            cairo_move_to(cr, tx, ty-11);
            cairo_line_to(cr, tx, ty+11);
            cairo_stroke(cr);
        }
        cairo_restore(cr);
    }

    double ix0 = SP_PAD/2 + bar_w + SP_ICON_GAP + SP_ICON_R;
    for (int i = 0; i < SPOT_MODE_COUNT; i++) {
        double cx = ix0 + i * (SP_ICON_R*2 + SP_ICON_GAP);
        double cy = H/2.0;
        S.icon_cx[i] = cx;
        S.icon_cy[i] = cy;
        draw_icon(cr, i, cx, cy, S.mode == (SpotMode)i, S.hover == i);
    }

    return FALSE;
}

static int hit_icon(double mx, double my) {
    for (int i = 0; i < SPOT_MODE_COUNT; i++) {
        double dx = mx - S.icon_cx[i];
        double dy = my - S.icon_cy[i];
        if (dx*dx + dy*dy <= SP_ICON_R*SP_ICON_R) return i;
    }
    return -1;
}

static gboolean on_click(GtkWidget *w, GdkEventButton *ev, gpointer d) {
    (void)w; (void)d;
    if (ev->button != 1) return FALSE;

    int idx = hit_icon(ev->x, ev->y);
    if (idx < 0) {
        gtk_widget_grab_focus(S.entry);
        return TRUE;
    }

    SpotMode m = (SpotMode)idx;
    if (m == S.mode && m != SPOT_MODE_APPS) {
        S.mode = SPOT_MODE_SEARCH;
        gtk_widget_grab_focus(S.entry);
        view_spot_redraw();
        return TRUE;
    }
    S.mode = m;

    if (S.mode == SPOT_MODE_APPS) {
        view_spot_hide();
        cmd_ctrl_execute_all("app:");
    } else if (S.mode == SPOT_MODE_AI) {
        view_spot_hide();
        cmd_ctrl_execute_all("ai:");
    } else {
        gtk_entry_set_text(GTK_ENTRY(S.entry), "");
        gtk_widget_grab_focus(S.entry);
        view_spot_redraw();
    }
    return TRUE;
}

static gboolean on_motion(GtkWidget *w, GdkEventMotion *ev, gpointer d) {
    (void)w; (void)d;
    int h = hit_icon(ev->x, ev->y);
    if (h != S.hover) { S.hover = h; view_spot_redraw(); }
    return FALSE;
}

static gboolean on_leave(GtkWidget *w, GdkEventCrossing *ev, gpointer d) {
    (void)w; (void)ev; (void)d;
    if (S.hover != -1) { S.hover = -1; view_spot_redraw(); }
    return FALSE;
}

static gboolean on_key(GtkWidget *w, GdkEventKey *ev, gpointer d) {
    (void)w; (void)d;
    if (ev->keyval == GDK_KEY_Escape) {
        view_spot_hide();
        return TRUE;
    }
    if (ev->keyval == GDK_KEY_Down) {
        if (gtk_revealer_get_reveal_child(GTK_REVEALER(S.revealer))) {
            gtk_widget_grab_focus(S.listbox);
            return TRUE;
        }
    }
    if (ev->keyval == GDK_KEY_Return || ev->keyval == GDK_KEY_KP_Enter) {
        const gchar *t = gtk_entry_get_text(GTK_ENTRY(S.entry));
        gchar *formatted_query = NULL;
        
        if (t && t[0]) {
            switch (S.mode) {
                case SPOT_MODE_APPS:
                    formatted_query = g_strdup_printf("app:%s", t);
                    break;
                case SPOT_MODE_SEARCH:
                    formatted_query = g_strdup(t); // main_ctrl decides what to do
                    break;
                case SPOT_MODE_COMMANDS:
                    formatted_query = g_str_has_prefix(t, "vater:") ? g_strdup(t) : g_strdup_printf("vater:%s", t);
                    break;
                case SPOT_MODE_AI:
                    formatted_query = g_str_has_prefix(t, "ai:") ? g_strdup(t) : g_strdup_printf("ai:%s", t);
                    break;
                default:
                    break;
            }
        } else {
            if (S.mode == SPOT_MODE_APPS) formatted_query = g_strdup("app:");
            else if (S.mode == SPOT_MODE_AI) formatted_query = g_strdup("ai:");
        }
        
        view_spot_hide();
        if (formatted_query) {
            cmd_ctrl_execute_all(formatted_query);
            g_free(formatted_query);
        }
        return TRUE;
    }
    if (!gtk_widget_has_focus(S.entry))
        return gtk_widget_event(S.entry, (GdkEvent *)ev);
        
    reset_blink_timer();
    return FALSE;
}

static void on_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer user_data) {
    (void)box; (void)user_data;
    gchar *res = (gchar *)g_object_get_data(G_OBJECT(row), "result");
    if (res) {
        GtkClipboard *cb = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
        gtk_clipboard_set_text(cb, res, -1);
        view_spot_hide();
        return;
    }
    
    gchar *path = (gchar *)g_object_get_data(G_OBJECT(row), "path");
    if (path) {
        gchar *uri = g_filename_to_uri(path, NULL, NULL);
        if (uri) {
            g_app_info_launch_default_for_uri(uri, NULL, NULL);
            g_free(uri);
        }
        view_spot_hide();
        return;
    }
}

static gboolean on_debounce_timeout(gpointer d) {
    (void)d;
    S.debounce_timer_id = 0;
    if (!S.debounce_query) return FALSE;
    
    GList *results = cmd_ctrl_exec_file_search(S.debounce_query);
    
    GList *children = gtk_container_get_children(GTK_CONTAINER(S.listbox));
    for (GList *iter = children; iter != NULL; iter = g_list_next(iter))
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    g_list_free(children);
    
    if (results) {
        int count = 0;
        for (GList *l = results; l != NULL && count < 10; l = l->next) {
            gchar *path = (gchar *)l->data;
            gchar *basename = g_path_get_basename(path);
            
            GtkWidget *row = gtk_list_box_row_new();
            GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
            GtkWidget *lbl_name = gtk_label_new(basename);
            gtk_widget_set_halign(lbl_name, GTK_ALIGN_START);
            GtkWidget *lbl_path = gtk_label_new(path);
            gtk_widget_set_halign(lbl_path, GTK_ALIGN_START);
            
            PangoAttrList *attrs = pango_attr_list_new();
            pango_attr_list_insert(attrs, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
            gtk_label_set_attributes(GTK_LABEL(lbl_name), attrs);
            pango_attr_list_unref(attrs);
            
            PangoAttrList *attrs2 = pango_attr_list_new();
            pango_attr_list_insert(attrs2, pango_attr_scale_new(0.8));
            pango_attr_list_insert(attrs2, pango_attr_foreground_new(40000, 40000, 40000));
            gtk_label_set_attributes(GTK_LABEL(lbl_path), attrs2);
            pango_attr_list_unref(attrs2);
            
            gtk_box_pack_start(GTK_BOX(box), lbl_name, FALSE, FALSE, 0);
            gtk_box_pack_start(GTK_BOX(box), lbl_path, FALSE, FALSE, 0);
            gtk_container_add(GTK_CONTAINER(row), box);
            
            g_object_set_data_full(G_OBJECT(row), "path", g_strdup(path), g_free);
            gtk_list_box_insert(GTK_LIST_BOX(S.listbox), row, -1);
            g_free(basename);
            count++;
        }
        g_list_free_full(results, g_free);
        gtk_widget_show_all(S.listbox);
        gtk_revealer_set_reveal_child(GTK_REVEALER(S.revealer), TRUE);
    } else {
        gtk_revealer_set_reveal_child(GTK_REVEALER(S.revealer), FALSE);
    }
    
    g_free(S.debounce_query);
    S.debounce_query = NULL;
    return FALSE;
}

static void on_entry_changed(GtkEditable *e, gpointer d) {
    (void)e; (void)d;
    reset_blink_timer();
    
    const gchar *text = gtk_entry_get_text(GTK_ENTRY(S.entry));
    if (!text || text[0] == '\0') {
        gtk_revealer_set_reveal_child(GTK_REVEALER(S.revealer), FALSE);
        return;
    }
    
    if (g_str_has_prefix(text, "!=") || g_str_has_prefix(text, "!:")) {
        gchar *expr = g_strdup(text + 2);
        gchar *res = cmd_ctrl_exec_math(expr);
        
        GList *children = gtk_container_get_children(GTK_CONTAINER(S.listbox));
        for (GList *iter = children; iter != NULL; iter = g_list_next(iter))
            gtk_widget_destroy(GTK_WIDGET(iter->data));
        g_list_free(children);
        
        if (res && strlen(res) > 0) {
            GtkWidget *row = gtk_list_box_row_new();
            GtkWidget *lbl = gtk_label_new(res);
            gtk_widget_set_halign(lbl, GTK_ALIGN_START);
            
            PangoAttrList *attrs = pango_attr_list_new();
            pango_attr_list_insert(attrs, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
            pango_attr_list_insert(attrs, pango_attr_scale_new(1.2));
            gtk_label_set_attributes(GTK_LABEL(lbl), attrs);
            pango_attr_list_unref(attrs);
            
            gtk_container_add(GTK_CONTAINER(row), lbl);
            gtk_list_box_insert(GTK_LIST_BOX(S.listbox), row, -1);
            gtk_widget_show_all(S.listbox);
            gtk_revealer_set_reveal_child(GTK_REVEALER(S.revealer), TRUE);
            
            g_object_set_data_full(G_OBJECT(row), "result", g_strdup(res), g_free);
        } else {
            gtk_revealer_set_reveal_child(GTK_REVEALER(S.revealer), FALSE);
        }
        g_free(res);
        g_free(expr);
    } else if (g_str_has_prefix(text, "vafile:")) {
        if (S.debounce_timer_id > 0) g_source_remove(S.debounce_timer_id);
        if (S.debounce_query) g_free(S.debounce_query);
        S.debounce_query = g_strdup(text + 7);
        S.debounce_timer_id = g_timeout_add(400, on_debounce_timeout, NULL);
    } else {
        gtk_revealer_set_reveal_child(GTK_REVEALER(S.revealer), FALSE);
    }
}

static void on_realize(GtkWidget *w, gpointer d) {
    (void)d;
    GdkWindow *gw = gtk_widget_get_window(w);
    if (!gw) return;
    if (GDK_IS_WAYLAND_WINDOW(gw))
        gdk_wayland_window_announce_csd(gw);
    else {
        gdk_window_set_decorations(gw, 0);
        gdk_window_set_functions(gw, 0);
    }
}

static gboolean on_window_delete(GtkWidget *widget, GdkEvent *event, gpointer data) {
    (void)widget; (void)event; (void)data;
    view_spot_hide();
    return TRUE;
}

static gboolean on_window_draw(GtkWidget *w, cairo_t *cr, gpointer d) {
    (void)w; (void)d;
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    return FALSE;
}

void view_spot_init(void) {
    S.mode    = SPOT_MODE_SEARCH;
    S.hover   = -1;
    S.visible = FALSE;
    S.debounce_timer_id = 0;
    S.debounce_query = NULL;

    static gboolean css_applied = FALSE;
    if (!css_applied) {
        GtkCssProvider *css = gtk_css_provider_new();
        gtk_css_provider_load_from_data(css,
            "#spot-window { background: transparent; }"
            "#spot-listbox { background: rgba(0, 0, 0, 0.5); border-radius: 12px; padding: 4px; border: 1px solid rgba(255,255,255,0.1); }"
            "#spot-listbox row { color: white; padding: 8px 12px; border-radius: 8px; }"
            "#spot-listbox row:selected { background: rgba(255, 255, 255, 0.15); }", -1, NULL);
        gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_USER + 100);
        g_object_unref(css);
        css_applied = TRUE;
    }

    S.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_name(S.window, "spot-window");
    gtk_window_set_title(GTK_WINDOW(S.window), "Basilisk Spotlight");
    gtk_window_set_decorated(GTK_WINDOW(S.window), FALSE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(S.window), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(S.window), TRUE);
    gtk_window_set_type_hint(GTK_WINDOW(S.window), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_keep_above(GTK_WINDOW(S.window), TRUE);
    gtk_window_set_resizable(GTK_WINDOW(S.window), FALSE);
    gtk_widget_set_app_paintable(S.window, TRUE);
    g_signal_connect(S.window, "draw", G_CALLBACK(on_window_draw), NULL);

    GdkScreen *scr = gtk_widget_get_screen(S.window);
    GdkVisual *vis = gdk_screen_get_rgba_visual(scr);
    if (vis) gtk_widget_set_visual(S.window, vis);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(S.window), main_box);

    GtkWidget *fixed = gtk_fixed_new();
    gtk_widget_set_size_request(fixed, SP_WIDTH, SP_HEIGHT);
    gtk_box_pack_start(GTK_BOX(main_box), fixed, FALSE, FALSE, 0);

    S.canvas = gtk_drawing_area_new();
    gtk_widget_set_size_request(S.canvas, SP_WIDTH, SP_HEIGHT);
    gtk_widget_set_can_focus(S.canvas, TRUE);
    gtk_widget_set_events(S.canvas, GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK | GDK_KEY_PRESS_MASK);
    gtk_fixed_put(GTK_FIXED(fixed), S.canvas, 0, 0);

    S.entry = gtk_entry_new();
    gtk_widget_set_size_request(S.entry, 1, 1);
    gtk_fixed_put(GTK_FIXED(fixed), S.entry, -1000, -1000);

    GtkWidget *rev_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(main_box), rev_hbox, TRUE, TRUE, 0);

    GtkWidget *pad_left = gtk_fixed_new();
    gtk_widget_set_size_request(pad_left, SP_PAD/2, 1);
    gtk_box_pack_start(GTK_BOX(rev_hbox), pad_left, FALSE, FALSE, 0);

    S.revealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(S.revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
    gtk_revealer_set_transition_duration(GTK_REVEALER(S.revealer), 150);
    gtk_box_pack_start(GTK_BOX(rev_hbox), S.revealer, TRUE, TRUE, 0);

    GtkWidget *pad_right = gtk_fixed_new();
    gtk_widget_set_size_request(pad_right, SP_WIDTH - (SP_PAD/2 + SP_SEARCH_W), 1);
    gtk_box_pack_start(GTK_BOX(rev_hbox), pad_right, FALSE, FALSE, 0);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_size_request(scroll, -1, 240);
    gtk_widget_set_margin_bottom(scroll, 10);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(S.revealer), scroll);

    S.listbox = gtk_list_box_new();
    gtk_widget_set_name(S.listbox, "spot-listbox");
    g_signal_connect(S.listbox, "row-activated", G_CALLBACK(on_row_activated), NULL);
    gtk_container_add(GTK_CONTAINER(scroll), S.listbox);

    g_signal_connect(S.canvas, "draw", G_CALLBACK(on_draw), NULL);
    g_signal_connect(S.canvas, "button-press-event", G_CALLBACK(on_click), NULL);
    g_signal_connect(S.canvas, "motion-notify-event", G_CALLBACK(on_motion), NULL);
    g_signal_connect(S.canvas, "leave-notify-event", G_CALLBACK(on_leave), NULL);
    g_signal_connect(S.entry, "changed", G_CALLBACK(on_entry_changed), NULL);
    g_signal_connect(S.window, "key-press-event", G_CALLBACK(on_key), NULL);
    g_signal_connect(S.window, "delete-event", G_CALLBACK(on_window_delete), NULL);
    g_signal_connect(S.window, "realize", G_CALLBACK(on_realize), NULL);
}

void view_spot_show(void) {
    if (S.visible) return;

    gint x = 0, y = 0;
    GdkDisplay *dpy = gdk_display_get_default();
    GdkMonitor *mon = gdk_display_get_primary_monitor(dpy);
    if (!mon && gdk_display_get_n_monitors(dpy) > 0)
        mon = gdk_display_get_monitor(dpy, 0);

    if (mon) {
        GdkRectangle geo;
        gdk_monitor_get_geometry(mon, &geo);
        x = geo.x + (geo.width  - SP_WIDTH)  / 2;
        y = geo.y + (int)(geo.height * 0.30);
    } else {
        gtk_window_set_position(GTK_WINDOW(S.window), GTK_WIN_POS_CENTER);
    }

    gtk_entry_set_text(GTK_ENTRY(S.entry), "");
    gtk_revealer_set_reveal_child(GTK_REVEALER(S.revealer), FALSE);
    S.mode  = SPOT_MODE_SEARCH;
    S.hover = -1;

    gtk_window_move(GTK_WINDOW(S.window), x, y);
    gtk_widget_show_all(S.window);
    gtk_window_present(GTK_WINDOW(S.window));
    gtk_widget_grab_focus(S.entry);

    S.visible = TRUE;
    reset_blink_timer();
}

void view_spot_hide(void) {
    if (!S.visible) return;
    if (S.blink_timer_id > 0) {
        g_source_remove(S.blink_timer_id);
        S.blink_timer_id = 0;
    }
    gtk_widget_hide(S.window);
    S.visible = FALSE;
}

void view_spot_toggle(void) {
    if (S.visible) view_spot_hide();
    else view_spot_show();
}
