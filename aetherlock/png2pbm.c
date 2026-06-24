#include <stdio.h>
#include <cairo.h>

int main() {
    cairo_surface_t *img = cairo_image_surface_create_from_png("vaxp-logo.png");
    if (cairo_surface_status(img) != CAIRO_STATUS_SUCCESS) {
        printf("Failed to load PNG\n");
        return 1;
    }
    int w = cairo_image_surface_get_width(img);
    int h = cairo_image_surface_get_height(img);
    unsigned char *data = cairo_image_surface_get_data(img);
    int stride = cairo_image_surface_get_stride(img);

    FILE *f = fopen("logo.pbm", "w");
    fprintf(f, "P1\n%d %d\n", w, h);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            // ARGB32 format
            unsigned char a = data[y * stride + x * 4 + 3];
            unsigned char r = data[y * stride + x * 4 + 2];
            unsigned char g = data[y * stride + x * 4 + 1];
            unsigned char b = data[y * stride + x * 4 + 0];
            
            // If the logo is white with transparent background, alpha=0 is white, alpha>0 is black (the logo)
            // If the logo is white, we want it black for potrace, and background white
            // Let's just use alpha: if alpha > 128, it's the logo (black = 1 in PBM).
            if (a > 128) {
                fprintf(f, "1 ");
            } else {
                fprintf(f, "0 ");
            }
        }
        fprintf(f, "\n");
    }
    fclose(f);
    return 0;
}
