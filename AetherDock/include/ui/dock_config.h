#ifndef AETHERDOCK_DOCK_CONFIG_H
#define AETHERDOCK_DOCK_CONFIG_H

#include "ui/globals.h"

void load_dock_config(void);
void apply_dock_position(void);
void on_dock_config_changed(GFileMonitor *monitor, GFile *file, GFile *other_file, GFileMonitorEvent event_type, gpointer user_data);

#endif // AETHERDOCK_DOCK_CONFIG_H
