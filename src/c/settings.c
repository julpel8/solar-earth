#include "settings.h"
#include "solarUtils.h"
#include "utils.h"
#include "widgets.h"
#include <pebble.h>

Settings globalSettings;

// ---------------------------------------------------------------------------
// Per-field persist keys. Each setting owns its own key, so adding or removing
// a field never disturbs the others — no positional blob, no migrations.
// Numbered from 100 to stay clear of the legacy/solar keys (1, 2, 3, 51).
// ---------------------------------------------------------------------------
enum {
  PK_TIME_COLOR = 100,
  PK_SUBTEXT_PRIMARY_COLOR,
  PK_SUBTEXT_SECONDARY_COLOR,
  PK_BG_COLOR,
  PK_NIGHT_TIME_COLOR,
  PK_NIGHT_SUBTEXT_PRIMARY_COLOR,
  PK_NIGHT_SUBTEXT_SECONDARY_COLOR,
  PK_NIGHT_BG_COLOR,
  PK_USE_NIGHT_THEME,
  PK_USE_LARGE_FONTS,
  PK_SHOW_LEADING_ZERO,
  PK_USE_PRIMARY_FONT,
  PK_TEMP_UNIT,
  PK_LANGUAGE,
  PK_TIME_FORMAT,
  PK_REGION,
  PK_EARTH_UPDATE_INTERVAL,
  PK_ALT_CITY_LABEL,
  PK_ALT_CITY_UTC_OFFSET,
  PK_ALT_CITY2_LABEL,
  PK_ALT_CITY2_UTC_OFFSET,
  PK_INFO_LAYOUT,
  PK_WIDGET_UPPER_SECONDARY,
  PK_WIDGET_UPPER_PRIMARY,
  PK_WIDGET_LOWER_PRIMARY,
  PK_WIDGET_LOWER_SECONDARY,
};

// ---------------------------------------------------------------------------
// Legacy positional blobs (format <= v10). Read once during migration, then the
// old keys are deleted. The layout MUST match exactly what older builds wrote,
// including the now-dead ring/sun/pip fields, so the byte offsets line up.
// ---------------------------------------------------------------------------
typedef enum { LEGACY_PIP_ALL = 0, LEGACY_PIP_MAJOR = 1, LEGACY_PIP_HIDDEN = 2 }
    LegacyPipVisibilityType;

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
  LegacyPipVisibilityType pipVisibility;
  TempUnitType tempUnit;
  uint8_t language;
  char widgetUpperSecondary[WIDGET_TEXT_LEN];
  char widgetUpperPrimary[WIDGET_TEXT_LEN];
  char widgetLowerPrimary[WIDGET_TEXT_LEN];
  char widgetLowerSecondary[WIDGET_TEXT_LEN];
} LegacyStoredSettings;

typedef struct {
  char altCityLabel[ALT_CITY_LABEL_LEN];
  int16_t altCityUtcOffset;
  char altCity2Label[ALT_CITY_LABEL_LEN];
  int16_t altCity2UtcOffset;
  int16_t localUtcOffset;
  bool usePrimaryFontForAllWidgets;
  uint8_t region;
  char infoLayout[INFO_LAYOUT_LEN];
  uint8_t timeFormat;
  uint8_t earthUpdateInterval;
} LegacyStoredSettingsExtra;

// ---------------------------------------------------------------------------
// Small persist helpers
// ---------------------------------------------------------------------------
static void persist_put_color(uint32_t key, GColor c) {
  persist_write_data(key, &c, sizeof(GColor));
}

static GColor persist_get_color(uint32_t key, GColor def) {
  GColor c = def;
  if (persist_exists(key)) {
    persist_read_data(key, &c, sizeof(GColor));
  }
  return c;
}

static int persist_get_int(uint32_t key, int def) {
  return persist_exists(key) ? persist_read_int(key) : def;
}

static bool persist_get_bool(uint32_t key, bool def) {
  return persist_exists(key) ? persist_read_bool(key) : def;
}

// Overwrite buf only if the key exists; otherwise leave the caller's default.
static void persist_load_str(uint32_t key, char *buf, size_t buf_len) {
  if (persist_exists(key)) {
    persist_read_string(key, buf, buf_len);
    buf[buf_len - 1] = '\0';
  }
}

// ---------------------------------------------------------------------------
// Defaults / load / save
// ---------------------------------------------------------------------------
static void set_defaults(void) {
  memset(&globalSettings, 0, sizeof(globalSettings));

  globalSettings.timeColor = DEFAULT_TIME_COLOR;
  globalSettings.subtextPrimaryColor = DEFAULT_SUBTEXT_PRIMARY_COLOR;
  globalSettings.subtextSecondaryColor = DEFAULT_SUBTEXT_SECONDARY_COLOR;
  globalSettings.bgColor = DEFAULT_BG_COLOR;

  globalSettings.nightTimeColor = DEFAULT_NIGHT_TIME_COLOR;
  globalSettings.nightSubtextPrimaryColor = DEFAULT_NIGHT_SUBTEXT_PRIMARY_COLOR;
  globalSettings.nightSubtextSecondaryColor =
      DEFAULT_NIGHT_SUBTEXT_SECONDARY_COLOR;
  globalSettings.nightBgColor = DEFAULT_NIGHT_BG_COLOR;

  globalSettings.useNightTheme = true;
  globalSettings.useLargeFonts = false;
  globalSettings.showLeadingZero = false;
  globalSettings.usePrimaryFontForAllWidgets = false;
  globalSettings.tempUnit = TEMP_UNIT_CELSIUS;
  globalSettings.language = 0;
  globalSettings.timeFormat = TIME_FORMAT_SYSTEM;
  globalSettings.region = 0;             // Europe
  globalSettings.earthUpdateInterval = 5;  // minutes

  // Weather-dependent slots use placeholders until JS sends real data, so raw
  // tokens like "{temp}°" don't flash on first run.
  strncpy(globalSettings.widgetUpperSecondary, "--° (--° / --°)",
          WIDGET_TEXT_LEN);
  strncpy(globalSettings.widgetUpperPrimary, "--", WIDGET_TEXT_LEN);
  strncpy(globalSettings.widgetLowerPrimary, "{local_date}", WIDGET_TEXT_LEN);
#if defined(PBL_HEALTH)
  strncpy(globalSettings.widgetLowerSecondary, "{steps} {t:STEPS}",
          WIDGET_TEXT_LEN);
#else
  strncpy(globalSettings.widgetLowerSecondary, "{t:BATTERY} {batt}%",
          WIDGET_TEXT_LEN);
#endif

  strncpy(globalSettings.altCityLabel, "TYO", ALT_CITY_LABEL_LEN);
  globalSettings.altCityUtcOffset = 540;
  strncpy(globalSettings.altCity2Label, "UTC", ALT_CITY_LABEL_LEN);
  globalSettings.altCity2UtcOffset = 0;
  globalSettings.localUtcOffset = 0;

  strncpy(globalSettings.infoLayout, DEFAULT_INFO_LAYOUT, INFO_LAYOUT_LEN);
}

static void load_from_keys(void) {
  globalSettings.timeColor =
      persist_get_color(PK_TIME_COLOR, globalSettings.timeColor);
  globalSettings.subtextPrimaryColor =
      persist_get_color(PK_SUBTEXT_PRIMARY_COLOR, globalSettings.subtextPrimaryColor);
  globalSettings.subtextSecondaryColor = persist_get_color(
      PK_SUBTEXT_SECONDARY_COLOR, globalSettings.subtextSecondaryColor);
  globalSettings.bgColor = persist_get_color(PK_BG_COLOR, globalSettings.bgColor);

  globalSettings.nightTimeColor =
      persist_get_color(PK_NIGHT_TIME_COLOR, globalSettings.nightTimeColor);
  globalSettings.nightSubtextPrimaryColor = persist_get_color(
      PK_NIGHT_SUBTEXT_PRIMARY_COLOR, globalSettings.nightSubtextPrimaryColor);
  globalSettings.nightSubtextSecondaryColor = persist_get_color(
      PK_NIGHT_SUBTEXT_SECONDARY_COLOR, globalSettings.nightSubtextSecondaryColor);
  globalSettings.nightBgColor =
      persist_get_color(PK_NIGHT_BG_COLOR, globalSettings.nightBgColor);

  globalSettings.useNightTheme =
      persist_get_bool(PK_USE_NIGHT_THEME, globalSettings.useNightTheme);
  globalSettings.useLargeFonts =
      persist_get_bool(PK_USE_LARGE_FONTS, globalSettings.useLargeFonts);
  globalSettings.showLeadingZero =
      persist_get_bool(PK_SHOW_LEADING_ZERO, globalSettings.showLeadingZero);
  globalSettings.usePrimaryFontForAllWidgets = persist_get_bool(
      PK_USE_PRIMARY_FONT, globalSettings.usePrimaryFontForAllWidgets);

  globalSettings.tempUnit =
      (TempUnitType)persist_get_int(PK_TEMP_UNIT, globalSettings.tempUnit);
  globalSettings.language =
      (uint8_t)persist_get_int(PK_LANGUAGE, globalSettings.language);
  globalSettings.timeFormat =
      (uint8_t)persist_get_int(PK_TIME_FORMAT, globalSettings.timeFormat);
  globalSettings.region =
      (uint8_t)persist_get_int(PK_REGION, globalSettings.region);
  globalSettings.earthUpdateInterval = (uint8_t)persist_get_int(
      PK_EARTH_UPDATE_INTERVAL, globalSettings.earthUpdateInterval);

  globalSettings.altCityUtcOffset = (int16_t)persist_get_int(
      PK_ALT_CITY_UTC_OFFSET, globalSettings.altCityUtcOffset);
  globalSettings.altCity2UtcOffset = (int16_t)persist_get_int(
      PK_ALT_CITY2_UTC_OFFSET, globalSettings.altCity2UtcOffset);

  persist_load_str(PK_ALT_CITY_LABEL, globalSettings.altCityLabel,
                   ALT_CITY_LABEL_LEN);
  persist_load_str(PK_ALT_CITY2_LABEL, globalSettings.altCity2Label,
                   ALT_CITY_LABEL_LEN);
  persist_load_str(PK_INFO_LAYOUT, globalSettings.infoLayout, INFO_LAYOUT_LEN);
  persist_load_str(PK_WIDGET_UPPER_SECONDARY,
                   globalSettings.widgetUpperSecondary, WIDGET_TEXT_LEN);
  persist_load_str(PK_WIDGET_UPPER_PRIMARY, globalSettings.widgetUpperPrimary,
                   WIDGET_TEXT_LEN);
  persist_load_str(PK_WIDGET_LOWER_PRIMARY, globalSettings.widgetLowerPrimary,
                   WIDGET_TEXT_LEN);
  persist_load_str(PK_WIDGET_LOWER_SECONDARY,
                   globalSettings.widgetLowerSecondary, WIDGET_TEXT_LEN);
}

// One-time import of the old positional blob into globalSettings. globalSettings
// already holds the defaults, so any field missing from a short legacy blob
// keeps its default.
static void migrate_from_legacy(void) {
  if (persist_exists(LEGACY_SETTINGS_PERSIST_KEY)) {
    LegacyStoredSettings ls;
    memset(&ls, 0, sizeof(ls));
    int sz = persist_get_size(LEGACY_SETTINGS_PERSIST_KEY);
    int rd = sz < (int)sizeof(ls) ? sz : (int)sizeof(ls);
    if (rd > 0) {
      persist_read_data(LEGACY_SETTINGS_PERSIST_KEY, &ls, rd);

      globalSettings.timeColor = ls.timeColor;
      globalSettings.subtextPrimaryColor = ls.subtextPrimaryColor;
      globalSettings.subtextSecondaryColor = ls.subtextSecondaryColor;
      globalSettings.bgColor = ls.bgColor;
      globalSettings.nightTimeColor = ls.nightTimeColor;
      globalSettings.nightSubtextPrimaryColor = ls.nightSubtextPrimaryColor;
      globalSettings.nightSubtextSecondaryColor = ls.nightSubtextSecondaryColor;
      globalSettings.nightBgColor = ls.nightBgColor;
      globalSettings.useNightTheme = ls.useNightTheme;
      globalSettings.useLargeFonts = ls.useLargeFonts;
      globalSettings.showLeadingZero = ls.showLeadingZero;
      globalSettings.tempUnit = ls.tempUnit;
      globalSettings.language = ls.language;
      strncpy(globalSettings.widgetUpperSecondary, ls.widgetUpperSecondary,
              WIDGET_TEXT_LEN);
      globalSettings.widgetUpperSecondary[WIDGET_TEXT_LEN - 1] = '\0';
      strncpy(globalSettings.widgetUpperPrimary, ls.widgetUpperPrimary,
              WIDGET_TEXT_LEN);
      globalSettings.widgetUpperPrimary[WIDGET_TEXT_LEN - 1] = '\0';
      strncpy(globalSettings.widgetLowerPrimary, ls.widgetLowerPrimary,
              WIDGET_TEXT_LEN);
      globalSettings.widgetLowerPrimary[WIDGET_TEXT_LEN - 1] = '\0';
      strncpy(globalSettings.widgetLowerSecondary, ls.widgetLowerSecondary,
              WIDGET_TEXT_LEN);
      globalSettings.widgetLowerSecondary[WIDGET_TEXT_LEN - 1] = '\0';
    }
  }

  if (persist_exists(LEGACY_SETTINGS_EXTRA_PERSIST_KEY)) {
    LegacyStoredSettingsExtra le;
    memset(&le, 0, sizeof(le));
    int sz = persist_get_size(LEGACY_SETTINGS_EXTRA_PERSIST_KEY);
    int rd = sz < (int)sizeof(le) ? sz : (int)sizeof(le);
    if (rd > 0) {
      persist_read_data(LEGACY_SETTINGS_EXTRA_PERSIST_KEY, &le, rd);

      strncpy(globalSettings.altCityLabel, le.altCityLabel, ALT_CITY_LABEL_LEN);
      globalSettings.altCityLabel[ALT_CITY_LABEL_LEN - 1] = '\0';
      globalSettings.altCityUtcOffset = le.altCityUtcOffset;
      strncpy(globalSettings.altCity2Label, le.altCity2Label, ALT_CITY_LABEL_LEN);
      globalSettings.altCity2Label[ALT_CITY_LABEL_LEN - 1] = '\0';
      globalSettings.altCity2UtcOffset = le.altCity2UtcOffset;
      globalSettings.usePrimaryFontForAllWidgets =
          le.usePrimaryFontForAllWidgets;
      globalSettings.region = le.region;
      strncpy(globalSettings.infoLayout, le.infoLayout, INFO_LAYOUT_LEN);
      globalSettings.infoLayout[INFO_LAYOUT_LEN - 1] = '\0';
      globalSettings.timeFormat = le.timeFormat;
      globalSettings.earthUpdateInterval = le.earthUpdateInterval;
    }
  }
}

void Settings_init() { Settings_loadFromStorage(); }

void Settings_deinit() { Settings_saveToStorage(); }

void Settings_loadFromStorage() {
  set_defaults();

  int version = persist_get_int(SETTINGS_VERSION_PERSIST_KEY, 0);

  if (version >= CURRENT_SETTINGS_VERSION) {
    // New keyed format.
    load_from_keys();
  } else if (persist_exists(LEGACY_SETTINGS_PERSIST_KEY) ||
             persist_exists(LEGACY_SETTINGS_EXTRA_PERSIST_KEY)) {
    // One-time migration from the old positional blob.
    migrate_from_legacy();
    Settings_saveToStorage();  // rewrite in keyed format, bumps the version
    persist_delete(LEGACY_SETTINGS_PERSIST_KEY);
    persist_delete(LEGACY_SETTINGS_EXTRA_PERSIST_KEY);
  }
  // else: fresh install — defaults stand; persisted on the first settings change.

  // Guard against an empty/corrupt info layout.
  if (globalSettings.infoLayout[0] == '\0') {
    strncpy(globalSettings.infoLayout, DEFAULT_INFO_LAYOUT, INFO_LAYOUT_LEN);
    globalSettings.infoLayout[INFO_LAYOUT_LEN - 1] = '\0';
  }

  Settings_updateDynamicSettings();
}

void Settings_saveToStorage() {
  Settings_updateDynamicSettings();

  persist_put_color(PK_TIME_COLOR, globalSettings.timeColor);
  persist_put_color(PK_SUBTEXT_PRIMARY_COLOR, globalSettings.subtextPrimaryColor);
  persist_put_color(PK_SUBTEXT_SECONDARY_COLOR,
                    globalSettings.subtextSecondaryColor);
  persist_put_color(PK_BG_COLOR, globalSettings.bgColor);
  persist_put_color(PK_NIGHT_TIME_COLOR, globalSettings.nightTimeColor);
  persist_put_color(PK_NIGHT_SUBTEXT_PRIMARY_COLOR,
                    globalSettings.nightSubtextPrimaryColor);
  persist_put_color(PK_NIGHT_SUBTEXT_SECONDARY_COLOR,
                    globalSettings.nightSubtextSecondaryColor);
  persist_put_color(PK_NIGHT_BG_COLOR, globalSettings.nightBgColor);

  persist_write_bool(PK_USE_NIGHT_THEME, globalSettings.useNightTheme);
  persist_write_bool(PK_USE_LARGE_FONTS, globalSettings.useLargeFonts);
  persist_write_bool(PK_SHOW_LEADING_ZERO, globalSettings.showLeadingZero);
  persist_write_bool(PK_USE_PRIMARY_FONT,
                     globalSettings.usePrimaryFontForAllWidgets);

  persist_write_int(PK_TEMP_UNIT, globalSettings.tempUnit);
  persist_write_int(PK_LANGUAGE, globalSettings.language);
  persist_write_int(PK_TIME_FORMAT, globalSettings.timeFormat);
  persist_write_int(PK_REGION, globalSettings.region);
  persist_write_int(PK_EARTH_UPDATE_INTERVAL, globalSettings.earthUpdateInterval);
  persist_write_int(PK_ALT_CITY_UTC_OFFSET, globalSettings.altCityUtcOffset);
  persist_write_int(PK_ALT_CITY2_UTC_OFFSET, globalSettings.altCity2UtcOffset);

  persist_write_string(PK_ALT_CITY_LABEL, globalSettings.altCityLabel);
  persist_write_string(PK_ALT_CITY2_LABEL, globalSettings.altCity2Label);
  persist_write_string(PK_INFO_LAYOUT, globalSettings.infoLayout);
  persist_write_string(PK_WIDGET_UPPER_SECONDARY,
                       globalSettings.widgetUpperSecondary);
  persist_write_string(PK_WIDGET_UPPER_PRIMARY,
                       globalSettings.widgetUpperPrimary);
  persist_write_string(PK_WIDGET_LOWER_PRIMARY,
                       globalSettings.widgetLowerPrimary);
  persist_write_string(PK_WIDGET_LOWER_SECONDARY,
                       globalSettings.widgetLowerSecondary);

  persist_write_int(SETTINGS_VERSION_PERSIST_KEY, CURRENT_SETTINGS_VERSION);
}

static int16_t get_local_utc_offset(void) {
  time_t now = time(NULL);
  struct tm local_time = *localtime(&now);
  struct tm utc_time = *gmtime(&now);

  int day_delta = local_time.tm_yday - utc_time.tm_yday;
  if (local_time.tm_year > utc_time.tm_year) {
    day_delta = 1;
  } else if (local_time.tm_year < utc_time.tm_year) {
    day_delta = -1;
  } else if (day_delta > 1) {
    day_delta = -1;
  } else if (day_delta < -1) {
    day_delta = 1;
  }

  return (int16_t)((day_delta * 24 + local_time.tm_hour - utc_time.tm_hour) *
                       60 +
                   local_time.tm_min - utc_time.tm_min);
}

void Settings_updateDynamicSettings() {
  globalSettings.localUtcOffset = get_local_utc_offset();
}

bool settings_is_24h(void) {
  switch ((TimeFormatType)globalSettings.timeFormat) {
    case TIME_FORMAT_24H:
      return true;
    case TIME_FORMAT_12H:
    case TIME_FORMAT_12H_AMPM:
      return false;
    case TIME_FORMAT_SYSTEM:
    default:
      return clock_is_24h_style();
  }
}

bool settings_show_am_pm(void) {
  return (TimeFormatType)globalSettings.timeFormat == TIME_FORMAT_12H_AMPM;
}

uint16_t settings_earth_update_seconds(void) {
  switch (globalSettings.earthUpdateInterval) {
    case 1:
    case 5:
    case 15:
    case 30:
    case 60:
      return (uint16_t)globalSettings.earthUpdateInterval * 60;
    default:
      return 5 * 60;
  }
}

ColorTheme getCurrentColorTheme() {
  ColorTheme theme;

  struct tm *timeInfo = getCurrentTime();
  int currentMinutes = timeInfo->tm_hour * 60 + timeInfo->tm_min;

  bool useNight = globalSettings.useNightTheme && isNightTime(currentMinutes);

  if (useNight) {
    theme.timeColor = globalSettings.nightTimeColor;
    theme.subtextPrimaryColor = globalSettings.nightSubtextPrimaryColor;
    theme.subtextSecondaryColor = globalSettings.nightSubtextSecondaryColor;
    theme.bgColor = globalSettings.nightBgColor;
  } else {
    theme.timeColor = globalSettings.timeColor;
    theme.subtextPrimaryColor = globalSettings.subtextPrimaryColor;
    theme.subtextSecondaryColor = globalSettings.subtextSecondaryColor;
    theme.bgColor = globalSettings.bgColor;
  }

  return theme;
}
