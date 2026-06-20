#pragma once
#include "widgets.h"
#include <pebble.h>
#include <sys/syslimits.h>

// Settings are persisted with one persist key per field (see settings.c). The
// version key records the storage format; v11 is the first keyed format and is
// migrated to once from the older positional blob (LEGACY_* keys).
#define CURRENT_SETTINGS_VERSION 11
#define SETTINGS_VERSION_PERSIST_KEY 1
// Legacy positional blobs (pre-v11), read once during migration then deleted.
#define LEGACY_SETTINGS_PERSIST_KEY 2
#define LEGACY_SETTINGS_EXTRA_PERSIST_KEY 3

#define ALT_CITY_LABEL_LEN 20
// Each info-layout entry is "id:group:size" (e.g. "2:1:1"); five entries plus
// commas need 30 bytes, so allow a little headroom.
#define INFO_LAYOUT_LEN 32
// id:group:size per line. size 0/1/2 = S/M/L. Secondary lines (0,4) small,
// primary lines (1,3) and the time (2) medium.
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
#else
#define DEFAULT_TIME_COLOR GColorBlack
#define DEFAULT_SUBTEXT_PRIMARY_COLOR GColorBlack
#define DEFAULT_SUBTEXT_SECONDARY_COLOR GColorBlack
#define DEFAULT_BG_COLOR GColorBlack
#endif

typedef enum { TEMP_UNIT_CELSIUS = 0, TEMP_UNIT_FAHRENHEIT = 1 } TempUnitType;

typedef enum {
  TIME_FORMAT_SYSTEM = 0,    // follow the watch's 12h/24h system setting
  TIME_FORMAT_24H = 1,       // force 24-hour
  TIME_FORMAT_12H = 2,       // force 12-hour, no AM/PM suffix
  TIME_FORMAT_12H_AMPM = 3   // force 12-hour with AM/PM suffix on the main time
} TimeFormatType;

// Color theme struct containing the resolved color fields.
typedef struct {
  GColor timeColor;
  GColor subtextPrimaryColor;
  GColor subtextSecondaryColor;
  GColor bgColor;
} ColorTheme;

// Live settings. Persistence is per-field (settings.c), so this struct's layout
// is free to change — add or remove fields without a migration.
typedef struct {
  // text colors
  GColor timeColor;
  GColor subtextPrimaryColor;
  GColor subtextSecondaryColor;
  GColor bgColor;

  bool useLargeFonts;
  bool showLeadingZero;
  bool usePrimaryFontForAllWidgets;
  TempUnitType tempUnit;
  uint8_t language;
  uint8_t timeFormat;  // TimeFormatType

  // Widget slots (stored as format strings)
  char widgetUpperSecondary[WIDGET_TEXT_LEN];
  char widgetUpperPrimary[WIDGET_TEXT_LEN];
  char widgetLowerPrimary[WIDGET_TEXT_LEN];
  char widgetLowerSecondary[WIDGET_TEXT_LEN];

  char altCityLabel[ALT_CITY_LABEL_LEN];
  int16_t altCityUtcOffset;
  char altCity2Label[ALT_CITY_LABEL_LEN];
  int16_t altCity2UtcOffset;
  int16_t localUtcOffset;  // derived each load; not persisted

  uint8_t region;  // globe centre region (EarthRegion)
  uint8_t earthUpdateInterval;  // globe day/night refresh interval, in minutes
  char infoLayout[INFO_LAYOUT_LEN];
} Settings;

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
