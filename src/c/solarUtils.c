#include "solarUtils.h"
#include <pebble.h>

SolarInfo currentSolarInfo = {
  .sunriseMinute = DEFAULT_SUNRISE_TIME,
  .sunsetMinute  = DEFAULT_SUNSET_TIME
};

void solarUtils_init() {
  if (persist_exists(SOLAR_DATA_KEY)) {
    persist_read_data(SOLAR_DATA_KEY, &currentSolarInfo, sizeof(SolarInfo));
  }
}

void solarUtils_setSolarMinutes(int sunrise, int sunset) {
  currentSolarInfo.sunriseMinute = sunrise;
  currentSolarInfo.sunsetMinute = sunset;
  persist_write_data(SOLAR_DATA_KEY, &currentSolarInfo, sizeof(SolarInfo));
}

void solarUtils_recalculateSolarData() {
  // No-op: solar data now comes from JS.
  // Retained so call sites in main.c don't need changes.
}

bool isNightTime(int currentMinutes) {
  bool result = (currentMinutes >= currentSolarInfo.sunsetMinute ||
                 currentMinutes <  currentSolarInfo.sunriseMinute);

  APP_LOG(APP_LOG_LEVEL_DEBUG,
          "isNightTime: current=%d, sunrise=%d, sunset=%d, result=%d",
          currentMinutes, currentSolarInfo.sunriseMinute,
          currentSolarInfo.sunsetMinute, result);

  return result;
}
