#include <glib.h>
#include <stdio.h>

int main() {
    const char *body = "هذا النص <b>عريض</b>، وهذا <i>مائل</i>، ورابط: <a href=\"https://vaxp.org\">VAXP</a>";
    gchar *wrapped = g_strdup_printf("<markup>%s</markup>", body);
    
    GMarkupParser parser = {0};
    GMarkupParseContext *ctx = g_markup_parse_context_new(&parser, 0, NULL, NULL);
    GError *error = NULL;
    gboolean valid = g_markup_parse_context_parse(ctx, wrapped, -1, &error);
    if (!valid) {
        printf("Error: %s\n", error->message);
    } else {
        valid = g_markup_parse_context_end_parse(ctx, &error);
        if (!valid) printf("End Error: %s\n", error->message);
        else printf("Valid!\n");
    }
    g_free(wrapped);
    return 0;
}
