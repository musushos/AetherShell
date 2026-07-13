/*
 * video_wallpaper.h
 * Video wallpaper subsystem via libmpv Software render API.
 */

#ifndef VIDEO_WALLPAPER_H
#define VIDEO_WALLPAPER_H

#include <gtk/gtk.h>
#include <cairo/cairo.h>

/*
 * video_wallpaper_init:
 *   Initialise the mpv handle and OpenGL render context.
 *   @gl_area: the GtkGLArea used for rendering the video.
 *   Returns TRUE on success.
 */
gboolean video_wallpaper_init(GtkWidget *gl_area);

/* Load and loop a video file as wallpaper. */
void video_wallpaper_load(const char *path);

/* Stop playback and release resources. */
void video_wallpaper_stop(void);

/* TRUE if a video is currently playing. */
gboolean video_wallpaper_is_active(void);

/* Set playback volume (0–100). */
void video_wallpaper_set_volume(int volume);

/*
 * is_video_file:
 *   Returns TRUE if @path has a recognised video extension.
 */
gboolean is_video_file(const char *path);

#endif /* VIDEO_WALLPAPER_H */
