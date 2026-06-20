#pragma once
#include <pebble.h>

/*
 * Responsible for managing the sunrise/sunset calculation
 */

// default day times
#define DAY_END_MINUTE 1440
#define TWILIGHT_DURATION 30
#define SUNRISE_DURATION 60

// default sunrise at 6:00am, default sunset at 6:00pm:
#define DEFAULT_SUNRISE_TIME 360
#define DEFAULT_SUNSET_TIME 1080

// persistent storage
#define SOLAR_DATA_KEY 51

typedef struct {
  int sunsetMinute;
  int sunriseMinute;
} SolarInfo;

/**
 * Initialize solar utils (loads from persistent storage)
 */
void solarUtils_init();

void solarUtils_setSolarMinutes(int sunrise, int sunset);

void solarUtils_recalculateSolarData();

extern SolarInfo currentSolarInfo;

/*
 * Determines if it is night time based on solar data for the given minutes since midnight
 */
bool isNightTime(int currentMinutes);
