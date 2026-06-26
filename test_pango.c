#include <pango/pango.h>
#include <stdio.h>

int main() {
    const char *body = "هذا النص <b>عريض (Bold)</b>، وهذا <i>مائل (Italic)</i>، وهذا <span color='red'>ملون بالأحمر</span>.\nوهذا رابط يمكنك تجربته: <a href=\"https://vaxp.org\">موقع VAXP</a>";
    GError *error = NULL;
    if (pango_parse_markup(body, -1, 0, NULL, NULL, NULL, &error)) {
        printf("Success!\n");
    } else {
        printf("Error: %s\n", error->message);
        g_clear_error(&error);
    }
    return 0;
}
