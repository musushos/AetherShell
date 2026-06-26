#include <glib.h>
#include <stdio.h>

int main() {
    const gchar *body = "هذا النص <b>عريض</b>، وهذا <i>مائل</i>، وهذا رابط يمكنك تجربته: <a href=\"https://vaxp.org\">موقع VAXP</a>";
    GRegex *regex = g_regex_new("<a[^>]*>|</a>", 0, 0, NULL);
    gchar *clean_body = g_regex_replace_literal(regex, body, -1, 0, "", 0, NULL);
    printf("Original: %s\nCleaned: %s\n", body, clean_body);
    g_free(clean_body);
    g_regex_unref(regex);
    return 0;
}
