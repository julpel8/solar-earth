#include "earth.h"
#include <math.h>

// Day globes use up to 8 palette colours (indices 0..7); the night palette is
// stacked into indices 8..15 of the same 4-bit (16-entry) palette, so a pixel
// is shown "at night" simply by adding 8 to its colour index.
#define N_DAY_COLOURS 8

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define DEG2RAD ((float)(M_PI / 180.0))

static GBitmap *s_day, *s_night, *s_combined;
static uint8_t *s_day_data, *s_night_data, *s_comb_data;
static unsigned int s_bpr, s_rows, s_cols;
static int s_lat0 = 0, s_lon0 = 0;

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

  // Stack the night palette into the upper half of the day palette.
  GColor *day_pal = gbitmap_get_palette(s_day);
  GColor *night_pal = gbitmap_get_palette(s_night);
  for (int i = 0; i < N_DAY_COLOURS; i++) {
    day_pal[i + N_DAY_COLOURS] = night_pal[i];
  }

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

GBitmap *earth_init(uint8_t region) { return load_region(region); }
GBitmap *earth_set_region(uint8_t region) { return load_region(region); }

void earth_start_update(time_t utc_now) {
  if (!s_combined) return;
  struct tm utc = *gmtime(&utc_now);

  // Subsolar point (where the sun is directly overhead).
  int doy = utc.tm_yday;
  float decl = 0.40928f * sinf(2.0f * (float)M_PI * (float)(doy - 80) / 365.0f);
  float utch = utc.tm_hour + utc.tm_min / 60.0f;
  float sublon = -15.0f * (utch - 12.0f) * DEG2RAD;
  float Sx = cosf(decl) * cosf(sublon);
  float Sy = cosf(decl) * sinf(sublon);
  float Sz = sinf(decl);

  // View basis at the globe centre: east, north(up), outward(toward viewer).
  float la = s_lat0 * DEG2RAD, lo = s_lon0 * DEG2RAD;
  float fwdx = cosf(la) * cosf(lo), fwdy = cosf(la) * sinf(lo), fwdz = sinf(la);
  float upx = -sinf(la) * cosf(lo), upy = -sinf(la) * sinf(lo), upz = cosf(la);
  float eax = -sinf(lo), eay = cosf(lo);
  // Sun direction expressed in the disc frame (x=east, y=up, z=toward viewer).
  s_update.sun_x = Sx * eax + Sy * eay;
  s_update.sun_y = Sx * upx + Sy * upy + Sz * upz;
  s_update.sun_z = Sx * fwdx + Sy * fwdy + Sz * fwdz;
  s_update.next_row = 0;
  s_update.active = true;
}

bool earth_update_is_active(void) { return s_update.active; }

bool earth_update_step(unsigned int max_rows) {
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
      uint8_t out = s_day_data[rb + byte];   // default: day pixels
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
          if (sub == 0) {
            uint8_t ni = (uint8_t)(((night_byte >> 4) & 0x0F) + N_DAY_COLOURS);
            out = (uint8_t)((out & 0x0F) | ((ni & 0x0F) << 4));
          } else {
            uint8_t ni = (uint8_t)((night_byte & 0x0F) + N_DAY_COLOURS);
            out = (uint8_t)((out & 0xF0) | (ni & 0x0F));
          }
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

void earth_update(time_t utc_now) {
  earth_start_update(utc_now);
  while (!earth_update_step(s_rows)) {
  }
}

void earth_destroy(void) {
  s_update.active = false;
  if (s_combined) { gbitmap_destroy(s_combined); s_combined = NULL; }
  if (s_day) { gbitmap_destroy(s_day); s_day = NULL; }
  if (s_night) { gbitmap_destroy(s_night); s_night = NULL; }
}
