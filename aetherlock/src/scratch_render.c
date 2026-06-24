#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <wayland-client.h>
#include "cairo.h"
#include "background-image.h"
#include "aetherlock.h"
#include "log.h"

#define M_PI 3.14159265358979323846

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

/* ── Frame callback (unchanged) ────────────────────────────────────────── */

static void surface_frame_handle_done(void *data, struct wl_callback *callback,
		uint32_t time) {
	struct aetherlock_surface *surface = data;
	wl_callback_destroy(callback);
	surface->frame = NULL;
	render(surface);
}

static const struct wl_callback_listener surface_frame_listener = {
	.done = surface_frame_handle_done,
};

static bool render_frame(struct aetherlock_surface *surface);

/* ── Background render (unchanged logic) ───────────────────────────────── */

void render(struct aetherlock_surface *surface) {
	struct aetherlock_state *state = surface->state;

	int buffer_width = surface->width * surface->scale;
	int buffer_height = surface->height * surface->scale;
	if (buffer_width == 0 || buffer_height == 0) {
		return;
	}
	if (!surface->dirty || surface->frame) {
		return;
	}

	bool need_destroy = false;
	struct pool_buffer buffer;

	if (buffer_width != surface->last_buffer_width ||
			buffer_height != surface->last_buffer_height) {
		need_destroy = true;
		if (!create_buffer(state->shm, &buffer, buffer_width, buffer_height,
				WL_SHM_FORMAT_ARGB8888)) {
			aetherlock_log(LOG_ERROR,
				"Failed to create new buffer for frame background.");
			return;
		}

		cairo_t *cairo = buffer.cairo;
		cairo_set_antialias(cairo, CAIRO_ANTIALIAS_BEST);

		cairo_save(cairo);
		cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
		cairo_set_source_u32(cairo, state->args.colors.background);
		cairo_paint(cairo);
		if (surface->image && state->args.mode != BACKGROUND_MODE_SOLID_COLOR) {
			cairo_set_operator(cairo, CAIRO_OPERATOR_OVER);
			render_background_image(cairo, surface->image,
				state->args.mode, buffer_width, buffer_height);
		}
		/* Dark overlay on the full screen — rgba(0,0,0,0.20) */
		cairo_set_operator(cairo, CAIRO_OPERATOR_OVER);
		cairo_set_source_rgba(cairo, 0.0, 0.0, 0.0, 0.20);
		cairo_paint(cairo);
		cairo_restore(cairo);
		cairo_identity_matrix(cairo);

		wl_surface_attach(surface->surface, buffer.buffer, 0, 0);
		wl_surface_damage_buffer(surface->surface, 0, 0, INT32_MAX, INT32_MAX);

		surface->last_buffer_width = buffer_width;
		surface->last_buffer_height = buffer_height;
	}

	wl_surface_set_buffer_scale(surface->surface, surface->scale);
	render_frame(surface);
	surface->dirty = false;
	surface->frame = wl_surface_frame(surface->surface);
	wl_callback_add_listener(surface->frame, &surface_frame_listener, surface);
	wl_surface_commit(surface->surface);

	if (need_destroy) {
		destroy_buffer(&buffer);
	}
}

/* ── Drawing helpers ────────────────────────────────────────────────────── */

static void rounded_rect(cairo_t *cr, double x, double y,
		double w, double h, double r) {
	cairo_move_to(cr, x + r, y);
	cairo_line_to(cr, x + w - r, y);
	cairo_arc(cr, x + w - r, y + r, r, -M_PI / 2.0, 0.0);
	cairo_line_to(cr, x + w, y + h - r);
	cairo_arc(cr, x + w - r, y + h - r, r, 0.0, M_PI / 2.0);
	cairo_line_to(cr, x + r, y + h);
	cairo_arc(cr, x + r, y + h - r, r, M_PI / 2.0, M_PI);
	cairo_line_to(cr, x, y + r);
	cairo_arc(cr, x + r, y + r, r, M_PI, 3.0 * M_PI / 2.0);
	cairo_close_path(cr);
}

static double draw_text_centered(cairo_t *cr, const char *text,
		double cx, double y) {
	cairo_text_extents_t ext;
	cairo_text_extents(cr, text, &ext);
	cairo_move_to(cr, cx - ext.width / 2.0 - ext.x_bearing, y);
	cairo_show_text(cr, text);
	return ext.width;
}

static void draw_person_icon(cairo_t *cr, double cx, double cy, double r) {
	cairo_set_source_rgba(cr, 1, 1, 1, 0.70);
	cairo_arc(cr, cx, cy - r * 0.30, r * 0.32, 0, 2.0 * M_PI);
	cairo_fill(cr);
	cairo_arc(cr, cx, cy + r * 0.55, r * 0.50, M_PI, 2.0 * M_PI);
	cairo_fill(cr);
}

/* ── New Layout Helpers ─────────────────────────────────────────────────── */

static void draw_card_bg(cairo_t *cr, double x, double y, double w, double h) {
	rounded_rect(cr, x, y, w, h, 16.0);
	cairo_set_source_rgba(cr, 20.0/255.0, 28.0/255.0, 30.0/255.0, 0.55); // panel-bg
	cairo_fill_preserve(cr);
	cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.07); // panel-border
	cairo_set_line_width(cr, 1.0);
	cairo_stroke(cr);
}

/* ── Main frame renderer ─────────────────────────────────────────────── */

static bool render_frame(struct aetherlock_surface *surface) {
	struct aetherlock_state *state = surface->state;
	double scale = (double)surface->scale;

	/* ── Layout constants (logical pixels) ── */
	const double PW = 1344.0;
	const double PH = 580.0;
	
	const double COL_W = 400.0;
	const double MID_W = 440.0;
	const double GAP = 24.0;
	const double PAD = 28.0;

	int buffer_width  = (int)(PW * scale);
	int buffer_height = (int)(PH * scale);
	int sc = (int)scale;
	if (sc > 1) {
		if (buffer_width  % sc) buffer_width  += sc - (buffer_width  % sc);
		if (buffer_height % sc) buffer_height += sc - (buffer_height % sc);
	}

	int subsurf_xpos = surface->width  / 2 - buffer_width  / (2 * sc);
	int subsurf_ypos = surface->height / 2 - buffer_height / (2 * sc);

	struct pool_buffer *buf = get_next_buffer(state->shm,
		surface->indicator_buffers, buffer_width, buffer_height);
	if (!buf) {
		aetherlock_log(LOG_ERROR, "No buffer for render_frame");
		return false;
	}

	cairo_t *cr = buf->cairo;
	cairo_set_antialias(cr, CAIRO_ANTIALIAS_BEST);
	cairo_identity_matrix(cr);
	cairo_scale(cr, scale, scale);

	/* Clear */
	cairo_save(cr);
	cairo_set_source_rgba(cr, 0, 0, 0, 0);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_paint(cr);
	cairo_restore(cr);

	/* Panel Base */
	rounded_rect(cr, 0, 0, PW, PH, 22.0);
	cairo_set_source_rgba(cr, 8.0/255.0, 14.0/255.0, 14.0/255.0, 0.45);
	cairo_fill_preserve(cr);
	cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.07);
	cairo_set_line_width(cr, 1.0);
	cairo_stroke(cr);

	/* ── Column 1: Left ───────────────────────────────────────────── */
	double cx1 = PAD;
	double cy = PAD;
	
	// Weather Card
	double w_card_h = 100.0;
	draw_card_bg(cr, cx1, cy, COL_W, w_card_h);
	
	cairo_select_font_face(cr, state->args.font, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, 20.0);
	cairo_set_source_rgba(cr, 126.0/255.0, 224.0/255.0, 201.0/255.0, 1.0); // teal
	cairo_move_to(cr, cx1 + 20, cy + 35);
	cairo_show_text(cr, "Weather");
	
	// Weather icon placeholder
	cairo_set_line_width(cr, 2.0);
	cairo_set_source_rgba(cr, 159.0/255.0, 179.0/255.0, 176.0/255.0, 1.0); // text-dim
	cairo_arc(cr, cx1 + 44, cy + 65, 12, 0, 2*M_PI);
	cairo_stroke(cr);
	
	cairo_set_font_size(cr, 18.0);
	cairo_set_source_rgba(cr, 230.0/255.0, 245.0/255.0, 240.0/255.0, 1.0); // text-bright
	cairo_move_to(cr, cx1 + 80, cy + 60);
	cairo_show_text(cr, "Overcast");
	
	cairo_select_font_face(cr, state->args.font, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 13.0);
	cairo_set_source_rgba(cr, 159.0/255.0, 179.0/255.0, 176.0/255.0, 1.0);
	cairo_move_to(cr, cx1 + 80, cy + 80);
	cairo_show_text(cr, "Humidity: 76%");
	
	cy += w_card_h + 16.0;

	// Fetch Card
	double f_card_h = 200.0;
	draw_card_bg(cr, cx1, cy, COL_W, f_card_h);
	// Prompt >
	cairo_arc(cr, cx1 + 33, cy + 31, 13, 0, 2*M_PI);
	cairo_set_source_rgba(cr, 126.0/255.0, 224.0/255.0, 201.0/255.0, 0.15);
	cairo_fill(cr);
	cairo_set_source_rgba(cr, 126.0/255.0, 224.0/255.0, 201.0/255.0, 1.0);
	cairo_set_font_size(cr, 13.0);
	draw_text_centered(cr, ">", cx1 + 33, cy + 36);
	
	cairo_set_font_size(cr, 14.0);
	cairo_set_source_rgba(cr, 159.0/255.0, 179.0/255.0, 176.0/255.0, 1.0);
	cairo_move_to(cr, cx1 + 56, cy + 36);
	cairo_show_text(cr, "caelestiafetch.sh");
	
	// Arch logo placeholder (triangle)
	cairo_move_to(cr, cx1 + 60, cy + 70);
	cairo_line_to(cr, cx1 + 90, cy + 130);
	cairo_line_to(cr, cx1 + 30, cy + 130);
	cairo_close_path(cr);
	cairo_set_source_rgba(cr, 95.0/255.0, 179.0/255.0, 224.0/255.0, 1.0);
	cairo_fill(cr);
	
	// Specs
	cairo_set_source_rgba(cr, 159.0/255.0, 179.0/255.0, 176.0/255.0, 1.0);
	cairo_move_to(cr, cx1 + 120, cy + 75);
	cairo_show_text(cr, "OS  : Arch Linux");
	cairo_move_to(cr, cx1 + 120, cy + 95);
	cairo_show_text(cr, "WM  : Hyprland");
	cairo_move_to(cr, cx1 + 120, cy + 115);
	cairo_show_text(cr, "USER: suyav");
	cairo_move_to(cr, cx1 + 120, cy + 135);
	cairo_show_text(cr, "UP  : 25 minutes");
	
	// Swatches
	const char* swatches[] = {"#3a3a3a", "#a8c93a", "#5fd9a8", "#7ee0c9", "#6f9b95", "#5b7fd6", "#7ee0c9"};
	double sw_x = cx1 + 20;
	for (int i=0; i<7; i++) {
		rounded_rect(cr, sw_x, cy + 160, 22, 22, 6.0);
		// simple parse hex to rgb (rough estimation since no helper)
		cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 1.0); // placeholder color
		cairo_fill(cr);
		sw_x += 30;
	}
	
	cy += f_card_h + 16.0;

	// Now Playing Card
	double np_card_h = 160.0;
	draw_card_bg(cr, cx1, cy, COL_W, np_card_h);
	// Content
	cairo_set_font_size(cr, 13.0);
	cairo_set_source_rgba(cr, 159.0/255.0, 179.0/255.0, 176.0/255.0, 1.0);
	cairo_move_to(cr, cx1 + 20, cy + 30);
	cairo_show_text(cr, "Now playing");
	
	cairo_set_font_size(cr, 19.0);
	cairo_set_source_rgba(cr, 230.0/255.0, 245.0/255.0, 240.0/255.0, 1.0);
	draw_text_centered(cr, "Chillhop Music", cx1 + COL_W/2, cy + 60);
	
	cairo_set_font_size(cr, 13.0);
	cairo_set_source_rgba(cr, 159.0/255.0, 179.0/255.0, 176.0/255.0, 1.0);
	draw_text_centered(cr, "nymano x Pandrezz -- Fireworks", cx1 + COL_W/2, cy + 85);

	// ── Column 2: Center ───────────────────────────────────────────
	double cx2 = cx1 + COL_W + GAP;
	double mid_x = cx2 + MID_W / 2.0;

	if (state->args.show_clock) {
		time_t t = time(NULL);
		struct tm *tm_info = localtime(&t);

		char date_str[64], time_str[32], ampm_str[8];
		strftime(date_str, sizeof(date_str), "%A, %e %B %Y", tm_info); 
		strftime(time_str, sizeof(time_str), "%I:%M", tm_info);
		strftime(ampm_str, sizeof(ampm_str), "%p", tm_info);

		cairo_select_font_face(cr, state->args.font, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
		cairo_set_font_size(cr, 64.0);
		cairo_set_source_rgba(cr, 230.0/255.0, 245.0/255.0, 240.0/255.0, 1.0);
		
		cairo_text_extents_t ext_time, ext_ampm;
		cairo_text_extents(cr, time_str, &ext_time);
		
		cairo_set_font_size(cr, 24.0);
		cairo_text_extents(cr, ampm_str, &ext_ampm);
		
		double total_w = ext_time.width + 8.0 + ext_ampm.width;
		double start_x = mid_x - total_w / 2.0;
		
		cairo_set_font_size(cr, 64.0);
		cairo_move_to(cr, start_x - ext_time.x_bearing, PAD + 80);
		cairo_show_text(cr, time_str);
		
		cairo_set_font_size(cr, 24.0);
		cairo_set_source_rgba(cr, 126.0/255.0, 224.0/255.0, 201.0/255.0, 1.0); // teal
		cairo_move_to(cr, start_x + ext_time.width + 8.0 - ext_ampm.x_bearing, PAD + 80);
		cairo_show_text(cr, ampm_str);
		
		cairo_select_font_face(cr, state->args.font, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
		cairo_set_font_size(cr, 18.0);
		cairo_set_source_rgba(cr, 159.0/255.0, 179.0/255.0, 176.0/255.0, 1.0);
		draw_text_centered(cr, date_str, mid_x, PAD + 115);
	}

	// Avatar
	const double AV_CY = PAD + 265.0;
	const double AV_R  = 100.0;
	
	cairo_arc(cr, mid_x, AV_CY, AV_R, 0, 2.0 * M_PI);
	cairo_set_source_rgba(cr, 1, 1, 1, 0.08);
	cairo_set_line_width(cr, 1.0);
	cairo_stroke(cr);
	
	if (state->avatar_image) {
		cairo_save(cr);
		cairo_arc(cr, mid_x, AV_CY, AV_R, 0, 2.0 * M_PI);
		cairo_clip(cr);
		int iw = cairo_image_surface_get_width(state->avatar_image);
		int ih = cairo_image_surface_get_height(state->avatar_image);
		double img_side = (double)MIN(iw, ih);
		double img_scale = (AV_R * 2.0) / img_side;
		cairo_translate(cr, mid_x - (double)iw * img_scale / 2.0, AV_CY - (double)ih * img_scale / 2.0);
		cairo_scale(cr, img_scale, img_scale);
		cairo_set_source_surface(cr, state->avatar_image, 0, 0);
		cairo_paint(cr);
		cairo_restore(cr);
	} else {
		cairo_arc(cr, mid_x, AV_CY, AV_R, 0, 2.0 * M_PI);
		cairo_set_source_rgba(cr, 17.0/255.0, 17.0/255.0, 17.0/255.0, 1.0);
		cairo_fill(cr);
		draw_person_icon(cr, mid_x, AV_CY, AV_R * 0.5);
	}

	// PW Box
	const double PW_W = 340.0;
	const double PW_H = 48.0;
	double pw_y = PAD + 395.0;
	double pw_x = mid_x - PW_W / 2.0;
	
	rounded_rect(cr, pw_x, pw_y, PW_W, PW_H, 24.0);
	cairo_set_source_rgba(cr, 1, 1, 1, 0.05);
	cairo_fill_preserve(cr);
	cairo_set_source_rgba(cr, 1, 1, 1, 0.08);
	cairo_set_line_width(cr, 1.0);
	cairo_stroke(cr);
	
	// lock icon placeholder
	cairo_set_source_rgba(cr, 159.0/255.0, 179.0/255.0, 176.0/255.0, 1.0);
	cairo_arc(cr, pw_x + 28, pw_y + PW_H/2, 6, 0, 2*M_PI);
	cairo_stroke(cr);
	
	// text or dots
	size_t pw_len = state->password.len;
	cairo_set_font_size(cr, 15.0);
	if (state->auth_state == AUTH_STATE_VALIDATING) {
		cairo_set_source_rgba(cr, 126.0/255.0, 224.0/255.0, 201.0/255.0, 1.0);
		cairo_move_to(cr, pw_x + 50, pw_y + PW_H/2 + 5);
		cairo_show_text(cr, "Verifying...");
	} else if (state->auth_state == AUTH_STATE_INVALID) {
		cairo_set_source_rgba(cr, 1.0, 0.3, 0.3, 1.0);
		cairo_move_to(cr, pw_x + 50, pw_y + PW_H/2 + 5);
		cairo_show_text(cr, "Incorrect password");
	} else if (pw_len > 0) {
		int ndots = (int)MIN(pw_len, 20);
		double dot_r = 4.0;
		double spacing = 8.0;
		cairo_set_source_rgba(cr, 230.0/255.0, 245.0/255.0, 240.0/255.0, 1.0);
		for (int i = 0; i < ndots; i++) {
			cairo_arc(cr, pw_x + 50 + i * (dot_r * 2.0 + spacing), pw_y + PW_H / 2.0, dot_r, 0, 2.0 * M_PI);
			cairo_fill(cr);
		}
	} else {
		cairo_set_source_rgba(cr, 159.0/255.0, 179.0/255.0, 176.0/255.0, 1.0);
		cairo_move_to(cr, pw_x + 50, pw_y + PW_H/2 + 5);
		cairo_show_text(cr, "Enter your password");
	}
	
	// arrow button
	cairo_arc(cr, pw_x + PW_W - 24, pw_y + PW_H/2, 15, 0, 2*M_PI);
	cairo_set_source_rgba(cr, 1, 1, 1, 0.07);
	cairo_fill(cr);
	cairo_move_to(cr, pw_x + PW_W - 27, pw_y + PW_H/2 - 4);
	cairo_line_to(cr, pw_x + PW_W - 21, pw_y + PW_H/2);
	cairo_line_to(cr, pw_x + PW_W - 27, pw_y + PW_H/2 + 4);
	cairo_set_source_rgba(cr, 159.0/255.0, 179.0/255.0, 176.0/255.0, 1.0);
	cairo_stroke(cr);

	// ── Column 3: Right ────────────────────────────────────────────
	double cx3 = cx2 + MID_W + GAP;
	cy = PAD;
	
	// Stats Grid
	double stat_size = (COL_W - 16.0) / 2.0;
	for (int i=0; i<4; i++) {
		double sx = cx3 + (i%2) * (stat_size + 16.0);
		double sy = cy + (i/2) * (stat_size + 16.0);
		draw_card_bg(cr, sx, sy, stat_size, stat_size);
		
		// ring
		cairo_arc(cr, sx + stat_size/2, sy + stat_size/2, stat_size/2 - 20, -M_PI/2, M_PI);
		if (i%2 == 0)
			cairo_set_source_rgba(cr, 126.0/255.0, 224.0/255.0, 201.0/255.0, 1.0);
		else
			cairo_set_source_rgba(cr, 159.0/255.0, 179.0/255.0, 176.0/255.0, 1.0);
		cairo_set_line_width(cr, 5.0);
		cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
		cairo_stroke(cr);
		
		// icon center placeholder
		cairo_arc(cr, sx + stat_size/2, sy + stat_size/2, 10, 0, 2*M_PI);
		cairo_stroke(cr);
	}
	
	cy += stat_size * 2 + 16.0 + 16.0;
	
	// Notifications Card
	double notif_h = PH - cy - PAD;
	draw_card_bg(cr, cx3, cy, COL_W, notif_h);
	cairo_set_font_size(cr, 13.0);
	cairo_set_source_rgba(cr, 159.0/255.0, 179.0/255.0, 176.0/255.0, 1.0);
	cairo_move_to(cr, cx3 + 20, cy + 30);
	cairo_show_text(cr, "Notifications");
	
	cairo_set_font_size(cr, 14.0);
	draw_text_centered(cr, "No Notifications", cx3 + COL_W/2, cy + notif_h - 40);

	/* ── Send to Wayland ─────────────────────────────────────────── */
	wl_subsurface_set_position(surface->subsurface, subsurf_xpos, subsurf_ypos);
	wl_surface_set_buffer_scale(surface->child, sc);
	wl_surface_attach(surface->child, buf->buffer, 0, 0);
	wl_surface_damage_buffer(surface->child, 0, 0, INT32_MAX, INT32_MAX);
	wl_surface_commit(surface->child);

	return true;
}
