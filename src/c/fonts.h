#pragma once

#include <pebble.h>

// Resolve a per-line size tier (INFO_SIZE_S / _M / _L) to the matching time
// font and its measured metrics. `height` receives the true pixel height and
// `offset` the top dead-space offset (see text_metrics.h).
GFont time_font_for_size(uint8_t size, int *height, int *offset);

// Same as time_font_for_size, but for the info/widget font family.
GFont info_font_for_size(uint8_t size, int *height, int *offset);

// Small font (with letters) used for the AM/PM suffix drawn next to the main
// time. The LECO "numbers" time fonts have no A/M/P glyphs, so the suffix is
// rendered separately in this proportionally smaller font.
GFont ampm_font(int *height, int *offset);
