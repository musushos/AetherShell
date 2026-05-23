.DEFAULT_GOAL := AetherCore
.PHONY: AetherCore designer clean install uninstall dist all

CC      = gcc
PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin
DATADIR ?= $(PREFIX)/share/AetherCore
RESOURCES_DIR ?= $(DATADIR)/resources
PLUGINDIR ?= $(PREFIX)/share/AetherCore/plugins

PKGS = gtk+-3.0 gtk-layer-shell-0 libpulse libpulse-mainloop-glib \
       gio-2.0 json-glib-1.0 dbusmenu-glib-0.4 wayland-client

DESIGNER_PKGS = gtk+-3.0 webkit2gtk-4.1 json-glib-1.0 gio-2.0

PKG_CHECK = $(shell pkg-config --exists $(PKGS) && echo yes || echo no)
ifeq ($(PKG_CHECK),no)
$(error Missing build dependencies. Install: libgtk-3-dev libgtk-layer-shell-dev \
libpulse-dev libjson-glib-dev libdbusmenu-glib-dev)
endif

CFLAGS  = -DVAXPWY_AetherCore_RESOURCES_DIR=\"$(RESOURCES_DIR)\" \
          $(shell pkg-config --cflags $(PKGS)) \
          -std=gnu11

LIBS    = $(shell pkg-config --libs $(PKGS)) -lX11 -lXext -lm -ldl

# ── Sources ────────────────────────────────────────────────────────────────── #
# Core engine (new files)
ENGINE_SRC = src/plugin_engine.c \
             src/css_provider.c \
             src/layout_builder.c \
             src/builtin_plugins.c

# Existing AetherCore components (unchanged)
COMPONENT_SRC = src/control_center_ui.c src/app_menu.c \
                src/battery_indicator.c src/pulse_volume.c src/power_profile.c \
                src/mpris_control.c src/power_actions.c src/venom_notifications.c \
                src/workspaces.c src/keyboard_layout.c src/sni_tray.c src/resource_paths.c \
                src/network_actions.c src/brightness_control.c src/volume_indicator.c \
                src/mic_indicator.c src/wifi_indicator.c src/sidebar.c src/notifications_ui.c \
                src/bluetooth_manager.c src/bt_indicator.c src/window_backend.c \
                src/compositor_backend.c src/aether-ipc-v1-client-protocol.c \
                src/workspaces_xorg.c src/kb_indicator_xorg.c

SRC      = src/AetherCore.c $(ENGINE_SRC) $(COMPONENT_SRC)
INCLUDES = -Iinclude -I.

# ── Build ──────────────────────────────────────────────────────────────────── #
AetherCore: $(SRC)
	$(CC) -o AetherCore $(SRC) $(CFLAGS) $(INCLUDES) $(LIBS)

# ── Designer ───────────────────────────────────────────────────────────────── #
DESIGNER_SRC = designer/main.c designer/config_io.c
DESIGNER_CFLAGS = $(shell pkg-config --cflags $(DESIGNER_PKGS)) -std=gnu11 -Idesigner
DESIGNER_LIBS   = $(shell pkg-config --libs   $(DESIGNER_PKGS)) -ldl

designer/AetherCore-designer: $(DESIGNER_SRC)
	$(CC) -o designer/AetherCore-designer $(DESIGNER_SRC) \
	      $(DESIGNER_CFLAGS) $(DESIGNER_LIBS)

designer: designer/AetherCore-designer

all: AetherCore designer

clean:
	rm -f AetherCore designer/AetherCore-designer

# ── Install ────────────────────────────────────────────────────────────────── #
install: AetherCore
	install -d $(DESTDIR)$(BINDIR)
	install -d $(DESTDIR)$(RESOURCES_DIR)
	install -d $(DESTDIR)$(RESOURCES_DIR)/images
	install -d $(DESTDIR)$(PLUGINDIR)
	install -m 755 AetherCore $(DESTDIR)$(BINDIR)/AetherCore
	install -m 644 style.css $(DESTDIR)$(RESOURCES_DIR)/style.css
	install -m 644 config/AetherCore.json $(DESTDIR)$(DATADIR)/AetherCore.json
	install -m 644 images/vaxp.png $(DESTDIR)$(RESOURCES_DIR)/images/vaxp.png
	install -m 644 images/control-center-icon.svg $(DESTDIR)$(RESOURCES_DIR)/images/control-center-icon.svg
	install -m 644 images/control-center-icon.png $(DESTDIR)$(RESOURCES_DIR)/images/control-center-icon.png

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/AetherCore
	rm -f $(DESTDIR)$(RESOURCES_DIR)/style.css
	rm -f $(DESTDIR)$(DATADIR)/AetherCore.json
	rm -f $(DESTDIR)$(RESOURCES_DIR)/images/vaxp.png
	rm -f $(DESTDIR)$(RESOURCES_DIR)/images/control-center-icon.svg
	rm -f $(DESTDIR)$(RESOURCES_DIR)/images/control-center-icon.png
	rmdir --ignore-fail-on-non-empty $(DESTDIR)$(RESOURCES_DIR)/images
	rmdir --ignore-fail-on-non-empty $(DESTDIR)$(RESOURCES_DIR)
	rmdir --ignore-fail-on-non-empty $(DESTDIR)$(PLUGINDIR)
	rmdir --ignore-fail-on-non-empty $(DESTDIR)$(DATADIR)

# ── Dist ───────────────────────────────────────────────────────────────────── #
dist:
	mkdir -p build/dist
	tar -czf build/dist/AetherCore.tar.gz \
		Makefile style.css config images include src
