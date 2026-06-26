#include <glib.h>
#include <stdio.h>

int main() {
    const char *body = "هذا النص <b>عريض</b>، وهذا <i>مائل</i>";
    GMarkupParseContext *ctx = g_markup_parse_context_new(NULL, 0, NULL, NULL);
    GError *error = NULL;
    gboolean valid = g_markup_parse_context_parse(ctx, body, -1, &error);
    if (!valid) {
        printf("Error: %s\n", error->message);
    } else {
        valid = g_markup_parse_context_end_parse(ctx, &error);
        if (!valid) printf("End Error: %s\n", error->message);
        else printf("Valid!\n");
    }
    return 0;
}
