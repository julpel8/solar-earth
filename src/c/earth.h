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

// Load a region's globe (day+night) and build the combined bitmap to display.
// Returns the combined GBitmap (owned here). earth_init for first load,
// earth_set_region to switch (frees the previous one first).
GBitmap *earth_init(uint8_t region);
GBitmap *earth_set_region(uint8_t region);

// Recompute day/night shading (terminator) for the given UTC epoch time.
void earth_update(time_t utc_now);

// Incremental update API for callers that must not block the UI thread.
void earth_start_update(time_t utc_now);
bool earth_update_step(unsigned int max_rows);
bool earth_update_is_active(void);

void earth_destroy(void);
