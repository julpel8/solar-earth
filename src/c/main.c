#include <pebble.h>

// #define FORCE_BACKLIGHT

#include "drawUtils.h"
#include "earth.h"
#include "messaging.h"
#include "settings.h"
#include "solarUtils.h"
#include "text_metrics.h"
#include "utils.h"
#include "widgets.h"

#define FORCE_12H false
#define TIME_STR_LEN 6

// Heartbeat timer: the watch requests data from the phone on this interval
#define UPDATE_REQUEST_INTERVAL_MS (30 * 60 * 1000) // 30 minutes
#define UPDATE_REQUEST_INITIAL_DELAY_MS (3 * 1000)   // 3 seconds after startup
#define UPDATE_REQUEST_RETRY_MS (30 * 1000)          // fast retry after outbox fail

#define EARTH_UPDATE_INTERVAL_SECONDS (5 * 60)
#define EARTH_UPDATE_SLICE_DELAY_MS 15
#define EARTH_UPDATE_ROWS_PER_SLICE 3

#define INFO_SLOT_UPPER_SECONDARY 0
#define INFO_SLOT_UPPER_PRIMARY 1
#define INFO_SLOT_TIME 2
#define INFO_SLOT_LOWER_PRIMARY 3
#define INFO_SLOT_LOWER_SECONDARY 4
#define INFO_SLOT_COUNT 5

#define INFO_GROUP_TOP 0
#define INFO_GROUP_CENTER 1
#define INFO_GROUP_BOTTOM 2

// windows and layers
static Window *mainWindow;
static Layer *windowLayer;
static Layer *shiftingLayer;
static Layer *centerLayer;
static Layer *ringLayer;
static Layer *infoLayer;
static BitmapLayer *earthLayer;   // globe disc, between centre bg and the time
static GBitmap *earthBitmap;
static GBitmap *earthDisplayBitmap;
static GSize s_earth_display_size = {0, 0};
static bool s_earth_display_dirty = true;
static uint8_t s_loaded_earth_region = 0xFF;
static bool s_loaded_show_solar_ring = true;
static bool s_quick_view_visible = false;

static GRect get_center_frame(GRect bounds) {
  int inset = globalSettings.showSolarRing ? EDGE_THICKNESS : 0;
  return GRect(bounds.origin.x + inset, bounds.origin.y + inset,
               bounds.size.w - inset * 2, bounds.size.h - inset * 2);
}

// Globe display size: native bitmap size when the ring is shown, else scaled
// up (preserving aspect ratio) to span the full centre frame width.
static GSize compute_earth_display_size(GRect centerFrame) {
  GSize src = gbitmap_get_bounds(earthBitmap).size;
  if (globalSettings.showSolarRing || src.w <= 0) return src;
  GSize displaySize = {centerFrame.size.w,
                       (int16_t)((int32_t)centerFrame.size.w * src.h / src.w)};
  return displaySize;
}

// Centre the globe bitmap inside the given centre frame.
static void position_earth_layer(GRect centerFrame) {
  if (!earthLayer || !earthBitmap) return;
  GSize displaySize = compute_earth_display_size(centerFrame);

  int ex = centerFrame.origin.x + (centerFrame.size.w - displaySize.w) / 2;
  int ey = centerFrame.origin.y + (centerFrame.size.h - displaySize.h) / 2;
  layer_set_frame(bitmap_layer_get_layer(earthLayer),
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
  if (earthDisplayBitmap) {
    gbitmap_destroy(earthDisplayBitmap);
    earthDisplayBitmap = NULL;
  }
  s_earth_display_size = GSize(0, 0);
  s_earth_display_dirty = true;
}

static GBitmap *get_earth_display_bitmap(GSize displaySize) {
  if (!earthBitmap) return NULL;

  GRect sourceBounds = gbitmap_get_bounds(earthBitmap);
  if (globalSettings.showSolarRing ||
      (displaySize.w == sourceBounds.size.w &&
       displaySize.h == sourceBounds.size.h)) {
    destroy_earth_display_bitmap();  // scaled copy no longer needed
    return earthBitmap;
  }

  if (gbitmap_get_format(earthBitmap) != GBitmapFormat4BitPalette) {
    return earthBitmap;
  }

  bool sizeChanged = !earthDisplayBitmap ||
                     s_earth_display_size.w != displaySize.w ||
                     s_earth_display_size.h != displaySize.h;
  if (sizeChanged) {
    destroy_earth_display_bitmap();
    earthDisplayBitmap =
        gbitmap_create_blank(displaySize, gbitmap_get_format(earthBitmap));
    if (!earthDisplayBitmap) return earthBitmap;

    GColor *palette = gbitmap_get_palette(earthBitmap);
    if (palette) gbitmap_set_palette(earthDisplayBitmap, palette, false);
    s_earth_display_size = displaySize;
    s_earth_display_dirty = true;
  }

  if (s_earth_display_dirty && earthDisplayBitmap) {
    const uint8_t *sourceData = gbitmap_get_data(earthBitmap);
    uint8_t *displayData = gbitmap_get_data(earthDisplayBitmap);
    uint16_t sourceBytesPerRow = gbitmap_get_bytes_per_row(earthBitmap);
    uint16_t displayBytesPerRow = gbitmap_get_bytes_per_row(earthDisplayBitmap);

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

  return earthDisplayBitmap ? earthDisplayBitmap : earthBitmap;
}

static void refresh_earth_layer(GRect centerFrame) {
  if (!earthLayer || !earthBitmap) return;
  GSize displaySize = compute_earth_display_size(centerFrame);

  GBitmap *displayBitmap = get_earth_display_bitmap(displaySize);
  bitmap_layer_set_bitmap(earthLayer, displayBitmap);
  position_earth_layer(centerFrame);
  layer_mark_dirty(bitmap_layer_get_layer(earthLayer));
}

// Time string (populated each tick)
static char timeText[TIME_STR_LEN];

// Heartbeat timer for requesting data updates from the phone
static AppTimer *s_update_request_timer = NULL;
static AppTimer *s_earth_update_timer = NULL;
static time_t s_last_earth_update_time = 0;

// ============================================================
// Slot descriptor — one per visible row in the layout
// ============================================================
typedef struct {
  const char *text;
  GFont font;
  int height; // true pixel height from metrics
  int offset; // top dead-space offset from metrics
  GColor color;
  uint8_t group;
} SlotDescriptor;

typedef struct {
  uint8_t id;
  uint8_t group;
} InfoLayoutItem;

// Buffer storage for each of the 4 widget slots. Populated by
// update_widget_text() on the minute tick / settings change, then read by
// draw_center_text() on every redraw — so redraws stay allocation-free.
static char widgetTextUS[WIDGET_TEXT_LEN]; // upper secondary
static char widgetTextUP[WIDGET_TEXT_LEN]; // upper primary
static char widgetTextLP[WIDGET_TEXT_LEN]; // lower primary
static char widgetTextLS[WIDGET_TEXT_LEN]; // lower secondary

static void update_widget_text(void) {
  if (globalSettings.widgetUpperSecondary[0] != '\0') {
    widget_get_text(globalSettings.widgetUpperSecondary, widgetTextUS,
                    WIDGET_TEXT_LEN);
  } else {
    widgetTextUS[0] = '\0';
  }
  if (globalSettings.widgetUpperPrimary[0] != '\0') {
    widget_get_text(globalSettings.widgetUpperPrimary, widgetTextUP,
                    WIDGET_TEXT_LEN);
  } else {
    widgetTextUP[0] = '\0';
  }
  if (globalSettings.widgetLowerPrimary[0] != '\0') {
    widget_get_text(globalSettings.widgetLowerPrimary, widgetTextLP,
                    WIDGET_TEXT_LEN);
  } else {
    widgetTextLP[0] = '\0';
  }
  if (globalSettings.widgetLowerSecondary[0] != '\0') {
    widget_get_text(globalSettings.widgetLowerSecondary, widgetTextLS,
                    WIDGET_TEXT_LEN);
  } else {
    widgetTextLS[0] = '\0';
  }
}

static bool parse_info_layout(const char *layout, InfoLayoutItem *items,
                              int *count) {
  if (!layout || !layout[0]) return false;

  bool seen[INFO_SLOT_COUNT] = {false};
  bool has_time = false;
  int n = 0;
  const char *p = layout;

  while (*p) {
    if (n >= INFO_SLOT_COUNT) return false;
    if (p[0] < '0' || p[0] > '4' || p[1] != ':' || p[2] < '0' ||
        p[2] > '2') {
      return false;
    }

    uint8_t id = (uint8_t)(p[0] - '0');
    uint8_t group = (uint8_t)(p[2] - '0');
    if (seen[id]) return false;
    seen[id] = true;
    if (id == INFO_SLOT_TIME) has_time = true;

    items[n].id = id;
    items[n].group = group;
    n++;
    p += 3;

    if (*p == ',') {
      p++;
    } else if (*p != '\0') {
      return false;
    }
  }

  if (!has_time || n == 0) return false;
  *count = n;
  return true;
}

static int get_info_layout(InfoLayoutItem *items) {
  int count = 0;
  if (parse_info_layout(globalSettings.infoLayout, items, &count)) {
    return count;
  }

  parse_info_layout(DEFAULT_INFO_LAYOUT, items, &count);
  return count;
}

static int slot_group_height(SlotDescriptor *slots, int count, uint8_t group) {
  int height = 0;
  int visible = 0;
  for (int i = 0; i < count; i++) {
    if (slots[i].group != group) continue;
    if (visible > 0) height += LINE_PADDING;
    height += slots[i].height;
    visible++;
  }
  return height;
}

static void draw_text_with_halo(GContext *ctx, const char *text, GFont font,
                                GRect frame, GColor color);

static void draw_slot_group(GContext *ctx, SlotDescriptor *slots, int count,
                            uint8_t group, int y, int width) {
  for (int i = 0; i < count; i++) {
    SlotDescriptor *s = &slots[i];
    if (s->group != group) continue;
    draw_text_with_halo(ctx, s->text, s->font,
                        GRect(0, y - s->offset, width, s->height), s->color);
    y += s->height + LINE_PADDING;
  }
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
  if (!earthBitmap || !earthLayer) return;
  if (earth_update_is_active() && !force) return;

  cancel_earth_update_timer();
  earth_start_update(now);
  schedule_earth_update_slice(1);
}

static void maybe_update_earth_async(time_t now) {
  if (!earthBitmap || earth_update_is_active()) return;
  if (s_last_earth_update_time == 0 ||
      now - s_last_earth_update_time >= EARTH_UPDATE_INTERVAL_SECONDS) {
    start_earth_update_async(now, false);
  }
}

static void earth_update_timer_callback(void *data) {
  s_earth_update_timer = NULL;
  bool done = earth_update_step(EARTH_UPDATE_ROWS_PER_SLICE);
  if (done) {
    s_last_earth_update_time = time(NULL);
    s_earth_display_dirty = true;
    if (centerLayer) {
      refresh_earth_layer(layer_get_frame(centerLayer));
    } else if (earthLayer) {
      layer_mark_dirty(bitmap_layer_get_layer(earthLayer));
    }
    APP_LOG(APP_LOG_LEVEL_INFO, "Earth shading update complete");
  } else {
    schedule_earth_update_slice(EARTH_UPDATE_SLICE_DELAY_MS);
  }
}

// Draw text with a fixed halo so it stays readable over the globe.
static void draw_text_with_halo(GContext *ctx, const char *text, GFont font,
                                GRect frame, GColor color) {
  (void)color;
  bool whiteText =
      globalSettings.textOutlineStyle == TEXT_OUTLINE_WHITE_WITH_BLACK;
  GColor halo = whiteText ? GColorBlack : GColorWhite;
  GColor text_color = whiteText ? GColorWhite : GColorBlack;
  static const int dx[] = {-1, 1, 0, 0, -1, -1, 1, 1};
  static const int dy[] = {0, 0, -1, 1, -1, 1, -1, 1};
  graphics_context_set_text_color(ctx, halo);
  for (int k = 0; k < 8; k++) {
    graphics_draw_text(ctx, text, font,
                       GRect(frame.origin.x + dx[k], frame.origin.y + dy[k],
                             frame.size.w, frame.size.h),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                       NULL);
  }
  graphics_context_set_text_color(ctx, text_color);
  graphics_draw_text(ctx, text, font, frame, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
}

static void draw_center_text(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  bool useLargeFont = globalSettings.useLargeFonts;

  bool useNightColors = false;
  if (globalSettings.useNightTheme) {
    struct tm *timeInfo = getCurrentTime();
    int currentMinutes = timeInfo->tm_hour * 60 + timeInfo->tm_min;
    useNightColors = isNightTime(currentMinutes);
  }

  // ---- Font selection ----
  GFont time_font = fonts_get_system_font(FONT_TIME);
  int time_height = FONT_TIME_HEIGHT;
  int time_offset = FONT_TIME_OFFSET;

  GFont primary_font = fonts_get_system_font(
      useLargeFont ? FONT_WIDGET_PRIMARY_LARGE : FONT_WIDGET_PRIMARY);
  int primary_height = useLargeFont ? FONT_WIDGET_PRIMARY_LARGE_HEIGHT
                                    : FONT_WIDGET_PRIMARY_HEIGHT;
  int primary_offset = useLargeFont ? FONT_WIDGET_PRIMARY_LARGE_OFFSET
                                    : FONT_WIDGET_PRIMARY_OFFSET;

  GFont secondary_font = fonts_get_system_font(
      useLargeFont ? FONT_WIDGET_SECONDARY_LARGE : FONT_WIDGET_SECONDARY);
  int secondary_height = useLargeFont ? FONT_WIDGET_SECONDARY_LARGE_HEIGHT
                                      : FONT_WIDGET_SECONDARY_HEIGHT;
  int secondary_offset = useLargeFont ? FONT_WIDGET_SECONDARY_LARGE_OFFSET
                                      : FONT_WIDGET_SECONDARY_OFFSET;

  // ---- Color selection ----
  GColor timeColor =
      useNightColors ? globalSettings.nightTimeColor : globalSettings.timeColor;
  GColor primaryColor = useNightColors ? globalSettings.nightSubtextPrimaryColor
                                       : globalSettings.subtextPrimaryColor;
  GColor secondaryColor = useNightColors
                              ? globalSettings.nightSubtextSecondaryColor
                              : globalSettings.subtextSecondaryColor;

  // ---- Build ordered slot list from the configurable layout ----
#define MAX_SLOTS INFO_SLOT_COUNT
  InfoLayoutItem layout_items[INFO_SLOT_COUNT];
  int layout_count = get_info_layout(layout_items);
  SlotDescriptor slots[INFO_SLOT_COUNT];
  int num_slots = 0;
  bool time_pushed = false;

// Helper macro to push a slot
#define PUSH_SLOT(txt, fnt, h, off, col, grp)                                  \
  do {                                                                         \
    if (num_slots < MAX_SLOTS) {                                               \
      slots[num_slots].text = (txt);                                           \
      slots[num_slots].font = (fnt);                                           \
      slots[num_slots].height = (h);                                           \
      slots[num_slots].offset = (off);                                         \
      slots[num_slots].color = (col);                                          \
      slots[num_slots].group = (grp);                                          \
      num_slots++;                                                             \
    }                                                                          \
  } while (0)

  for (int i = 0; i < layout_count; i++) {
    uint8_t group = layout_items[i].group;
    switch (layout_items[i].id) {
      case INFO_SLOT_UPPER_SECONDARY:
        if (!s_quick_view_visible && widgetTextUS[0] != '\0') {
          PUSH_SLOT(widgetTextUS,
                    globalSettings.usePrimaryFontForAllWidgets ? primary_font
                                                                : secondary_font,
                    globalSettings.usePrimaryFontForAllWidgets ? primary_height
                                                                : secondary_height,
                    globalSettings.usePrimaryFontForAllWidgets ? primary_offset
                                                                : secondary_offset,
                    secondaryColor, group);
        }
        break;
      case INFO_SLOT_UPPER_PRIMARY:
        if (widgetTextUP[0] != '\0') {
          PUSH_SLOT(widgetTextUP, primary_font, primary_height, primary_offset,
                    primaryColor, group);
        }
        break;
      case INFO_SLOT_TIME:
        PUSH_SLOT(timeText, time_font, time_height, time_offset, timeColor,
                  group);
        time_pushed = true;
        break;
      case INFO_SLOT_LOWER_PRIMARY:
        if (widgetTextLP[0] != '\0') {
          PUSH_SLOT(widgetTextLP, primary_font, primary_height, primary_offset,
                    primaryColor, group);
        }
        break;
      case INFO_SLOT_LOWER_SECONDARY:
        if (!s_quick_view_visible && widgetTextLS[0] != '\0') {
          PUSH_SLOT(widgetTextLS,
                    globalSettings.usePrimaryFontForAllWidgets ? primary_font
                                                                : secondary_font,
                    globalSettings.usePrimaryFontForAllWidgets ? primary_height
                                                                : secondary_height,
                    globalSettings.usePrimaryFontForAllWidgets ? primary_offset
                                                                : secondary_offset,
                    secondaryColor, group);
        }
        break;
    }
  }

  if (!time_pushed) {
    PUSH_SLOT(timeText, time_font, time_height, time_offset, timeColor,
              INFO_GROUP_CENTER);
  }

#undef PUSH_SLOT
#undef MAX_SLOTS

  int top_height = slot_group_height(slots, num_slots, INFO_GROUP_TOP);
  int center_height = slot_group_height(slots, num_slots, INFO_GROUP_CENTER);
  int bottom_height = slot_group_height(slots, num_slots, INFO_GROUP_BOTTOM);

  int top_y = 2;
  int bottom_y = bounds.size.h - bottom_height - 2;
  int center_top = top_y + top_height + (top_height > 0 ? LINE_PADDING : 0);
  int center_bottom = bottom_y - (bottom_height > 0 ? LINE_PADDING : 0);
  if (bottom_y < center_top) bottom_y = center_top;
  if (center_bottom < center_top) center_bottom = center_top;

  int center_area_height = center_bottom - center_top;
  int center_y = center_top;
  if (center_area_height > center_height) {
    center_y += (center_area_height - center_height) / 2;
  }

  draw_slot_group(ctx, slots, num_slots, INFO_GROUP_TOP, top_y, bounds.size.w);
  draw_slot_group(ctx, slots, num_slots, INFO_GROUP_CENTER, center_y,
                  bounds.size.w);
  draw_slot_group(ctx, slots, num_slots, INFO_GROUP_BOTTOM, bottom_y,
                  bounds.size.w);
}

// Resize literally everything on quick view
static void quickViewLayerReposition() {
  GRect full_bounds = layer_get_bounds(windowLayer);
  GRect bounds = layer_get_unobstructed_bounds(windowLayer);
  s_quick_view_visible = bounds.size.h < full_bounds.size.h;

  // Resize the shifting and center layer based on the current unobstructed
  // bounds
  layer_set_frame(shiftingLayer,
                  GRect(0, 0, full_bounds.size.w, bounds.size.h));
  layer_set_frame(ringLayer, GRect(0, 0, full_bounds.size.w, bounds.size.h));

  GRect centerFrame = get_center_frame(
      GRect(0, 0, full_bounds.size.w, bounds.size.h));
  layer_set_frame(centerLayer, centerFrame);

  layer_set_frame(infoLayer, GRect(0, centerFrame.origin.y, full_bounds.size.w,
                                   centerFrame.size.h));

  refresh_earth_layer(centerFrame);

  // Mark everything as dirty to redraw
  layer_mark_dirty(ringLayer);
  layer_mark_dirty(centerLayer);
  layer_mark_dirty(infoLayer);
}

static void update_clock() {
  Settings_updateDynamicSettings();

  time_t now = time(NULL);
  struct tm *timeInfo = getCurrentTime();

  // set time string
  if (clock_is_24h_style() && !FORCE_12H) {
    strftime(timeText, TIME_STR_LEN, "%H:%M", timeInfo);
  } else {
    strftime(timeText, TIME_STR_LEN, "%I:%M", timeInfo);
  }

  if (!globalSettings.showLeadingZero) {
    if (timeText[0] == '0') {
      for (int i = 0; i < TIME_STR_LEN - 1; i++) {
        timeText[i] = timeText[i + 1];
      }
    }
  }

  // Re-parse widget format strings into static buffers; the layer update
  // callback only reads them, so redraws (e.g. obstruction animations) don't
  // re-parse on every frame.
  update_widget_text();

  // ensure colors are updated based on settings
  ColorTheme currentTheme = getCurrentColorTheme();
  window_set_background_color(
      mainWindow, globalSettings.showSolarRing ? currentTheme.ringStrokeColor
                                               : currentTheme.bgColor);
  if (earthLayer) {
    bitmap_layer_set_background_color(earthLayer, currentTheme.bgColor);
  }

  // if sunrise/sunset has not yet been calculated, do that
  if (currentSolarInfo.sunriseMinute == DEFAULT_SUNRISE_TIME &&
      currentSolarInfo.sunsetMinute == DEFAULT_SUNSET_TIME) {
    solarUtils_recalculateSolarData();
  }

  // Recompute the globe day/night shading off the startup path. The trigonometry
  // is too expensive to run synchronously on the UI thread at launch.
  maybe_update_earth_async(now);

  // redraw solar ring layer
  layer_mark_dirty(ringLayer);
  layer_mark_dirty(centerLayer);
}

// settings might have changed, so recalculate solar data and refresh screen
void onSettingsChanged() {
  solarUtils_recalculateSolarData();

  bool regionChanged =
      !earthBitmap || s_loaded_earth_region != globalSettings.region;
  bool ringLayoutChanged =
      s_loaded_show_solar_ring != globalSettings.showSolarRing;
  s_loaded_show_solar_ring = globalSettings.showSolarRing;

  if (regionChanged) {
    cancel_earth_update_timer();
    destroy_earth_display_bitmap();
    earthBitmap = earth_set_region(globalSettings.region);
    s_loaded_earth_region = globalSettings.region;
    s_last_earth_update_time = 0;
  }

  if (earthLayer) {
    bitmap_layer_set_background_color(earthLayer, getCurrentColorTheme().bgColor);
  }

  if (windowLayer && (regionChanged || ringLayoutChanged)) {
    quickViewLayerReposition();
  }

  if (regionChanged) {
    start_earth_update_async(time(NULL), true);
  }

  update_clock();
}

// Event fires frequently, while obstruction is appearing or disappearing
static void onUnobstructedAreaChange(AnimationProgress progress,
                                     void *context) {
  quickViewLayerReposition();
}

// Event fires once, after obstruction appears or disappears
static void onUnobstructedAreaDidChange(void *context) {
  quickViewLayerReposition();
}

static void main_window_load(Window *window) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Main window load");

  // get information about the Window
  windowLayer = window_get_root_layer(window);
  ColorTheme currentTheme = getCurrentColorTheme();
  window_set_background_color(
      window, globalSettings.showSolarRing ? currentTheme.ringStrokeColor
                                           : currentTheme.bgColor);
  GRect bounds = layer_get_bounds(windowLayer);

  shiftingLayer = layer_create(bounds);
  layer_add_child(windowLayer, shiftingLayer);

  // create central rectangle
  GRect centerFrame = get_center_frame(bounds);
  centerLayer = layer_create(centerFrame);
  layer_set_update_proc(centerLayer, draw_center_layer);

  layer_add_child(shiftingLayer, centerLayer);

  // Earth globe — sits above the centre background, below the time text.
  earthBitmap = earth_init(globalSettings.region);
  s_loaded_earth_region = globalSettings.region;
  s_loaded_show_solar_ring = globalSettings.showSolarRing;
  earthLayer = bitmap_layer_create(centerFrame);
  bitmap_layer_set_compositing_mode(earthLayer, GCompOpSet);
  bitmap_layer_set_background_color(earthLayer, currentTheme.bgColor);
  layer_add_child(shiftingLayer, bitmap_layer_get_layer(earthLayer));
  refresh_earth_layer(centerFrame);

  infoLayer = layer_create(
      GRect(0, centerFrame.origin.y, bounds.size.w, centerFrame.size.h));
  layer_set_update_proc(infoLayer, draw_center_text);
  layer_add_child(shiftingLayer, infoLayer);

  // create ring layer
  ringLayer = layer_create(bounds);
  layer_set_update_proc(ringLayer, draw_ring_layer);
  layer_add_child(shiftingLayer, ringLayer);

  // subscribe to the unobstructed area events
  UnobstructedAreaHandlers handlers = {.change = onUnobstructedAreaChange,
                                       .did_change =
                                           onUnobstructedAreaDidChange};
  unobstructed_area_service_subscribe(handlers, NULL);

  // just in case quick view is open on load
  quickViewLayerReposition();

  // make sure the time is displayed from the start
  update_clock();
}

static void main_window_unload(Window *window) {
  cancel_earth_update_timer();

  // destroy everything
  layer_destroy(ringLayer);
  layer_destroy(infoLayer);
  bitmap_layer_destroy(earthLayer);
  destroy_earth_display_bitmap();
  earth_destroy();
  layer_destroy(centerLayer);
  layer_destroy(shiftingLayer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_clock();
}

#if defined(PBL_HEALTH)
static void health_handler(HealthEventType event, void *context) {
  if (event == HealthEventHeartRateUpdate) {
    update_clock();
  }
}
#endif

static void update_request_timer_callback(void *data);

static void schedule_next_update_request(uint32_t delay_ms) {
  if (s_update_request_timer) {
    app_timer_cancel(s_update_request_timer);
  }
  s_update_request_timer =
      app_timer_register(delay_ms, update_request_timer_callback, NULL);
}

static void update_request_timer_callback(void *data) {
  s_update_request_timer = NULL;

  // Schedule the next normal-interval tick *first*, so that a sync outbox
  // failure inside messaging_request_update() can call on_request_failed()
  // and pull the next attempt forward without us clobbering it afterward.
  schedule_next_update_request(UPDATE_REQUEST_INTERVAL_MS);
  messaging_request_update();
}

static void on_request_failed(void) {
  schedule_next_update_request(UPDATE_REQUEST_RETRY_MS);
}

static void init() {
  APP_LOG(APP_LOG_LEVEL_INFO, "Solar Earth init");

#ifdef FORCE_BACKLIGHT
  light_enable(true);
#endif

  // load those settings
  Settings_init();

  // init solar stuff
  solarUtils_init();

  // init the messaging thing
  messaging_init(onSettingsChanged, on_request_failed);

  // Create main Window element and assign to pointer
  mainWindow = window_create();

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(
      mainWindow,
      (WindowHandlers){.load = main_window_load, .unload = main_window_unload});

  window_stack_push(mainWindow, true);

  // Register with TickTimerService
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

#if defined(PBL_HEALTH)
  health_service_events_subscribe(health_handler, NULL);
#endif

  // Schedule initial update request with short delay to give PKJS time to start
  schedule_next_update_request(UPDATE_REQUEST_INITIAL_DELAY_MS);
}

static void deinit() {
  cancel_earth_update_timer();
  if (s_update_request_timer) {
    app_timer_cancel(s_update_request_timer);
    s_update_request_timer = NULL;
  }
#if defined(PBL_HEALTH)
  health_service_events_unsubscribe();
#endif
  window_destroy(mainWindow);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
