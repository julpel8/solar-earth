#include "earth.h"
#include "settings.h"
#include <math.h>

// The C side owns every displayed colour, so the resource compiler can never
// change what the watch shows by reordering a PNG palette. Day pixels keep
// palette indices 0..7 but their RGB values are overwritten below; night
// pixels are remapped into fixed upper-palette indices 8..11. The PNG palettes
// are used only to *classify* each pixel (ocean vs land, city vs dark).
#define N_DAY_COLOURS 8
#define NIGHT_IDX_OCEAN 8
#define NIGHT_IDX_LAND 9
#define NIGHT_IDX_CITY_DIM 10
#define NIGHT_IDX_CITY_BRIGHT 11

// Single source of truth for every displayed colour (RGB, quantised to
// Pebble's 64-colour space at runtime by GColorFromHEX). Vivid palette
// inspired by Joshua Simmons' Blue Pebble (#0055FF ocean, #00AA00 land) for
// strong contrast on the 64-colour display. Night land mirrors the day-land
// hue (dark green); cities are warm amber -> bright yellow so they read
// against the dark globe.
#define DAY_OCEAN_HEX 0x0055FF
#define DAY_LAND_HEX 0x00AA00
#define NIGHT_OCEAN_HEX 0x000055
#define NIGHT_LAND_HEX 0x005500
#define NIGHT_CITY_DIM_HEX 0x005500
#define NIGHT_CITY_BRIGHT_HEX 0xFFFF55

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define HALF_PI ((float)(M_PI / 2.0))
#define TWO_PI ((float)(M_PI * 2.0))
#define DEG2RAD ((float)(M_PI / 180.0))

static GBitmap *s_day, *s_night, *s_combined;
static uint8_t *s_day_data, *s_night_data, *s_comb_data;
static unsigned int s_bpr, s_rows, s_cols;
static int s_lat0 = 0, s_lon0 = 0;
static uint8_t s_day_to_night_base[16];
static uint8_t s_night_to_city[16];

typedef struct {
  bool active;
  unsigned int next_row;
  float sun_x;
  float sun_y;
  float sun_z;
} EarthUpdateState;

static EarthUpdateState s_update;

typedef struct {
  uint32_t day_res, night_res;
  int lat, lon;
} RegionDef;

static const RegionDef REGIONS[EARTH_NUM_REGIONS] = {
    {RESOURCE_ID_BLUE_MARBLE_EUROPE, RESOURCE_ID_BLACK_MARBLE_EUROPE, 50, 10},
    {RESOURCE_ID_BLUE_MARBLE_AFRICA, RESOURCE_ID_BLACK_MARBLE_AFRICA, 2, 20},
    {RESOURCE_ID_BLUE_MARBLE_NAMERICA, RESOURCE_ID_BLACK_MARBLE_NAMERICA, 45, -100},
    {RESOURCE_ID_BLUE_MARBLE_SAMERICA, RESOURCE_ID_BLACK_MARBLE_SAMERICA, -20, -60},
    {RESOURCE_ID_BLUE_MARBLE_ASIA, RESOURCE_ID_BLACK_MARBLE_ASIA, 35, 100},
    {RESOURCE_ID_BLUE_MARBLE_OCEANIA, RESOURCE_ID_BLACK_MARBLE_OCEANIA, -25, 135},
};

static float fast_sqrtf(float x) {
  if (x <= 0.0f) return 0.0f;

  // Avoid the SDK libm sqrtf path, which faults on-device for this app.
  union {
    float f;
    uint32_t i;
  } conv;

  float half = 0.5f * x;
  conv.f = x;
  conv.i = 0x5f3759df - (conv.i >> 1);
  float inv = conv.f;
  inv = inv * (1.5f - half * inv * inv);
  inv = inv * (1.5f - half * inv * inv);
  return x * inv;
}

static float wrap_pi(float x) {
  while (x > (float)M_PI) x -= TWO_PI;
  while (x < -(float)M_PI) x += TWO_PI;
  return x;
}

static float fast_sinf(float x) {
  x = wrap_pi(x);
  if (x > HALF_PI) {
    x = (float)M_PI - x;
  } else if (x < -HALF_PI) {
    x = -(float)M_PI - x;
  }

  float x2 = x * x;
  return x * (1.0f - x2 / 6.0f + (x2 * x2) / 120.0f);
}

static float fast_cosf(float x) { return fast_sinf(x + HALF_PI); }

// 4-bit (16-entry palette) pixel packing: two pixels per byte, sub 0 = high
// nibble, sub 1 = low nibble.
static uint8_t pixel4_get(uint8_t byte, int sub) {
  return sub == 0 ? (byte >> 4) & 0x0F : byte & 0x0F;
}

static uint8_t pixel4_set(uint8_t byte, int sub, uint8_t index) {
  if (sub == 0) return (uint8_t)((byte & 0x0F) | ((index & 0x0F) << 4));
  return (uint8_t)((byte & 0xF0) | (index & 0x0F));
}

static bool is_day_ocean_color(GColor color) {
  return color.a > 0 && color.b > color.g && color.b > color.r;
}

static uint8_t city_index_for_night_color(GColor color) {
  if (color.a == 0) return 0;
  if (color.r >= 3 && color.g >= 3) return NIGHT_IDX_CITY_BRIGHT;
  if (color.r >= 3 && color.g >= 2) return NIGHT_IDX_CITY_DIM;
  return 0;
}

static void prepare_palette(GColor *day_pal, GColor *night_pal) {
  day_pal[NIGHT_IDX_OCEAN] = GColorFromHEX(NIGHT_OCEAN_HEX);
  day_pal[NIGHT_IDX_LAND] = GColorFromHEX(NIGHT_LAND_HEX);
  day_pal[NIGHT_IDX_CITY_DIM] = GColorFromHEX(NIGHT_CITY_DIM_HEX);
  day_pal[NIGHT_IDX_CITY_BRIGHT] = GColorFromHEX(NIGHT_CITY_BRIGHT_HEX);

  for (int i = 0; i < 16; i++) {
    s_day_to_night_base[i] = NIGHT_IDX_OCEAN;
    s_night_to_city[i] = 0;
  }

  for (int i = 0; i < N_DAY_COLOURS; i++) {
    bool ocean = is_day_ocean_color(day_pal[i]);
    s_day_to_night_base[i] = ocean ? NIGHT_IDX_OCEAN : NIGHT_IDX_LAND;
    s_night_to_city[i] = city_index_for_night_color(night_pal[i]);
    // C has the final say on day colours: overwrite whatever the resource
    // compiler emitted, but leave the disc's transparent background alone.
    if (day_pal[i].a != 0) {
      day_pal[i] =
          ocean ? GColorFromHEX(DAY_OCEAN_HEX) : GColorFromHEX(DAY_LAND_HEX);
    }
  }
}

static uint8_t night_index_for_pixel(uint8_t day_index, uint8_t night_index) {
  uint8_t city = s_night_to_city[night_index & 0x0F];
  if (city) return city;
  return s_day_to_night_base[day_index & 0x0F];
}

static void earth_destroy(void);

static GBitmap *load_region(uint8_t region) {
  if (region >= EARTH_NUM_REGIONS) region = 0;
  earth_destroy();  // free any previous globe first
  s_update.active = false;
  const RegionDef *r = &REGIONS[region];
  s_day = gbitmap_create_with_resource(r->day_res);
  s_night = gbitmap_create_with_resource(r->night_res);
  if (!s_day || !s_night) return NULL;
  s_lat0 = r->lat;
  s_lon0 = r->lon;

  GColor *day_pal = gbitmap_get_palette(s_day);
  GColor *night_pal = gbitmap_get_palette(s_night);
  prepare_palette(day_pal, night_pal);

  GRect b = gbitmap_get_bounds(s_day);
  s_bpr = gbitmap_get_bytes_per_row(s_day);
  s_rows = b.size.h;
  s_cols = b.size.w;

  s_combined = gbitmap_create_blank(b.size, gbitmap_get_format(s_day));
  s_comb_data = gbitmap_get_data(s_combined);
  s_day_data = gbitmap_get_data(s_day);
  s_night_data = gbitmap_get_data(s_night);
  memcpy(s_comb_data, s_day_data, s_bpr * s_rows);
  gbitmap_set_palette(s_combined, day_pal, false);
  return s_combined;
}

static GBitmap *earth_init(uint8_t region) { return load_region(region); }
static GBitmap *earth_set_region(uint8_t region) { return load_region(region); }

static void earth_start_update(time_t utc_now) {
  if (!s_combined) return;
  struct tm utc = *gmtime(&utc_now);

  // Subsolar point (where the sun is directly overhead).
  int doy = utc.tm_yday;
  float decl =
      0.40928f * fast_sinf(TWO_PI * (float)(doy - 80) / 365.0f);
  float utch = utc.tm_hour + utc.tm_min / 60.0f;
  float sublon = -15.0f * (utch - 12.0f) * DEG2RAD;
  float Sx = fast_cosf(decl) * fast_cosf(sublon);
  float Sy = fast_cosf(decl) * fast_sinf(sublon);
  float Sz = fast_sinf(decl);

  // View basis at the globe centre: east, north(up), outward(toward viewer).
  float la = s_lat0 * DEG2RAD, lo = s_lon0 * DEG2RAD;
  float fwdx = fast_cosf(la) * fast_cosf(lo);
  float fwdy = fast_cosf(la) * fast_sinf(lo);
  float fwdz = fast_sinf(la);
  float upx = -fast_sinf(la) * fast_cosf(lo);
  float upy = -fast_sinf(la) * fast_sinf(lo);
  float upz = fast_cosf(la);
  float eax = -fast_sinf(lo), eay = fast_cosf(lo);
  // Sun direction expressed in the disc frame (x=east, y=up, z=toward viewer).
  s_update.sun_x = Sx * eax + Sy * eay;
  s_update.sun_y = Sx * upx + Sy * upy + Sz * upz;
  s_update.sun_z = Sx * fwdx + Sy * fwdy + Sz * fwdz;
  s_update.next_row = 0;
  s_update.active = true;
}

static bool earth_update_is_active(void) { return s_update.active; }

static bool earth_update_step(unsigned int max_rows) {
  if (!s_combined || !s_update.active) return true;
  if (max_rows == 0) max_rows = 1;

  float R = s_cols / 2.0f;
  float cx = (s_cols - 1) / 2.0f;
  float cy = (s_rows - 1) / 2.0f;
  float invR = 1.0f / R;

  unsigned int end_row = s_update.next_row + max_rows;
  if (end_row > s_rows) end_row = s_rows;

  for (unsigned int row = s_update.next_row; row < end_row; row++) {
    float fy = (cy - (float)row) * invR;
    unsigned int rb = row * s_bpr;
    for (unsigned int byte = 0; byte < s_bpr; byte++) {
      uint8_t day_byte = s_day_data[rb + byte];
      uint8_t out = day_byte;                 // default: day pixels
      uint8_t night_byte = s_night_data[rb + byte];
      for (int sub = 0; sub < 2; sub++) {
        unsigned int col = byte * 2 + sub;
        if (col >= s_cols) break;
        float fx = ((float)col - cx) * invR;
        float r2 = fx * fx + fy * fy;
        if (r2 > 1.0f) continue;             // outside the globe -> keep (transparent)
        float z = fast_sqrtf(1.0f - r2);
        float dot = fx * s_update.sun_x + fy * s_update.sun_y +
                    z * s_update.sun_z;
        if (dot <= 0.0f) {                    // night side
          uint8_t di = pixel4_get(day_byte, sub);
          uint8_t ni = pixel4_get(night_byte, sub);
          out = pixel4_set(out, sub, night_index_for_pixel(di, ni));
        }
      }
      s_comb_data[rb + byte] = out;
    }
  }

  s_update.next_row = end_row;
  if (s_update.next_row >= s_rows) {
    s_update.active = false;
    return true;
  }

  return false;
}

static void earth_destroy(void) {
  s_update.active = false;
  if (s_combined) { gbitmap_destroy(s_combined); s_combined = NULL; }
  if (s_day) { gbitmap_destroy(s_day); s_day = NULL; }
  if (s_night) { gbitmap_destroy(s_night); s_night = NULL; }
}

// ===========================================================================
// Globe presentation: BitmapLayer, scaled display bitmap and the incremental
// day/night shading timer. Builds on the globe model above.
// ===========================================================================

// The day/night refresh cadence is user-configurable; see
// settings_earth_update_seconds(). Slicing the work keeps the UI responsive.
#define EARTH_UPDATE_SLICE_DELAY_MS 15
#define EARTH_UPDATE_ROWS_PER_SLICE 3

// Globe display size: scaled (preserving aspect ratio) to span the centre frame
// width minus a small side margin so the globe never bleeds to the very edge.
#define EARTH_SIDE_MARGIN 3

static BitmapLayer *s_earth_layer;   // globe disc, between centre bg and the time
static GBitmap *s_earth_bitmap;      // source globe (the combined model bitmap)
static GBitmap *s_earth_display_bitmap;  // scaled copy (owned here)
static GSize s_earth_display_size = {0, 0};
static bool s_earth_display_dirty = true;
static uint8_t s_loaded_earth_region = 0xFF;
static GRect s_center_frame;

static AppTimer *s_earth_update_timer = NULL;
static time_t s_last_earth_update_time = 0;

static GSize compute_earth_display_size(GRect centerFrame) {
  GSize src = gbitmap_get_bounds(s_earth_bitmap).size;
  if (src.w <= 0) return src;
  int16_t targetW = centerFrame.size.w - 2 * EARTH_SIDE_MARGIN;
  if (targetW < 1) targetW = centerFrame.size.w;
  GSize displaySize = {targetW,
                       (int16_t)((int32_t)targetW * src.h / src.w)};
  return displaySize;
}

// Centre the globe bitmap inside the given centre frame.
static void position_earth_layer(GRect centerFrame) {
  if (!s_earth_layer || !s_earth_bitmap) return;
  GSize displaySize = compute_earth_display_size(centerFrame);

  int ex = centerFrame.origin.x + (centerFrame.size.w - displaySize.w) / 2;
  int ey = centerFrame.origin.y + (centerFrame.size.h - displaySize.h) / 2;
  layer_set_frame(bitmap_layer_get_layer(s_earth_layer),
                  GRect(ex, ey, displaySize.w, displaySize.h));
}

static uint8_t get_4bit_pixel(const uint8_t *data, uint16_t bytesPerRow,
                              int x, int y) {
  return pixel4_get(data[y * bytesPerRow + x / 2], x % 2);
}

static void set_4bit_pixel(uint8_t *data, uint16_t bytesPerRow, int x, int y,
                           uint8_t value) {
  uint8_t *packed = &data[y * bytesPerRow + x / 2];
  *packed = pixel4_set(*packed, x % 2, value);
}

static void destroy_earth_display_bitmap(void) {
  if (s_earth_display_bitmap) {
    gbitmap_destroy(s_earth_display_bitmap);
    s_earth_display_bitmap = NULL;
  }
  s_earth_display_size = GSize(0, 0);
  s_earth_display_dirty = true;
}

static GBitmap *get_earth_display_bitmap(GSize displaySize) {
  if (!s_earth_bitmap) return NULL;

  GRect sourceBounds = gbitmap_get_bounds(s_earth_bitmap);
  if (displaySize.w == sourceBounds.size.w &&
      displaySize.h == sourceBounds.size.h) {
    destroy_earth_display_bitmap();  // scaled copy no longer needed
    return s_earth_bitmap;
  }

  if (gbitmap_get_format(s_earth_bitmap) != GBitmapFormat4BitPalette) {
    return s_earth_bitmap;
  }

  bool sizeChanged = !s_earth_display_bitmap ||
                     s_earth_display_size.w != displaySize.w ||
                     s_earth_display_size.h != displaySize.h;
  if (sizeChanged) {
    destroy_earth_display_bitmap();
    s_earth_display_bitmap =
        gbitmap_create_blank(displaySize, gbitmap_get_format(s_earth_bitmap));
    if (!s_earth_display_bitmap) return s_earth_bitmap;

    GColor *palette = gbitmap_get_palette(s_earth_bitmap);
    if (palette) gbitmap_set_palette(s_earth_display_bitmap, palette, false);
    s_earth_display_size = displaySize;
    s_earth_display_dirty = true;
  }

  if (s_earth_display_dirty && s_earth_display_bitmap) {
    const uint8_t *sourceData = gbitmap_get_data(s_earth_bitmap);
    uint8_t *displayData = gbitmap_get_data(s_earth_display_bitmap);
    uint16_t sourceBytesPerRow = gbitmap_get_bytes_per_row(s_earth_bitmap);
    uint16_t displayBytesPerRow =
        gbitmap_get_bytes_per_row(s_earth_display_bitmap);

    memset(displayData, 0, displayBytesPerRow * displaySize.h);
    for (int y = 0; y < displaySize.h; y++) {
      int sourceY = (int32_t)y * sourceBounds.size.h / displaySize.h;
      for (int x = 0; x < displaySize.w; x++) {
        int sourceX = (int32_t)x * sourceBounds.size.w / displaySize.w;
        uint8_t pixel =
            get_4bit_pixel(sourceData, sourceBytesPerRow, sourceX, sourceY);
        set_4bit_pixel(displayData, displayBytesPerRow, x, y, pixel);
      }
    }
    s_earth_display_dirty = false;
  }

  return s_earth_display_bitmap ? s_earth_display_bitmap : s_earth_bitmap;
}

static void refresh_earth_layer(GRect centerFrame) {
  if (!s_earth_layer || !s_earth_bitmap) return;
  GSize displaySize = compute_earth_display_size(centerFrame);

  GBitmap *displayBitmap = get_earth_display_bitmap(displaySize);
  bitmap_layer_set_bitmap(s_earth_layer, displayBitmap);
  position_earth_layer(centerFrame);
  layer_mark_dirty(bitmap_layer_get_layer(s_earth_layer));
}

static void earth_update_timer_callback(void *data);

static void cancel_earth_update_timer(void) {
  if (s_earth_update_timer) {
    app_timer_cancel(s_earth_update_timer);
    s_earth_update_timer = NULL;
  }
}

static void schedule_earth_update_slice(uint32_t delay_ms) {
  s_earth_update_timer =
      app_timer_register(delay_ms, earth_update_timer_callback, NULL);
}

static void start_earth_update_async(time_t now, bool force) {
  if (!s_earth_bitmap || !s_earth_layer) return;
  if (earth_update_is_active() && !force) return;

  cancel_earth_update_timer();
  earth_start_update(now);
  schedule_earth_update_slice(1);
}

static void earth_update_timer_callback(void *data) {
  s_earth_update_timer = NULL;
  bool done = earth_update_step(EARTH_UPDATE_ROWS_PER_SLICE);
  if (done) {
    s_last_earth_update_time = time(NULL);
    s_earth_display_dirty = true;
    refresh_earth_layer(s_center_frame);
    APP_LOG(APP_LOG_LEVEL_INFO, "Earth shading update complete");
  } else {
    schedule_earth_update_slice(EARTH_UPDATE_SLICE_DELAY_MS);
  }
}

void earth_render_init(Layer *parent, GRect frame, GColor bg, uint8_t region) {
  s_earth_bitmap = earth_init(region);
  s_loaded_earth_region = region;
  s_center_frame = frame;

  s_earth_layer = bitmap_layer_create(frame);
  bitmap_layer_set_compositing_mode(s_earth_layer, GCompOpSet);
  bitmap_layer_set_background_color(s_earth_layer, bg);
  layer_add_child(parent, bitmap_layer_get_layer(s_earth_layer));

  refresh_earth_layer(frame);
}

void earth_render_deinit(void) {
  cancel_earth_update_timer();
  if (s_earth_layer) {
    bitmap_layer_destroy(s_earth_layer);
    s_earth_layer = NULL;
  }
  destroy_earth_display_bitmap();
  earth_destroy();
  s_earth_bitmap = NULL;
}

void earth_render_reposition(GRect frame) {
  s_center_frame = frame;
  refresh_earth_layer(frame);
}

void earth_render_set_bg(GColor bg) {
  if (s_earth_layer) {
    bitmap_layer_set_background_color(s_earth_layer, bg);
  }
}

bool earth_render_set_region(uint8_t region) {
  bool regionChanged = !s_earth_bitmap || s_loaded_earth_region != region;
  if (!regionChanged) return false;

  cancel_earth_update_timer();
  destroy_earth_display_bitmap();
  s_earth_bitmap = earth_set_region(region);
  s_loaded_earth_region = region;
  s_last_earth_update_time = 0;
  return true;
}

void earth_render_maybe_update(time_t now) {
  if (!s_earth_bitmap || earth_update_is_active()) return;
  if (s_last_earth_update_time == 0 ||
      now - s_last_earth_update_time >= settings_earth_update_seconds()) {
    start_earth_update_async(now, false);
  }
}

void earth_render_force_update(time_t now) {
  start_earth_update_async(now, true);
}

// TEMP DEBUG: see earth.h.
int32_t earth_render_seconds_until_update(time_t now) {
  if (s_last_earth_update_time == 0) return 0;
  return (int32_t)(s_last_earth_update_time + settings_earth_update_seconds() -
                   now);
}
