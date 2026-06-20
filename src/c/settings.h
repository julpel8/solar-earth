#pragma once
#include "widgets.h"
#include <pebble.h>
#include <sys/syslimits.h>

#define CURRENT_SETTINGS_VERSION 10
#define SETTINGS_VERSION_PERSIST_KEY 1
#define SETTINGS_PERSIST_KEY 2
#define SETTINGS_EXTRA_PERSIST_KEY 3
#define ALT_CITY_LABEL_LEN 20
// Each info-layout entry is "id:group:size" (e.g. "2:1:1"); five entries plus
// commas need 30 bytes, so allow a little headroom.
#define INFO_LAYOUT_LEN 32
// id:group:size per line. size 0/1/2 = S/M/L. These defaults reproduce the
// previous look: secondary lines (0,4) small, primary lines (1,3) and the time
// (2) medium.
#define DEFAULT_INFO_LAYOUT "0:0:0,1:0:1,2:1:1,3:2:1,4:2:0"

// Per-line text size (S/M/L). Stored as the third field of each info-layout
// entry; the time line maps these to a proportionally larger font family.
#define INFO_SIZE_S 0
#define INFO_SIZE_M 1
#define INFO_SIZE_L 2
#define INFO_SIZE_DEFAULT INFO_SIZE_M

// default settings
#ifdef PBL_COLOR
#define DEFAULT_TIME_COLOR GColorBlack
#define DEFAULT_SUBTEXT_PRIMARY_COLOR GColorBlack
#define DEFAULT_SUBTEXT_SECONDARY_COLOR GColorDarkGray
#define DEFAULT_BG_COLOR GColorBlack
#define DEFAULT_PIP_COLOR_PRIMARY GColorBlack
#define DEFAULT_PIP_COLOR_SECONDARY GColorLightGray
#define DEFAULT_RING_STROKE_COLOR GColorBlack
#define DEFAULT_RING_NIGHT_COLOR GColorCobaltBlue
#define DEFAULT_RING_DAY_COLOR GColorVividCerulean
#define DEFAULT_RING_SUNRISE_COLOR GColorMelon
#define DEFAULT_RING_SUNSET_COLOR GColorChromeYellow
#define DEFAULT_SUN_STROKE_COLOR GColorBlack
#define DEFAULT_SUN_FILL_COLOR GColorYellow

// night theme defaults
#define DEFAULT_NIGHT_TIME_COLOR GColorFromHEX(0xFFFFFF)
#define DEFAULT_NIGHT_SUBTEXT_PRIMARY_COLOR GColorFromHEX(0xFFFFFF)
#define DEFAULT_NIGHT_SUBTEXT_SECONDARY_COLOR GColorFromHEX(0xAAAAFF)
#define DEFAULT_NIGHT_BG_COLOR GColorBlack
#define DEFAULT_NIGHT_PIP_COLOR_PRIMARY GColorFromHEX(0xFFFFFF)
#define DEFAULT_NIGHT_PIP_COLOR_SECONDARY GColorFromHEX(0x0055AA)
#define DEFAULT_NIGHT_RING_STROKE_COLOR GColorBlack
#define DEFAULT_NIGHT_RING_NIGHT_COLOR GColorFromHEX(0x0000AA)
#define DEFAULT_NIGHT_RING_DAY_COLOR GColorFromHEX(0x00AAFF)
#define DEFAULT_NIGHT_RING_SUNRISE_COLOR GColorFromHEX(0x0055FF)
#define DEFAULT_NIGHT_RING_SUNSET_COLOR GColorFromHEX(0x0055FF)
#define DEFAULT_NIGHT_SUN_STROKE_COLOR GColorBlack
#define DEFAULT_NIGHT_SUN_FILL_COLOR GColorFromHEX(0xFFFFFF)
#else
#define DEFAULT_TIME_COLOR GColorBlack
#define DEFAULT_SUBTEXT_PRIMARY_COLOR GColorBlack
#define DEFAULT_SUBTEXT_SECONDARY_COLOR GColorBlack
#define DEFAULT_BG_COLOR GColorBlack
#define DEFAULT_PIP_COLOR_PRIMARY GColorBlack
#define DEFAULT_PIP_COLOR_SECONDARY GColorBlack
#define DEFAULT_RING_STROKE_COLOR GColorBlack
#define DEFAULT_RING_NIGHT_COLOR GColorBlack
#define DEFAULT_RING_DAY_COLOR GColorWhite
#define DEFAULT_RING_SUNRISE_COLOR GColorLightGray
#define DEFAULT_RING_SUNSET_COLOR GColorLightGray
#define DEFAULT_SUN_STROKE_COLOR GColorBlack
#define DEFAULT_SUN_FILL_COLOR GColorWhite

// night theme defaults
#define DEFAULT_NIGHT_TIME_COLOR GColorWhite
#define DEFAULT_NIGHT_SUBTEXT_PRIMARY_COLOR GColorWhite
#define DEFAULT_NIGHT_SUBTEXT_SECONDARY_COLOR GColorWhite
#define DEFAULT_NIGHT_BG_COLOR GColorBlack
#define DEFAULT_NIGHT_PIP_COLOR_PRIMARY GColorWhite
#define DEFAULT_NIGHT_PIP_COLOR_SECONDARY GColorWhite
#define DEFAULT_NIGHT_RING_STROKE_COLOR GColorBlack
#define DEFAULT_NIGHT_RING_NIGHT_COLOR GColorBlack
#define DEFAULT_NIGHT_RING_DAY_COLOR GColorWhite
#define DEFAULT_NIGHT_RING_SUNRISE_COLOR GColorLightGray
#define DEFAULT_NIGHT_RING_SUNSET_COLOR GColorLightGray
#define DEFAULT_NIGHT_SUN_STROKE_COLOR GColorBlack
#define DEFAULT_NIGHT_SUN_FILL_COLOR GColorWhite
#endif

// default settings, black and white

typedef enum {
  PIP_SHOW_ALL = 0,
  PIP_SHOW_MAJOR = 1,
  PIP_HIDDEN = 2
} PipVisibilityType;

typedef enum { TEMP_UNIT_CELSIUS = 0, TEMP_UNIT_FAHRENHEIT = 1 } TempUnitType;

typedef enum {
  TIME_FORMAT_SYSTEM = 0,    // follow the watch's 12h/24h system setting
  TIME_FORMAT_24H = 1,       // force 24-hour
  TIME_FORMAT_12H = 2,       // force 12-hour, no AM/PM suffix
  TIME_FORMAT_12H_AMPM = 3   // force 12-hour with AM/PM suffix on the main time
} TimeFormatType;

// typedef enum {
//   NO_VIBE = 0,
//   VIBE_EVERY_HOUR = 1,
//   VIBE_EVERY_HALF_HOUR = 2
// } VibeIntervalType;

// Color theme struct containing just the color fields
typedef struct {
  GColor timeColor;
  GColor subtextPrimaryColor;
  GColor subtextSecondaryColor;
  GColor bgColor;
  GColor pipColorPrimary;
  GColor pipColorSecondary;
  GColor ringStrokeColor;
  GColor ringNightColor;
  GColor ringDayColor;
  GColor ringSunriseColor;
  GColor ringSunsetColor;
  GColor sunStrokeColor;
  GColor sunFillColor;
} ColorTheme;

typedef struct {
  // text colors
  GColor timeColor;
  GColor subtextPrimaryColor;
  GColor subtextSecondaryColor;

  // decoration colors
  GColor bgColor;
  GColor pipColorPrimary;
  GColor pipColorSecondary;
  GColor ringStrokeColor;
  GColor ringNightColor;
  GColor ringDayColor;
  GColor ringSunriseColor;
  GColor ringSunsetColor;
  GColor sunStrokeColor;
  GColor sunFillColor;

  // night theme colors
  GColor nightTimeColor;
  GColor nightSubtextPrimaryColor;
  GColor nightSubtextSecondaryColor;
  GColor nightBgColor;
  GColor nightPipColorPrimary;
  GColor nightPipColorSecondary;
  GColor nightRingStrokeColor;
  GColor nightRingNightColor;
  GColor nightRingDayColor;
  GColor nightRingSunriseColor;
  GColor nightRingSunsetColor;
  GColor nightSunStrokeColor;
  GColor nightSunFillColor;

  bool useNightTheme;
  bool useLargeFonts;
  bool showLeadingZero;
  PipVisibilityType pipVisibility;
  TempUnitType tempUnit;
  uint8_t language;

  // Widget slots (stored as format strings)
  char widgetUpperSecondary[WIDGET_TEXT_LEN];
  char widgetUpperPrimary[WIDGET_TEXT_LEN];
  char widgetLowerPrimary[WIDGET_TEXT_LEN];
  char widgetLowerSecondary[WIDGET_TEXT_LEN];

  char altCityLabel[ALT_CITY_LABEL_LEN];
  int16_t altCityUtcOffset;
  char altCity2Label[ALT_CITY_LABEL_LEN];
  int16_t altCity2UtcOffset;
  int16_t localUtcOffset;
  bool usePrimaryFontForAllWidgets;
  uint8_t region;  // globe centre region (EarthRegion)
  char infoLayout[INFO_LAYOUT_LEN];
  uint8_t timeFormat;  // TimeFormatType
  uint8_t earthUpdateInterval;  // globe day/night refresh interval, in minutes
} Settings;

typedef struct {
  // text colors
  GColor timeColor;
  GColor subtextPrimaryColor;
  GColor subtextSecondaryColor;

  // decoration colors
  GColor bgColor;
  GColor pipColorPrimary;
  GColor pipColorSecondary;
  GColor ringStrokeColor;
  GColor ringNightColor;
  GColor ringDayColor;
  GColor ringSunriseColor;
  GColor ringSunsetColor;
  GColor sunStrokeColor;
  GColor sunFillColor;

  // night theme colors
  GColor nightTimeColor;
  GColor nightSubtextPrimaryColor;
  GColor nightSubtextSecondaryColor;
  GColor nightBgColor;
  GColor nightPipColorPrimary;
  GColor nightPipColorSecondary;
  GColor nightRingStrokeColor;
  GColor nightRingNightColor;
  GColor nightRingDayColor;
  GColor nightRingSunriseColor;
  GColor nightRingSunsetColor;
  GColor nightSunStrokeColor;
  GColor nightSunFillColor;

  bool useNightTheme;
  bool useLargeFonts;
  bool showLeadingZero;

  PipVisibilityType pipVisibility;
  TempUnitType tempUnit;
  uint8_t language;

  // Widget slots (stored as format strings)
  char widgetUpperSecondary[WIDGET_TEXT_LEN];
  char widgetUpperPrimary[WIDGET_TEXT_LEN];
  char widgetLowerPrimary[WIDGET_TEXT_LEN];
  char widgetLowerSecondary[WIDGET_TEXT_LEN];
} StoredSettings;

// we're kinda at the limit for stored settings, so new settings get their own
// special struct
typedef struct {
  char altCityLabel[ALT_CITY_LABEL_LEN];
  int16_t altCityUtcOffset;
  char altCity2Label[ALT_CITY_LABEL_LEN];
  int16_t altCity2UtcOffset;
  int16_t localUtcOffset;
  bool usePrimaryFontForAllWidgets;
  uint8_t region;  // globe centre region (EarthRegion)
  char infoLayout[INFO_LAYOUT_LEN];
  uint8_t timeFormat;  // TimeFormatType
  uint8_t earthUpdateInterval;  // globe refresh interval (min); appended last so
                                // old blobs keep default
} StoredSettingsExtra;

typedef char StoredSettings_must_fit_in_persist_data
    [(sizeof(StoredSettings) <= PERSIST_DATA_MAX_LENGTH) ? 1 : -1];
typedef char StoredSettingsExtra_must_fit_in_persist_data
    [(sizeof(StoredSettingsExtra) <= PERSIST_DATA_MAX_LENGTH) ? 1 : -1];

extern Settings globalSettings;

ColorTheme getCurrentColorTheme();

// Resolve the effective time format from globalSettings.timeFormat.
// settings_is_24h() honours the TIME_FORMAT_SYSTEM choice by falling back to
// the watch's clock_is_24h_style(). settings_show_am_pm() is true only for the
// explicit "12-hour with AM/PM" option (controls the main time line).
bool settings_is_24h(void);
bool settings_show_am_pm(void);

// Globe day/night refresh interval in seconds, derived from the configured
// earthUpdateInterval (minutes). Falls back to 5 minutes for unknown values.
uint16_t settings_earth_update_seconds(void);

void Settings_init();
void Settings_deinit();
void Settings_loadFromStorage();
void Settings_saveToStorage();
void Settings_updateDynamicSettings();
