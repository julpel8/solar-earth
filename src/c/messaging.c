#include "messaging.h"
#include "earth.h"
#include "settings.h"
#include <pebble.h>

void (*message_processed_callback)(void);
void (*request_failed_callback)(void);

#define APP_MESSAGE_INBOX_SIZE 1536
#define APP_MESSAGE_OUTBOX_SIZE 128

void messaging_init(void (*processed_callback)(void),
                    void (*failed_callback)(void)) {
  message_processed_callback = processed_callback;
  request_failed_callback = failed_callback;

  // Register callbacks
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);

  AppMessageResult result =
      app_message_open(APP_MESSAGE_INBOX_SIZE, APP_MESSAGE_OUTBOX_SIZE);
  APP_LOG(APP_LOG_LEVEL_INFO, "AppMessage open result: %d", (int)result);
}

void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "AppMessage inbox received");

  // text colors — fill and outline, one per line id (0..INFO_LINE_COUNT-1)
  const uint32_t lineColorKeys[INFO_LINE_COUNT] = {
      MESSAGE_KEY_SETTING_LINE_COLOR_0, MESSAGE_KEY_SETTING_LINE_COLOR_1,
      MESSAGE_KEY_SETTING_LINE_COLOR_2, MESSAGE_KEY_SETTING_LINE_COLOR_3,
      MESSAGE_KEY_SETTING_LINE_COLOR_4};
  const uint32_t lineOutlineKeys[INFO_LINE_COUNT] = {
      MESSAGE_KEY_SETTING_LINE_OUTLINE_0, MESSAGE_KEY_SETTING_LINE_OUTLINE_1,
      MESSAGE_KEY_SETTING_LINE_OUTLINE_2, MESSAGE_KEY_SETTING_LINE_OUTLINE_3,
      MESSAGE_KEY_SETTING_LINE_OUTLINE_4};
  Tuple *bgColor_tuple = dict_find(iterator, MESSAGE_KEY_SETTING_BG_COLOR);

  Tuple *globeDayOcean_tuple =
      dict_find(iterator, MESSAGE_KEY_SETTING_GLOBE_DAY_OCEAN);
  Tuple *globeDayLand_tuple =
      dict_find(iterator, MESSAGE_KEY_SETTING_GLOBE_DAY_LAND);
  Tuple *globeNightOcean_tuple =
      dict_find(iterator, MESSAGE_KEY_SETTING_GLOBE_NIGHT_OCEAN);
  Tuple *globeNightLand_tuple =
      dict_find(iterator, MESSAGE_KEY_SETTING_GLOBE_NIGHT_LAND);
  Tuple *globeCity_tuple =
      dict_find(iterator, MESSAGE_KEY_SETTING_GLOBE_CITY);

  Tuple *useLargeFonts_tuple =
      dict_find(iterator, MESSAGE_KEY_SETTING_USE_LARGE_FONTS);

  Tuple *usePrimaryFontForAllWidgets_tuple =
      dict_find(iterator, MESSAGE_KEY_SETTING_USE_PRIMARY_WIDGET_FONT);

  Tuple *showLeadingZero_tuple =
      dict_find(iterator, MESSAGE_KEY_SETTING_SHOW_LEADING_ZERO);

  Tuple *timeFormat_tuple =
      dict_find(iterator, MESSAGE_KEY_SETTING_TIME_FORMAT);

  Tuple *earthUpdateInterval_tuple =
      dict_find(iterator, MESSAGE_KEY_SETTING_EARTH_UPDATE_INTERVAL);

  Tuple *widgetUpperSecondary_tuple =
      dict_find(iterator, MESSAGE_KEY_SETTING_WIDGET_UPPER_SECONDARY);
  Tuple *widgetUpperPrimary_tuple =
      dict_find(iterator, MESSAGE_KEY_SETTING_WIDGET_UPPER_PRIMARY);
  Tuple *widgetLowerPrimary_tuple =
      dict_find(iterator, MESSAGE_KEY_SETTING_WIDGET_LOWER_PRIMARY);
  Tuple *widgetLowerSecondary_tuple =
      dict_find(iterator, MESSAGE_KEY_SETTING_WIDGET_LOWER_SECONDARY);

  Tuple *tempUnit_tuple =
      dict_find(iterator, MESSAGE_KEY_SETTING_TEMP_UNIT);
  Tuple *language_tuple =
      dict_find(iterator, MESSAGE_KEY_SETTING_LANGUAGE);
  Tuple *altCityLabel_tuple =
      dict_find(iterator, MESSAGE_KEY_SETTING_ALT_CITY_LABEL);
  Tuple *altCityUtcOffset_tuple =
      dict_find(iterator, MESSAGE_KEY_ALT_CITY_UTC_OFFSET);
  Tuple *altCity2Label_tuple =
      dict_find(iterator, MESSAGE_KEY_SETTING_ALT_CITY2_LABEL);
  Tuple *altCity2UtcOffset_tuple =
      dict_find(iterator, MESSAGE_KEY_ALT_CITY2_UTC_OFFSET);
  Tuple *localUtcOffset_tuple =
      dict_find(iterator, MESSAGE_KEY_LOCAL_UTC_OFFSET);
  Tuple *region_tuple = dict_find(iterator, MESSAGE_KEY_SETTING_REGION);
  Tuple *infoLayout_tuple = dict_find(iterator, MESSAGE_KEY_SETTING_INFO_LAYOUT);

  for (int i = 0; i < INFO_LINE_COUNT; i++) {
    Tuple *lineColor_tuple = dict_find(iterator, lineColorKeys[i]);
    if (lineColor_tuple != NULL) {
      globalSettings.lineColor[i] =
          GColorFromHEX(lineColor_tuple->value->int32);
    }
    Tuple *lineOutline_tuple = dict_find(iterator, lineOutlineKeys[i]);
    if (lineOutline_tuple != NULL) {
      globalSettings.lineOutlineColor[i] =
          GColorFromHEX(lineOutline_tuple->value->int32);
    }
  }

  if (bgColor_tuple != NULL) {
    globalSettings.bgColor = GColorFromHEX(bgColor_tuple->value->int32);
  }

  if (globeDayOcean_tuple != NULL) {
    globalSettings.globeDayOcean =
        GColorFromHEX(globeDayOcean_tuple->value->int32);
  }
  if (globeDayLand_tuple != NULL) {
    globalSettings.globeDayLand =
        GColorFromHEX(globeDayLand_tuple->value->int32);
  }
  if (globeNightOcean_tuple != NULL) {
    globalSettings.globeNightOcean =
        GColorFromHEX(globeNightOcean_tuple->value->int32);
  }
  if (globeNightLand_tuple != NULL) {
    globalSettings.globeNightLand =
        GColorFromHEX(globeNightLand_tuple->value->int32);
  }
  if (globeCity_tuple != NULL) {
    globalSettings.globeCity = GColorFromHEX(globeCity_tuple->value->int32);
  }

  if (useLargeFonts_tuple != NULL) {
    globalSettings.useLargeFonts = (bool)useLargeFonts_tuple->value->int8;
  }

  if (usePrimaryFontForAllWidgets_tuple != NULL) {
    globalSettings.usePrimaryFontForAllWidgets =
        (bool)usePrimaryFontForAllWidgets_tuple->value->int8;
  }

  if (showLeadingZero_tuple != NULL) {
    globalSettings.showLeadingZero = (bool)showLeadingZero_tuple->value->int8;
  }

  if (timeFormat_tuple != NULL) {
    uint8_t tf = (uint8_t)timeFormat_tuple->value->int8;
    globalSettings.timeFormat =
        tf <= TIME_FORMAT_12H_AMPM ? tf : TIME_FORMAT_SYSTEM;
  }

  if (earthUpdateInterval_tuple != NULL) {
    uint8_t mins = (uint8_t)earthUpdateInterval_tuple->value->int8;
    switch (mins) {
      case 1:
      case 5:
      case 15:
      case 30:
      case 60:
        globalSettings.earthUpdateInterval = mins;
        break;
      default:
        globalSettings.earthUpdateInterval = 5;
        break;
    }
  }

  if (widgetUpperSecondary_tuple != NULL) {
    strncpy(globalSettings.widgetUpperSecondary,
            widgetUpperSecondary_tuple->value->cstring, WIDGET_TEXT_LEN);
    globalSettings.widgetUpperSecondary[WIDGET_TEXT_LEN - 1] = '\0';
  }
  if (widgetUpperPrimary_tuple != NULL) {
    strncpy(globalSettings.widgetUpperPrimary,
            widgetUpperPrimary_tuple->value->cstring, WIDGET_TEXT_LEN);
    globalSettings.widgetUpperPrimary[WIDGET_TEXT_LEN - 1] = '\0';
  }
  if (widgetLowerPrimary_tuple != NULL) {
    strncpy(globalSettings.widgetLowerPrimary,
            widgetLowerPrimary_tuple->value->cstring, WIDGET_TEXT_LEN);
    globalSettings.widgetLowerPrimary[WIDGET_TEXT_LEN - 1] = '\0';
  }
  if (widgetLowerSecondary_tuple != NULL) {
    strncpy(globalSettings.widgetLowerSecondary,
            widgetLowerSecondary_tuple->value->cstring, WIDGET_TEXT_LEN);
    globalSettings.widgetLowerSecondary[WIDGET_TEXT_LEN - 1] = '\0';
  }

  if (tempUnit_tuple != NULL) {
    globalSettings.tempUnit = (TempUnitType)tempUnit_tuple->value->int8;
  }

  if (language_tuple != NULL) {
    globalSettings.language = (uint8_t)language_tuple->value->int8;
  }

  if (altCityLabel_tuple != NULL) {
    strncpy(globalSettings.altCityLabel, altCityLabel_tuple->value->cstring,
            ALT_CITY_LABEL_LEN);
    globalSettings.altCityLabel[ALT_CITY_LABEL_LEN - 1] = '\0';
  }

  if (altCityUtcOffset_tuple != NULL) {
    globalSettings.altCityUtcOffset =
        (int16_t)altCityUtcOffset_tuple->value->int32;
  }

  if (altCity2Label_tuple != NULL) {
    strncpy(globalSettings.altCity2Label, altCity2Label_tuple->value->cstring,
            ALT_CITY_LABEL_LEN);
    globalSettings.altCity2Label[ALT_CITY_LABEL_LEN - 1] = '\0';
  }

  if (altCity2UtcOffset_tuple != NULL) {
    globalSettings.altCity2UtcOffset =
        (int16_t)altCity2UtcOffset_tuple->value->int32;
  }

  if (localUtcOffset_tuple != NULL) {
    globalSettings.localUtcOffset =
        (int16_t)localUtcOffset_tuple->value->int32;
  }

  if (region_tuple != NULL) {
    uint8_t region = (uint8_t)region_tuple->value->int8;
    globalSettings.region = region < EARTH_NUM_REGIONS ? region : 0;
    APP_LOG(APP_LOG_LEVEL_INFO, "Received region: %d",
            (int)globalSettings.region);
  }

  if (infoLayout_tuple != NULL) {
    strncpy(globalSettings.infoLayout, infoLayout_tuple->value->cstring,
            INFO_LAYOUT_LEN);
    globalSettings.infoLayout[INFO_LAYOUT_LEN - 1] = '\0';
    APP_LOG(APP_LOG_LEVEL_INFO, "Received infoLayout: %s",
            globalSettings.infoLayout);
  }

  Settings_saveToStorage();

  if (message_processed_callback) message_processed_callback();
}

void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Inbox message dropped: %d", (int)reason);
}

void outbox_failed_callback(DictionaryIterator *iterator,
                            AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed: %d", (int)reason);
  // The watch only ever sends REQUEST_UPDATE via the outbox, so any failure
  // here means the heartbeat didn't reach the phone — ask main to retry soon.
  if (request_failed_callback) request_failed_callback();
}

void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  // APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

void messaging_request_update() {
  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);

  if (result != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to begin outbox: %d", (int)result);
    if (request_failed_callback) request_failed_callback();
    return;
  }

  dict_write_int8(iter, MESSAGE_KEY_REQUEST_UPDATE, 1);
  dict_write_int8(iter, MESSAGE_KEY_WATCH_IS_24H,
                  clock_is_24h_style() ? 1 : 0);

  result = app_message_outbox_send();
  if (result != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to send outbox: %d", (int)result);
    if (request_failed_callback) request_failed_callback();
  }
}
