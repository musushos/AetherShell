#include <gtk/gtk.h>
#include <malloc.h>
#include "wallpaper.h"
#include "layer_manager.h"

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    init_main_window();
    layer_manager_init(main_window);

    /* By default, show the static image layout layer */
    layer_manager_show_image(wallpaper_get_widget());

    gtk_widget_show_all(main_window);

    init_wallpaper_monitor();
    load_saved_wallpaper();
    malloc_trim(0);
    
    gtk_main();

    return 0;
}
