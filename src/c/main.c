#include <pebble.h>

// #define FORCE_BACKLIGHT

#include "drawUtils.h"
#include "earth.h"
#include "info_layout.h"
#include "messaging.h"
#include "settings.h"
#include "utils.h"

// Long enough for "12:30 PM" plus the null terminator.
#define TIME_STR_LEN 12

// Heartbeat timer: the watch requests data from the phone on this interval
#define UPDATE_REQUEST_INTERVAL_MS (30 * 60 * 1000) // 30 minutes
#define UPDATE_REQUEST_INITIAL_DELAY_MS (3 * 1000)   // 3 seconds after startup
#define UPDATE_REQUEST_RETRY_MS (30 * 1000)          // fast retry after outbox fail

// windows and layers
static Window *mainWindow;
static Layer *windowLayer;
static Layer *shiftingLayer;
static Layer *centerLayer;
static Layer *infoLayer;

// Time string (populated each tick), read by the info layout.
static char timeText[TIME_STR_LEN];

// Heartbeat timer for requesting data updates from the phone
static AppTimer *s_update_request_timer = NULL;

static GRect get_center_frame(GRect bounds) {
  // The globe always fills the screen; no edge ring inset.
  return bounds;
}

// Resize literally everything on quick view
static void quickViewLayerReposition() {
  GRect full_bounds = layer_get_bounds(windowLayer);
  GRect bounds = layer_get_unobstructed_bounds(windowLayer);
  info_layout_set_quick_view(bounds.size.h < full_bounds.size.h);

  // Resize the shifting and center layer based on the current unobstructed
  // bounds
  layer_set_frame(shiftingLayer,
                  GRect(0, 0, full_bounds.size.w, bounds.size.h));

  GRect centerFrame = get_center_frame(
      GRect(0, 0, full_bounds.size.w, bounds.size.h));
  layer_set_frame(centerLayer, centerFrame);

  layer_set_frame(infoLayer, GRect(0, centerFrame.origin.y, full_bounds.size.w,
                                   centerFrame.size.h));

  earth_render_reposition(centerFrame);

  // Mark everything as dirty to redraw
  layer_mark_dirty(centerLayer);
  layer_mark_dirty(infoLayer);
}

static void update_clock() {
  Settings_updateDynamicSettings();

  time_t now = time(NULL);
  struct tm *timeInfo = getCurrentTime();

  // set time string
  if (settings_is_24h()) {
    strftime(timeText, TIME_STR_LEN, "%H:%M", timeInfo);
  } else if (settings_show_am_pm()) {
    strftime(timeText, TIME_STR_LEN, "%I:%M %p", timeInfo);
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
  info_layout_update_widgets();

  // ensure colors are updated based on settings
  ColorTheme currentTheme = getCurrentColorTheme();
  window_set_background_color(mainWindow, currentTheme.bgColor);
  earth_render_set_bg(currentTheme.bgColor);

  // Recompute the globe day/night shading off the startup path. The trigonometry
  // is too expensive to run synchronously on the UI thread at launch.
  earth_render_maybe_update(now);

  layer_mark_dirty(centerLayer);
}

// settings might have changed, so recalculate solar data and refresh screen
void onSettingsChanged() {
  bool regionChanged = earth_render_set_region(globalSettings.region);
  bool colorsChanged = earth_render_set_colors();

  earth_render_set_bg(getCurrentColorTheme().bgColor);

  // Both set_region and set_colors free the bitmap the earth layer points at;
  // reposition synchronously rebuilds and re-binds it before any repaint, so we
  // must do it for either change (not just region) to avoid a dangling pointer.
  if (windowLayer && (regionChanged || colorsChanged)) {
    quickViewLayerReposition();
  }

  if (regionChanged || colorsChanged) {
    earth_render_force_update(time(NULL));
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
  window_set_background_color(window, currentTheme.bgColor);
  GRect bounds = layer_get_bounds(windowLayer);

  shiftingLayer = layer_create(bounds);
  layer_add_child(windowLayer, shiftingLayer);

  // create central rectangle
  GRect centerFrame = get_center_frame(bounds);
  centerLayer = layer_create(centerFrame);
  layer_set_update_proc(centerLayer, draw_center_layer);

  layer_add_child(shiftingLayer, centerLayer);

  // Earth globe — sits above the centre background, below the time text.
  earth_render_init(shiftingLayer, centerFrame, currentTheme.bgColor,
                    globalSettings.region);

  infoLayer = layer_create(
      GRect(0, centerFrame.origin.y, bounds.size.w, centerFrame.size.h));
  info_layout_set_time_text(timeText);
  layer_set_update_proc(infoLayer, info_layout_update_proc);
  layer_add_child(shiftingLayer, infoLayer);

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
  // destroy everything
  layer_destroy(infoLayer);
  earth_render_deinit();
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
