#ifndef CORE_STATE_H
#define CORE_STATE_H

#include <glib.h>

// Categories
typedef enum {
    CAT_ALL = 0,
    CAT_DEVELOPMENT,
    CAT_SYSTEM,
    CAT_INTERNET,
    CAT_UTILITY,
    CAT_OTHER,
    CAT_COUNT
} AppCategory;

// App Entry Model
typedef struct {
    gchar *name;
    gchar *exec;
    gchar *icon;
    gchar *desktop_file;
    AppCategory category;
} AppEntry;

#endif
