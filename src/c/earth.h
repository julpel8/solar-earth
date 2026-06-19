#pragma once
#include <pebble.h>

#define EARTH_NUM_REGIONS 6

// Region order — must match the Clay config / settings values.
typedef enum {
  EARTH_EUROPE = 0,
  EARTH_AFRICA,
  EARTH_NAMERICA,
  EARTH_SAMERICA,
  EARTH_ASIA,
  EARTH_OCEANIA,
} EarthRegion;

// Visual presentation of the globe: owns the BitmapLayer, the scaled display
// bitmap and the incremental day/night shading timer. The globe model (palette
// + terminator maths) is internal to earth.c.

// Load `region`, create the globe BitmapLayer as a child of `parent`, and run
// the first scale/position pass inside `frame`.
void earth_render_init(Layer *parent, GRect frame, GColor bg, uint8_t region);

// Cancel the shading timer and destroy the layer, scaled bitmap and model.
void earth_render_deinit(void);

// Re-scale and re-centre the globe inside `frame` (call on layout changes).
void earth_render_reposition(GRect frame);

// Background colour shown through the transparent disc edges.
void earth_render_set_bg(GColor bg);

// Switch the displayed region if it differs from the current one. Returns true
// when a switch actually happened (caller may want to reposition / re-shade).
bool earth_render_set_region(uint8_t region);

// Recompute the day/night shading if the refresh interval has elapsed.
void earth_render_maybe_update(time_t now);

// Force an immediate (async) day/night shading recompute.
void earth_render_force_update(time_t now);
