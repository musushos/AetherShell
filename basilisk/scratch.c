#include <gtk/gtk.h>
#include <cmark-gfm.h>
#include <cmark-gfm-core-extensions.h>
#include <stdio.h>

int main(int argc, char **argv) {
    gtk_init(&argc, &argv);
    cmark_gfm_core_extensions_ensure_registered();
    
    int options = CMARK_OPT_DEFAULT | CMARK_OPT_UNSAFE;
    cmark_parser *parser = cmark_parser_new(options);
    cmark_syntax_extension *ext = cmark_find_syntax_extension("table");
    if (ext) cmark_parser_attach_syntax_extension(parser, ext);
    
    const char *md = "### Heading 3\n\n- item 1\n- item 2\n\n<execute>sudo ls</execute>";
    cmark_parser_feed(parser, md, strlen(md));
    cmark_node *doc = cmark_parser_finish(parser);
    
    cmark_iter *iter = cmark_iter_new(doc);
    cmark_event_type ev_type;
    while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
        cmark_node *cur = cmark_iter_get_node(iter);
        printf("%s %s\n", ev_type == CMARK_EVENT_ENTER ? "ENTER" : "EXIT", cmark_node_get_type_string(cur));
        if (ev_type == CMARK_EVENT_ENTER) {
            if (cmark_node_get_type(cur) == CMARK_NODE_HEADING) {
                printf("  Heading level: %d\n", cmark_node_get_heading_level(cur));
            }
        }
    }
    cmark_iter_free(iter);
    cmark_node_free(doc);
    cmark_parser_free(parser);
    return 0;
}
