#pragma once

#include <pebble.h>

// Renders the configurable info layout (time + the four widget slots) with a
// readable halo over the globe. Drawing is allocation-free: widget format
// strings are parsed into static buffers by info_layout_update_widgets() and
// only read back during the layer update.

// Point the layout at the caller-owned time string buffer. The pointer is kept,
// so the buffer must stay valid; its contents may change between redraws.
void info_layout_set_time_text(const char *time_text);

// Hide the secondary widget rows while quick view (an obstruction) is showing.
void info_layout_set_quick_view(bool visible);

// Re-parse the widget format strings from globalSettings into the static
// buffers. Call on the minute tick / settings change, not on every redraw.
void info_layout_update_widgets(void);

// Layer update_proc: draws the ordered slot list for the current layout.
void info_layout_update_proc(Layer *layer, GContext *ctx);
