CC = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude -D_GNU_SOURCE $(shell pkg-config --cflags polkit-agent-1 glib-2.0 gio-unix-2.0 gtk+-3.0 gtk-layer-shell-0)
LDFLAGS = $(shell pkg-config --libs polkit-agent-1 glib-2.0 gio-unix-2.0 gtk+-3.0 gtk-layer-shell-0)

TARGET = auth
SRCDIR = src
INCDIR = include
BUILDDIR = build

SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)
	@echo "Build complete!"

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	@echo "Cleaning..."
	rm -rf $(BUILDDIR) $(TARGET)
	@echo "Clean complete!"

.PHONY: all clean
