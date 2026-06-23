var SunCalc = require('./suncalc');
var Weather = require('./weather');
var Languages = require('./languages');
var Cities = require('./cities');

// Cached data (in-memory; also persisted to localStorage)
var cachedWeather = null;
var cachedSolar = null;
var cachedSettings = null;

var TIME_FORMAT_STORAGE_KEY = 'solarEarthIs24h';
var DEFAULT_ALT_CITY = 'TOKYO';
var DEFAULT_ALT_CITY2 = 'UTC';
var ALT_LABEL_MAX_LENGTH = 6;

function restoreIs24h() {
  try {
    return localStorage.getItem(TIME_FORMAT_STORAGE_KEY) === 'true';
  } catch (e) {
    return false;
  }
}

function saveIs24h(is24h) {
  try {
    localStorage.setItem(TIME_FORMAT_STORAGE_KEY, is24h ? 'true' : 'false');
  } catch (e) { }
}

// 24-hour preference from the watch. Restored before the first startup render,
// then refreshed from WATCH_IS_24H on each watch-initiated heartbeat.
var cachedIs24h = restoreIs24h();

// Default widget format strings — used when no settings have been configured yet
// so that JS can perform token substitution even on first run. The lower-primary
// uses the {local_date} super-token, which the watch expands per-language at
// render time.
var DEFAULT_WIDGETS = {
  'SETTING_WIDGET_UPPER_SECONDARY': '{temp}° ({thi}°/{tlo})°',
  'SETTING_WIDGET_UPPER_PRIMARY': '{cond}',
  'SETTING_WIDGET_LOWER_PRIMARY': '{local_date}',
  'SETTING_WIDGET_LOWER_SECONDARY': '{steps} {t:STEPS}'
};

var WIDGET_SLOT_KEYS = [
  'SETTING_WIDGET_UPPER_SECONDARY',
  'SETTING_WIDGET_UPPER_PRIMARY',
  'SETTING_WIDGET_LOWER_PRIMARY',
  'SETTING_WIDGET_LOWER_SECONDARY'
];

// Each entry is "id:group:size" — size 0/1/2/3 = S/M/L/XL. Must match the C-side
// DEFAULT_INFO_LAYOUT in settings.h.
var DEFAULT_INFO_LAYOUT = '0:0:0,1:0:1,2:1:1,3:2:1,4:2:0';

// Maps an info-layout slot id to its widget setting key (id 2 = time, no key).
// Shared by settingsUseWeather() and injected into the config page.
var SLOT_KEY_BY_ID = {
  0: 'SETTING_WIDGET_UPPER_SECONDARY',
  1: 'SETTING_WIDGET_UPPER_PRIMARY',
  3: 'SETTING_WIDGET_LOWER_PRIMARY',
  4: 'SETTING_WIDGET_LOWER_SECONDARY'
};

var WEATHER_WIDGET_TOKENS = [
  '{temp}', '{thi}', '{tlo}', '{cond}', '{cond_day}', '{hum}',
  '{wind}', '{uv}', '{rain}', '{pop}', '{dew}', '{temp_unit}',
  '{wind_unit}', '{wind_dir}'
];

var CONFIG_WIDGET_OPTIONS = [
  { v: '{local_date}', l: 'Date' },
  { v: '{year}-{month_num}-{day0}', l: 'Numeric date' },
  { v: '{day_name} {day}', l: 'Day + date' },
  { v: '{t:DAY} {day_of_year}', l: 'Day of year' },
  { v: '{t:WEEK} {week_of_year}', l: 'Week' },
  { v: '{next_solar}', l: 'Next sun event' },
  { v: '{sunrise}', l: 'Sunrise' },
  { v: '{sunset}', l: 'Sunset' },
  { v: '{steps} {t:STEPS}', l: 'Steps' },
  { v: '{dist} {dist_unit}', l: 'Distance' },
  { v: '{hr} {t:BPM}', l: 'Heart rate' },
  { v: '{t:BATTERY} {batt}%', l: 'Battery' },
  { v: '{temp}° ({thi}°/{tlo}°)', l: 'Temperature min/max' },
  { v: '{temp}°', l: 'Temperature' },
  { v: '{cond}', l: 'Weather' },
  { v: '{hum}% {t:HUMIDITY}', l: 'Humidity' },
  { v: '{wind} {wind_unit} {wind_dir}', l: 'Wind' },
  { v: '{t:UV} {uv}', l: 'UV' },
  { v: '{t:DPT} {dew}°', l: 'Dew point' },
  { v: '{t:RAIN} {pop}%', l: 'Rain' },
  { v: '{alt_tz}', l: 'Time zone 1' },
  { v: '{alt_tz2}', l: 'Time zone 2' },
  { v: '__custom__', l: 'Custom' }
];

// Reference shown by the "?" help modal in the Displayed info section. Mirrors
// the token list documented for the (removed) web config; grouped by category.
var CONFIG_WIDGET_HELP = [
  { cat: 'Date & Time', items: [
    { tok: '{local_date}', desc: 'Full localized date, in the selected language format.' },
    { tok: '{day_name}', desc: 'Abbreviated weekday name (e.g. MON).' },
    { tok: '{month_name}', desc: 'Abbreviated month name (e.g. JAN).' },
    { tok: '{day}', desc: 'Day of the month (1-31).' },
    { tok: '{day0}', desc: 'Day of the month, zero-padded (01-31).' },
    { tok: '{month_num}', desc: 'Month number, zero-padded (01-12).' },
    { tok: '{year}', desc: 'Four-digit year.' },
    { tok: '{day_of_year}', desc: 'Day number within the year (1-366).' },
    { tok: '{week_of_year}', desc: 'ISO week number (1-53).' }
  ] },
  { cat: 'World Time', items: [
    { tok: '{alt_tz}', desc: 'Time zone 1: label, time, and day (day shown only when it differs).' },
    { tok: '{alt_tz_label}', desc: 'Time zone 1 label only.' },
    { tok: '{alt_tz_time}', desc: 'Time zone 1 time only.' },
    { tok: '{alt_tz_day}', desc: 'Time zone 1 weekday only.' },
    { tok: '{alt_tz2}', desc: 'Time zone 2: label, time, and day (day shown only when it differs).' },
    { tok: '{alt_tz2_label}', desc: 'Time zone 2 label only.' },
    { tok: '{alt_tz2_time}', desc: 'Time zone 2 time only.' },
    { tok: '{alt_tz2_day}', desc: 'Time zone 2 weekday only.' }
  ] },
  { cat: 'Solar', items: [
    { tok: '{sunrise}', desc: "Today's sunrise time." },
    { tok: '{sunset}', desc: "Today's sunset time." },
    { tok: '{next_solar}', desc: 'Next solar event: label and time.' },
    { tok: '{next_solar_label}', desc: 'Label of the next solar event (RISE or SET).' },
    { tok: '{next_solar_time}', desc: 'Time of the next solar event.' }
  ] },
  { cat: 'Health & Device', items: [
    { tok: '{steps}', desc: 'Step count for today.' },
    { tok: '{dist}', desc: 'Distance walked today (value only).' },
    { tok: '{dist_unit}', desc: 'Distance unit (KM or MI).' },
    { tok: '{hr}', desc: 'Current heart rate (value only).' },
    { tok: '{batt}', desc: 'Battery level percentage (value only).' }
  ] },
  { cat: 'Weather', items: [
    { tok: '{temp}', desc: 'Current temperature (value only).' },
    { tok: '{thi}', desc: 'Forecast high temperature (value only).' },
    { tok: '{tlo}', desc: 'Forecast low temperature (value only).' },
    { tok: '{cond}', desc: 'Current weather condition.' },
    { tok: '{cond_day}', desc: 'Daytime weather condition.' },
    { tok: '{hum}', desc: 'Relative humidity (value only).' },
    { tok: '{wind}', desc: 'Wind speed (value only).' },
    { tok: '{wind_unit}', desc: 'Wind speed unit (KM/H or MPH).' },
    { tok: '{wind_dir}', desc: 'Wind direction (cardinal, e.g. NW).' },
    { tok: '{uv}', desc: 'UV index (value only).' },
    { tok: '{rain}', desc: 'Rain amount (value only).' },
    { tok: '{pop}', desc: 'Chance of rain percentage (value only).' },
    { tok: '{dew}', desc: 'Dew point temperature (value only).' },
    { tok: '{temp_unit}', desc: 'Temperature unit (°C or °F).' }
  ] }
];

var CONFIG_LANGUAGES = [
  'English', 'French', 'German', 'Spanish', 'Italian', 'Dutch',
  'Turkish', 'Czech', 'Portuguese', 'Greek', 'Swedish', 'Polish', 'Slovak',
  'Vietnamese', 'Romanian', 'Catalan', 'Norwegian', 'Russian', 'Estonian',
  'Basque', 'Finnish', 'Danish', 'Lithuanian', 'Slovenian', 'Hungarian',
  'Croatian', 'Irish', 'Latvian', 'Serbian', 'Chinese', 'Indonesian',
  'Ukrainian', 'Welsh', 'Galician', 'Japanese', 'Korean', 'Hebrew',
  'English UK'
];

function getDefaultWidgets() {
  var defaults = {};
  Object.keys(DEFAULT_WIDGETS).forEach(function (key) {
    defaults[key] = DEFAULT_WIDGETS[key];
  });

  var watchInfo = Pebble.getActiveWatchInfo && Pebble.getActiveWatchInfo();
  if (watchInfo && watchInfo.platform === 'aplite') {
    defaults.SETTING_WIDGET_LOWER_SECONDARY = '{t:BATTERY} {batt}%';
  }

  return defaults;
}

function mergeObjects() {
  var merged = {};
  for (var i = 0; i < arguments.length; i++) {
    var source = arguments[i] || {};
    Object.keys(source).forEach(function (key) {
      merged[key] = source[key];
    });
  }
  return merged;
}

function getDefaultConfigSettings() {
  return mergeObjects({
    SETTING_LINE_COLOR_0: 'AAAAAA',
    SETTING_LINE_COLOR_1: 'FFFFFF',
    SETTING_LINE_COLOR_2: 'FFFFFF',
    SETTING_LINE_COLOR_3: 'FFFFFF',
    SETTING_LINE_COLOR_4: 'AAAAAA',
    SETTING_LINE_OUTLINE_0: '000000',
    SETTING_LINE_OUTLINE_1: '000000',
    SETTING_LINE_OUTLINE_2: '000000',
    SETTING_LINE_OUTLINE_3: '000000',
    SETTING_LINE_OUTLINE_4: '000000',
    SETTING_BG_COLOR: '000000',
    SETTING_USE_LARGE_FONTS: 0,
    SETTING_USE_PRIMARY_WIDGET_FONT: 0,
    SETTING_SHOW_LEADING_ZERO: 0,
    SETTING_TIME_FORMAT: 0,
    SETTING_EARTH_UPDATE_INTERVAL: 5,
    SETTING_TEMP_UNIT: 0,
    SETTING_LANGUAGE: 0,
    SETTING_ALT_CITY: 'TOKYO',
    SETTING_ALT_LABEL: 'TYO',
    SETTING_ALT_CITY2: 'UTC',
    SETTING_ALT_LABEL2: 'UTC',
    SETTING_REGION: 0,
    SETTING_INFO_LAYOUT: DEFAULT_INFO_LAYOUT
  }, getDefaultWidgets());
}

function getConfigSettings() {
  return mergeObjects(getDefaultConfigSettings(), cachedSettings || {});
}

function settingsUseWeather(settings) {
  var defaultWidgets = getDefaultWidgets();
  settings = settings || {};
  var activeKeys = {};
  String(settings.SETTING_INFO_LAYOUT || DEFAULT_INFO_LAYOUT).split(',').forEach(function (part) {
    var match = /^([0-4]):[0-2](?::[0-3])?$/.exec(part);
    if (match && SLOT_KEY_BY_ID[match[1]]) {
      activeKeys[SLOT_KEY_BY_ID[match[1]]] = true;
    }
  });

  return WIDGET_SLOT_KEYS.some(function (key) {
    if (!activeKeys[key]) return false;
    var fmt = settings[key];
    if (fmt === undefined || fmt === null) {
      fmt = defaultWidgets[key];
    }
    if (!fmt) return false;

    return WEATHER_WIDGET_TOKENS.some(function (token) {
      return fmt.indexOf(token) !== -1;
    });
  });
}

// ---- Time helpers ----

// Resolve the effective 24h flag from the time-format setting, mirroring the
// C-side settings_is_24h(). 0 = follow the watch (cachedIs24h), 1 = 24h,
// 2/3 = 12h. Used so JS-formatted solar tokens match the watch's main time.
function resolveUse24h(settings) {
  var tf = parseInt(settings && settings.SETTING_TIME_FORMAT, 10);
  if (tf === 1) return true;
  if (tf === 2 || tf === 3) return false;
  return cachedIs24h;
}

function formatMinutes(minutes, use24h) {
  if (minutes < 0) return '--:--';
  var h = Math.floor(minutes / 60) % 24;
  var m = minutes % 60;
  if (use24h) {
    return h + ':' + (m < 10 ? '0' : '') + m;
  }
  var ampm = h >= 12 ? 'PM' : 'AM';
  var h12 = h % 12 || 12;
  return h12 + ':' + (m < 10 ? '0' : '') + m + ' ' + ampm;
}

function getNextSolarEvent(solar, use24h, labels) {
  if (!solar) {
    return {
      label: '--',
      time: '--:--',
      text: '-- --:--'
    };
  }

  var now = new Date();
  var currentMinute = now.getHours() * 60 + now.getMinutes();
  var sunriseDelta = (solar.sunriseMinute - currentMinute + 24 * 60) % (24 * 60);
  var sunsetDelta = (solar.sunsetMinute - currentMinute + 24 * 60) % (24 * 60);
  var isSunriseNext = sunriseDelta < sunsetDelta;
  var label = isSunriseNext ? (labels.RISE || 'RISE') : (labels.SET || 'SET');
  var time = formatMinutes(isSunriseNext ? solar.sunriseMinute : solar.sunsetMinute, use24h);

  return {
    label: label,
    time: time,
    text: label + ' ' + time
  };
}

function getSelectedAltCity(settings, key, fallback) {
  var cityName = settings[key] || fallback;
  return Cities.findCity(cityName) || Cities.findCity('UTC');
}

function applyAltCityMessage(msg, settings, cityKey, labelKey, messageLabelKey, messageOffsetKey, fallbackCity) {
  var now = new Date();
  var city = getSelectedAltCity(settings || {}, cityKey, fallbackCity);
  if (!city) return;

  var label = settings[labelKey] || city.label || city.name;
  msg[messageLabelKey] = String(label).toUpperCase().slice(0, ALT_LABEL_MAX_LENGTH);
  msg[messageOffsetKey] = Cities.cityOffsetMinutes(city, now);
  msg.LOCAL_UTC_OFFSET = -now.getTimezoneOffset();
}

// ---- Pass 1: token substitution ----

/**
 * Given a format string like "{temp}° {cond} · {sunrise}", replaces all
 * JS-side tokens with their current values and returns the result.
 * Unknown tokens (e.g. {date}, {steps}) are left untouched for the C side.
 */
function applyJsTokens(formatStr, weather, solar, isImperial, use24h, lang) {
  if (!formatStr) return formatStr;

  var L = Languages.getLang(lang);

  var result = formatStr;

  // Solar tokens
  if (solar) {
    var nextSolar = getNextSolarEvent(solar, use24h, L.labels);
    result = result.replace('{sunrise}', formatMinutes(solar.sunriseMinute, use24h));
    result = result.replace('{sunset}', formatMinutes(solar.sunsetMinute, use24h));
    result = result.replace('{next_solar}', nextSolar.text);
    result = result.replace('{next_solar_label}', nextSolar.label);
    result = result.replace('{next_solar_time}', nextSolar.time);
  } else {
    var nextSolarPlaceholder = getNextSolarEvent(null, use24h, L.labels);
    result = result.replace('{sunrise}', '--:--');
    result = result.replace('{sunset}', '--:--');
    result = result.replace('{next_solar}', nextSolarPlaceholder.text);
    result = result.replace('{next_solar_label}', nextSolarPlaceholder.label);
    result = result.replace('{next_solar_time}', nextSolarPlaceholder.time);
  }

  // Weather tokens
  if (weather) {
    var temp = isImperial ? Weather.toF(weather.temp) : Math.round(weather.temp);
    var tempHi = isImperial ? Weather.toF(weather.tempHi) : Math.round(weather.tempHi);
    var tempLo = isImperial ? Weather.toF(weather.tempLo) : Math.round(weather.tempLo);
    var dew = isImperial ? Weather.toF(weather.dew) : Math.round(weather.dew);
    var wind = isImperial ? Weather.toMPH(weather.wind) : Math.round(weather.wind);
    var rain = isImperial ? Weather.toInch(weather.rain) : weather.rain.toFixed(1);

    result = result.replace('{temp}', String(temp));
    result = result.replace('{thi}', String(tempHi));
    result = result.replace('{tlo}', String(tempLo));
    result = result.replace('{cond}', Weather.getCondition(weather.code, lang) || '--');
    result = result.replace('{cond_day}', Weather.getCondition(weather.codeDay, lang) || '--');
    result = result.replace('{hum}', String(Math.round(weather.hum)));
    result = result.replace('{wind}', String(wind));
    result = result.replace('{uv}', String(Math.round(weather.uv)));
    result = result.replace('{rain}', String(rain));
    result = result.replace('{pop}', String(Math.round(weather.pop)));
    result = result.replace('{dew}', String(dew));
    result = result.replace('{temp_unit}', isImperial ? '°F' : '°C');
    result = result.replace('{wind_unit}', isImperial ? 'MPH' : 'KM/H');
    result = result.replace('{wind_dir}', Weather.getCardinal(weather.wind_dir, lang));
  } else {
    // No weather data yet — replace with placeholders so the watch shows something
    var dash = '--';
    ['temp', 'thi', 'tlo', 'cond', 'cond_day', 'hum', 'wind', 'uv', 'rain', 'pop', 'dew', 'temp_unit', 'wind_unit', 'wind_dir'].forEach(function (t) {
      result = result.replace('{' + t + '}', dash);
    });
  }

  // Universal translation token substitution {t:KEY}
  result = result.replace(/\{t:([A-Z_]+)\}/g, function (match, key) {
    return L.labels[key] || match;
  });

  return result;
}

// ---- Build and send all data to watch ----

function sendDataToWatch() {
  var settings = cachedSettings;
  if (!settings) {
    console.log('No settings cached yet, using defaults');
    settings = {};
  }

  var isImperial = (settings.SETTING_TEMP_UNIT === 1);
  var lang = settings.SETTING_LANGUAGE || 0;
  var use24h = resolveUse24h(settings);
  var weather = Weather.isDisplayable(cachedWeather) ? cachedWeather : null;
  var defaultWidgets = getDefaultWidgets();

  if (cachedWeather && !weather) {
    cachedWeather = null;
    Weather.clear();
  }

  var msg = {};
  msg.SETTING_INFO_LAYOUT = settings.SETTING_INFO_LAYOUT || DEFAULT_INFO_LAYOUT;

  WIDGET_SLOT_KEYS.forEach(function (key) {
    // Use configured setting, falling back to default format string
    var fmt = settings[key];
    if (fmt === undefined || fmt === null) {
      fmt = defaultWidgets[key];
    }
    if (fmt !== undefined && fmt !== null) {
      // Apply JS tokens; C tokens pass through untouched
      var processed = applyJsTokens(fmt, weather, cachedSolar, isImperial, use24h, lang);
      msg[key] = processed;
    }
  });

  applyAltCityMessage(msg, settings, 'SETTING_ALT_CITY', 'SETTING_ALT_LABEL',
    'SETTING_ALT_CITY_LABEL', 'ALT_CITY_UTC_OFFSET', DEFAULT_ALT_CITY);
  applyAltCityMessage(msg, settings, 'SETTING_ALT_CITY2', 'SETTING_ALT_LABEL2',
    'SETTING_ALT_CITY2_LABEL', 'ALT_CITY2_UTC_OFFSET', DEFAULT_ALT_CITY2);

  // Send temp unit setting
  msg['SETTING_TEMP_UNIT'] = isImperial ? 1 : 0;

  // Send the language so the watch renders the date in the same language the
  // weather condition was localized into here. Without this, the date (watch
  // side, from globalSettings.language) and the weather (baked in here using
  // `lang`) can desync — e.g. after a reinstall the watch defaults to English
  // while the phone still has French cached, giving an English date next to a
  // French weather string. English (0) is the default everywhere.
  msg['SETTING_LANGUAGE'] = parseInt(lang, 10) || 0;

  console.log('Sending to watch: ' + JSON.stringify(msg));

  Pebble.sendAppMessage(msg,
    function () { console.log('Data sent to watch successfully'); },
    function (e) { console.log('Error sending data to watch: ' + JSON.stringify(e)); }
  );
}

// ---- Location + weather fetch ----
//
// Backoff: on any failure (geolocation or weather fetch), schedule a JS-side
// retry at 1m → 5m → 15m, then give up and let the next watch-driven
// REQUEST_UPDATE (every 30m) take over. Reset on success.
//
// JS timers can be killed by the phone suspending PKJS, so we still rely on
// the watch's heartbeat as the ultimate floor — backoff is best-effort.
var RETRY_DELAYS_MS = [60 * 1000, 5 * 60 * 1000, 15 * 60 * 1000];
var retryAttempt = 0;
var retryTimer = null;

function scheduleRetry(reason) {
  if (retryAttempt >= RETRY_DELAYS_MS.length) {
    console.log('Backoff exhausted (' + reason + '); waiting for next REQUEST_UPDATE');
    return;
  }
  var delay = RETRY_DELAYS_MS[retryAttempt];
  retryAttempt++;
  console.log('Scheduling retry #' + retryAttempt + ' in ' + (delay / 1000) + 's (' + reason + ')');
  if (retryTimer) clearTimeout(retryTimer);
  retryTimer = setTimeout(function () {
    retryTimer = null;
    getLocation();
  }, delay);
}

function resetBackoff() {
  if (retryTimer) {
    clearTimeout(retryTimer);
    retryTimer = null;
  }
  retryAttempt = 0;
}

function locationError(err) {
  console.log('Location error: ' + err.message);
  scheduleRetry('geolocation: ' + err.message);
}

function locationSuccess(pos) {
  var lat = pos.coords.latitude;
  var lng = pos.coords.longitude;

  // Calculate sunrise/sunset on the JS side
  var times = SunCalc.getTimes(new Date(), lat, lng);
  var solar = {
    sunriseMinute: times.sunrise.getHours() * 60 + times.sunrise.getMinutes(),
    sunsetMinute: times.sunset.getHours() * 60 + times.sunset.getMinutes()
  };
  cachedSolar = solar;
  localStorage.setItem('solarEarthSolar', JSON.stringify(solar));
  console.log('Solar: sunrise=' + solar.sunriseMinute + ', sunset=' + solar.sunsetMinute);

  // Send to watch immediately (even before weather)
  sendDataToWatch();

  if (!settingsUseWeather(cachedSettings)) {
    console.log('No weather widgets configured; skipping weather fetch');
    resetBackoff();
    return;
  }

  Weather.fetch(lat, lng,
    function (data) {
      cachedWeather = data;
      resetBackoff();
      sendDataToWatch();
    },
    function (reason) {
      scheduleRetry('weather: ' + reason);
    }
  );

  // NOTE: No setInterval here! The watch sends REQUEST_UPDATE on a timer,
  // which triggers getLocation() → this function again. That approach is
  // reliable because the watch timer always runs, unlike JS setInterval
  // which can be killed when the phone suspends the PKJS runtime.
}

function getLocation() {
  navigator.geolocation.getCurrentPosition(
    locationSuccess,
    locationError,
    { timeout: 15000, maximumAge: 10 * 60 * 1000 }
  );
}

// ---- App lifecycle ----

Pebble.addEventListener('ready', function (e) {
  console.log('PebbleKit JS ready');

  // Restore cached weather/solar from previous session
  cachedWeather = Weather.restore();
  var savedSolar = localStorage.getItem('solarEarthSolar');
  if (savedSolar) {
    try { cachedSolar = JSON.parse(savedSolar); } catch (e) { }
  }

  // Restore settings
  var savedSettings = localStorage.getItem('solarEarthSettings');
  if (savedSettings) {
    try { cachedSettings = JSON.parse(savedSettings); } catch (e) { }
  }

  // If we have cached data, send it immediately so the watch has something
  // (uses defaults if no settings configured yet)
  sendDataToWatch();

  // Then kick off a fresh location + weather fetch
  getLocation();
});

// ---- Watch-initiated heartbeat ----
// The watch sends REQUEST_UPDATE every ~30 minutes to request fresh data.
// It also includes its 24h time format preference.
Pebble.addEventListener('appmessage', function (e) {
  console.log('Received message from watch: ' + JSON.stringify(e.payload));

  if (e.payload['REQUEST_UPDATE']) {
    // Extract 24h preference from the watch
    if (e.payload['WATCH_IS_24H'] !== undefined) {
      cachedIs24h = !!e.payload['WATCH_IS_24H'];
      saveIs24h(cachedIs24h);
      console.log('Watch 24h mode: ' + cachedIs24h);
    }

    // Fresh heartbeat from the watch — start a new backoff cycle.
    resetBackoff();
    getLocation();
  }
});

// ---- Configuration ----

function safeScriptJson(value) {
  return JSON.stringify(value).replace(/</g, '\\u003c');
}

function buildLocalConfigHtml(settings) {
  var settingsJson = safeScriptJson(settings);
  var widgetOptionsJson = safeScriptJson(CONFIG_WIDGET_OPTIONS);
  var languagesJson = safeScriptJson(CONFIG_LANGUAGES);
  return [
    '<!doctype html><html><head><meta charset="utf-8">',
    '<meta name="viewport" content="width=device-width,initial-scale=1">',
    '<title>Solar Earth</title>',
    '<style>',
    'body{margin:0;font:16px -apple-system,BlinkMacSystemFont,Segoe UI,sans-serif;background:#111;color:#f5f5f5}',
    'main{max-width:520px;margin:0 auto;padding:18px 16px 28px}',
    'h1{font-size:22px;margin:0 0 18px}',
    'section{border-top:1px solid #333;padding:16px 0}',
    'label{display:block;font-weight:650;margin:0 0 8px}',
    '.row{display:flex;align-items:center;gap:12px}',
    'select,input[type=text]{width:100%;min-height:44px;box-sizing:border-box;background:#202020;color:#fff;border:1px solid #444;border-radius:6px;padding:0 10px;font:inherit}',
    'input[type=checkbox]{width:24px;height:24px}',
    '.choice label{font-weight:500;margin:10px 0}',
    '.hint{color:#bbb;font-size:13px;margin-top:6px}',
    '#infoList{display:flex;flex-direction:column;gap:10px}',
    '.info-row{border:1px solid #333;border-radius:8px;padding:10px;background:#171717}',
    '.info-grid{display:grid;grid-template-columns:1fr 96px 70px;gap:8px}',
    '.info-colors{display:flex;flex-direction:column;gap:12px;margin-top:10px}',
    '.cc{display:block;font-weight:500;font-size:13px;margin:0}',
    '.cc>span{display:block;margin-bottom:6px;color:#bbb}',
    '.info-actions{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin-top:8px}',
    '.info-actions button,#addInfo{min-height:38px;background:#2b2b2b;color:#fff}',
    '.info-actions button:disabled,#addInfo:disabled{opacity:.35}',
    '.fmt{margin-top:8px}',
    '.info-group{border-top:1px dashed #2a2a2a;padding-top:10px}',
    '.group-title{font-weight:700;font-size:12px;color:#ff9a3c;text-transform:uppercase;letter-spacing:.6px;margin:2px 0 8px}',
    '.actions{display:flex;gap:10px;margin-top:20px}',
    'button{flex:1;min-height:46px;border:0;border-radius:8px;font:inherit;font-weight:750}',
    '.save{background:#ff7a00;color:#fff}.cancel{background:#333;color:#fff}',
    '.lbl-row{display:flex;align-items:center;justify-content:space-between;margin:0 0 8px}',
    '.lbl-row label{margin:0}',
    '.help-btn{flex:0 0 auto;width:30px;height:30px;min-height:30px;border-radius:50%;background:#2b2b2b;color:#fff;font-weight:700;border:1px solid #444;padding:0}',
    '.help-overlay{position:fixed;inset:0;background:rgba(0,0,0,.6);display:none;z-index:50;padding:16px;box-sizing:border-box;overflow:auto}',
    '.help-overlay.open{display:block}',
    '.help-modal{max-width:520px;margin:18px auto;background:#171717;border:1px solid #333;border-radius:10px;padding:16px}',
    '.help-modal h2{font-size:18px;margin:0 0 4px}',
    '.help-close{float:right;width:34px;height:34px;min-height:34px;background:#2b2b2b;color:#fff;border-radius:6px;border:1px solid #444;font-size:18px}',
    '.help-cat{font-weight:700;font-size:12px;color:#ff9a3c;text-transform:uppercase;letter-spacing:.6px;margin:14px 0 6px}',
    '.help-item{padding:6px 0;border-top:1px solid #262626}',
    '.help-item code{background:#202020;border:1px solid #333;border-radius:4px;padding:1px 6px;font-size:13px;color:#9ad}',
    '.help-desc{display:block;color:#bbb;font-size:13px;margin-top:3px}',
    '.swatches{display:grid;grid-template-columns:repeat(8,1fr);gap:6px}',
    '.swatch{aspect-ratio:1;border-radius:6px;border:2px solid #2a2a2a;cursor:pointer;box-sizing:border-box}',
    '.swatch.sel{border-color:#fff;box-shadow:0 0 0 2px #ff7a00}',
    '</style></head><body><main>',
    '<h1>Solar Earth</h1>',
    '<section><label>Background color</label><div id="bgSwatches" class="swatches"></div></section>',
    '<section><div class="lbl-row"><label>Displayed info</label>',
    '<button id="helpBtn" type="button" class="help-btn" aria-label="Variable help">?</button></div><div id="infoList"></div>',
    '<button id="addInfo" type="button">Add info</button>',
    '<div class="hint">Up to 5 lines including the time. Pick where each line sits, its text size (S/M/L/XL) and its colour; the time is always larger than the others. The time line cannot be removed.</div></section>',
    '<section><label for="lang">Date language</label><select id="lang"></select></section>',
    '<section><label for="timeFormat">Time format</label><select id="timeFormat">',
    '<option value="0">Watch default</option>',
    '<option value="1">24-hour</option>',
    '<option value="2">12-hour</option>',
    '<option value="3">12-hour (AM/PM)</option>',
    '</select></section>',
    '<section><label for="tempUnit">Temperature unit</label><select id="tempUnit">',
    '<option value="0">Celsius</option><option value="1">Fahrenheit</option>',
    '</select></section>',
    '<section><label for="region">Map location</label><select id="region">',
    '<option value="0">Europe</option>',
    '<option value="1">Africa</option>',
    '<option value="2">North America</option>',
    '<option value="3">South America</option>',
    '<option value="4">Asia</option>',
    '<option value="5">Oceania</option>',
    '</select></section>',
    '<section><label for="earthInterval">Globe refresh</label><select id="earthInterval">',
    '<option value="1">Every 1 min</option>',
    '<option value="5">Every 5 min</option>',
    '<option value="15">Every 15 min</option>',
    '<option value="30">Every 30 min</option>',
    '<option value="60">Every 60 min</option>',
    '</select>',
    '<div class="hint">How often the day/night shading on the globe is recomputed. Lower values track the sun more smoothly; higher values save battery.</div></section>',
    '<div class="actions"><button class="cancel" id="cancel" type="button">Cancel</button>',
    '<button class="save" id="save" type="button">Save</button></div>',
    '<div id="helpOverlay" class="help-overlay"><div class="help-modal">',
    '<button id="helpClose" type="button" class="help-close">×</button>',
    '<h2>Text variables</h2>',
    '<div class="hint">Mix plain text with variables in braces. Each is replaced with live data from the watch. Use {t:KEY} for translated labels (e.g. {t:STEPS}).</div>',
    '<div id="helpBody"></div></div></div>',
    '</main><script>',
    'var settings=' + settingsJson + ';',
    'var widgetOptions=' + widgetOptionsJson + ';',
    'var widgetHelp=' + safeScriptJson(CONFIG_WIDGET_HELP) + ';',
    'var languages=' + languagesJson + ';',
    'var DEFAULT_INFO_LAYOUT="' + DEFAULT_INFO_LAYOUT + '";',
    'var slotKeys=' + safeScriptJson(SLOT_KEY_BY_ID) + ';',
    'var optionalIds=[0,1,3,4];',
    'var rows=[];',
    'function $(id){return document.getElementById(id)}',
    'function hex(v){v=String(v||"000000").replace("#","").toUpperCase();return /^[0-9A-F]{6}$/.test(v)?v:"000000"}',
    // The 64 colors the Pebble can display: each RGB channel has 4 levels.
    'var PEBBLE_LEVELS=["00","55","AA","FF"];',
    'var PEBBLE_COLORS=[];for(var pr=0;pr<4;pr++){for(var pg=0;pg<4;pg++){for(var pb=0;pb<4;pb++){PEBBLE_COLORS.push(PEBBLE_LEVELS[pr]+PEBBLE_LEVELS[pg]+PEBBLE_LEVELS[pb])}}}',
    // Clay-style swatch grid: tap a Pebble color to select it; returns a state holder.
    'function buildSwatches(containerId,initialHex){var cont=$(containerId);var state={value:hex(initialHex)};PEBBLE_COLORS.forEach(function(c){var sw=document.createElement("div");sw.className=(c===state.value)?"swatch sel":"swatch";sw.style.background="#"+c;sw.title="#"+c;sw.onclick=function(){state.value=c;var kids=cont.children;for(var i=0;i<kids.length;i++){kids[i].className="swatch"}sw.className="swatch sel"};cont.appendChild(sw)});return state}',
    'function num(v,d){v=parseInt(v,10);return isNaN(v)?d:v}',
    'function clampText(v){return String(v||"").slice(0,47)}',
    'function hasId(id){for(var i=0;i<rows.length;i++){if(rows[i].id===id)return true}return false}',
    'function firstUnusedSlot(){for(var i=0;i<optionalIds.length;i++){if(!hasId(optionalIds[i]))return optionalIds[i]}return null}',
    'function parseLayout(s){var out=[],seen={};String(s||DEFAULT_INFO_LAYOUT).split(",").forEach(function(part){var m=/^([0-4]):([0-2])(?::([0-3]))?$/.exec(part);if(!m)return;var id=num(m[1],-1),g=num(m[2],1),sz=num(m[3],1);if(seen[id])return;seen[id]=1;if(id!==2&&!settings[slotKeys[id]])return;out.push({id:id,group:g,size:sz})});if(!seen[2])out.splice(Math.min(2,out.length),0,{id:2,group:1,size:1});return out.slice(0,5)}',
    'function optionValueFor(v){for(var i=0;i<widgetOptions.length;i++){if(widgetOptions[i].v===v)return v}return "__custom__"}',
    'function fillSelect(sel,value){for(var i=0;i<widgetOptions.length;i++){var o=document.createElement("option");o.value=widgetOptions[i].v;o.textContent=widgetOptions[i].l;sel.appendChild(o)}sel.value=optionValueFor(value)}',
    'function groupSelect(value){var sel=document.createElement("select");[["0","Top"],["1","Center"],["2","Bottom"]].forEach(function(g){var o=document.createElement("option");o.value=g[0];o.textContent=g[1];sel.appendChild(o)});sel.value=String(value);return sel}',
    'function sizeSelect(value){var sel=document.createElement("select");[["0","S"],["1","M"],["2","L"],["3","XL"]].forEach(function(g){var o=document.createElement("option");o.value=g[0];o.textContent=g[1];sel.appendChild(o)});sel.value=String(value);return sel}',
    'function colorField(label,key){var l=document.createElement("label");l.className="cc";var s=document.createElement("span");s.textContent=label;var g=document.createElement("div");g.className="swatches";PEBBLE_COLORS.forEach(function(c){var sw=document.createElement("div");sw.className=(c===hex(settings[key]))?"swatch sel":"swatch";sw.style.background="#"+c;sw.title="#"+c;sw.onclick=function(){settings[key]=c;var k=g.children;for(var i=0;i<k.length;i++){k[i].className="swatch"}sw.className="swatch sel"};g.appendChild(sw)});l.appendChild(s);l.appendChild(g);return l}',
    'function groupIndices(g){var a=[];for(var i=0;i<rows.length;i++){if(rows[i].group===g)a.push(i)}return a}',
    'function moveWithin(idx,dir){var order=groupIndices(rows[idx].group);var pos=order.indexOf(idx);var sw=order[pos+dir];if(sw===undefined)return;var t=rows[idx];rows[idx]=rows[sw];rows[sw]=t;renderRows()}',
    'function makeCard(idx,pos,len){var row=rows[idx];var card=document.createElement("div");card.className="info-row";var grid=document.createElement("div");grid.className="info-grid";var info=document.createElement("select");var fmt=document.createElement("input");fmt.type="text";fmt.className="fmt";fmt.maxLength=47;if(row.id===2){var t=document.createElement("option");t.value="time";t.textContent="Time";info.appendChild(t);info.disabled=true;fmt.value="Time";fmt.disabled=true}else{var key=slotKeys[row.id];fmt.value=clampText(settings[key]);fillSelect(info,settings[key]);info.onchange=function(){if(info.value!=="__custom__"){fmt.value=info.value;settings[key]=fmt.value}else{fmt.focus()}};fmt.oninput=function(){settings[key]=clampText(fmt.value);info.value=optionValueFor(settings[key])}}var grp=groupSelect(row.group);grp.onchange=function(){row.group=num(grp.value,1);renderRows()};var szl=sizeSelect(row.size);szl.onchange=function(){row.size=num(szl.value,1)};grid.appendChild(info);grid.appendChild(grp);grid.appendChild(szl);card.appendChild(grid);card.appendChild(fmt);var crow=document.createElement("div");crow.className="info-colors";crow.appendChild(colorField("Text","SETTING_LINE_COLOR_"+row.id));crow.appendChild(colorField("Outline","SETTING_LINE_OUTLINE_"+row.id));card.appendChild(crow);var acts=document.createElement("div");acts.className="info-actions";var up=document.createElement("button");up.type="button";up.textContent="Up";up.disabled=pos===0;up.onclick=function(){moveWithin(idx,-1)};var down=document.createElement("button");down.type="button";down.textContent="Down";down.disabled=pos===len-1;down.onclick=function(){moveWithin(idx,1)};var del=document.createElement("button");del.type="button";del.textContent="Delete";del.disabled=row.id===2;del.onclick=function(){if(row.id!==2){settings[slotKeys[row.id]]="";rows.splice(idx,1);renderRows()}};acts.appendChild(up);acts.appendChild(down);acts.appendChild(del);card.appendChild(acts);return card}',
    'function renderRows(){var list=$("infoList");list.innerHTML="";var groups=[[0,"Top"],[1,"Center"],[2,"Bottom"]];for(var gi=0;gi<groups.length;gi++){var g=groups[gi][0];var sec=document.createElement("div");sec.className="info-group";var h=document.createElement("div");h.className="group-title";h.textContent=groups[gi][1];sec.appendChild(h);var order=groupIndices(g);if(order.length===0){var em=document.createElement("div");em.className="hint";em.textContent="\\u2014";sec.appendChild(em)}for(var k=0;k<order.length;k++){sec.appendChild(makeCard(order[k],k,order.length))}list.appendChild(sec)}$("addInfo").disabled=rows.length>=5||firstUnusedSlot()===null}',
    'function saveRows(){var active={};var parts=[];for(var i=0;i<rows.length;i++){var r=rows[i];if(r.id===2){parts.push("2:"+num(r.group,1)+":"+num(r.size,1));continue}var key=slotKeys[r.id];settings[key]=clampText(settings[key]);if(settings[key]){active[r.id]=1;parts.push(r.id+":"+num(r.group,1)+":"+num(r.size,1))}}for(var j=0;j<optionalIds.length;j++){if(!active[optionalIds[j]])settings[slotKeys[optionalIds[j]]]="";}settings.SETTING_INFO_LAYOUT=parts.slice(0,5).join(",")}',
    'var bgState=buildSwatches("bgSwatches",settings.SETTING_BG_COLOR);',
    '$("region").value=String(num(settings.SETTING_REGION,0));',
    '$("tempUnit").value=String(num(settings.SETTING_TEMP_UNIT,0));',
    '$("timeFormat").value=String(num(settings.SETTING_TIME_FORMAT,0));',
    '$("earthInterval").value=String(num(settings.SETTING_EARTH_UPDATE_INTERVAL,5));',
    'for(var li=0;li<languages.length;li++){var lo=document.createElement("option");lo.value=String(li);lo.textContent=languages[li];$("lang").appendChild(lo)}',
    '$("lang").value=String(num(settings.SETTING_LANGUAGE,0));',
    'rows=parseLayout(settings.SETTING_INFO_LAYOUT);renderRows();',
    '$("addInfo").onclick=function(){var id=firstUnusedSlot();if(id===null||rows.length>=5)return;settings[slotKeys[id]]="{local_date}";rows.push({id:id,group:1,size:1});renderRows()};',
    'function renderHelp(){var b=$("helpBody");if(b.childNodes.length)return;for(var i=0;i<widgetHelp.length;i++){var g=widgetHelp[i];var h=document.createElement("div");h.className="help-cat";h.textContent=g.cat;b.appendChild(h);for(var j=0;j<g.items.length;j++){var it=g.items[j];var row=document.createElement("div");row.className="help-item";var c=document.createElement("code");c.textContent=it.tok;row.appendChild(c);var d=document.createElement("span");d.className="help-desc";d.textContent=it.desc;row.appendChild(d);b.appendChild(row)}}}',
    'function openHelp(){renderHelp();$("helpOverlay").classList.add("open")}',
    'function closeHelp(){$("helpOverlay").classList.remove("open")}',
    '$("helpBtn").onclick=openHelp;',
    '$("helpClose").onclick=closeHelp;',
    '$("helpOverlay").onclick=function(e){if(e.target===$("helpOverlay"))closeHelp()};',
    '$("cancel").onclick=function(){location.href="pebblejs://close#CANCELLED"};',
    '$("save").onclick=function(){',
    'saveRows();',
    'settings.SETTING_BG_COLOR=hex(bgState.value);',
    'settings.SETTING_REGION=num($("region").value,0);',
    'settings.SETTING_TEMP_UNIT=num($("tempUnit").value,0);',
    'settings.SETTING_TIME_FORMAT=num($("timeFormat").value,0);',
    'settings.SETTING_EARTH_UPDATE_INTERVAL=num($("earthInterval").value,5);',
    'settings.SETTING_LANGUAGE=num($("lang").value,0);',
    'location.href="pebblejs://close#"+encodeURIComponent(JSON.stringify(settings));',
    '};',
    '</script></body></html>'
  ].join('');
}

Pebble.addEventListener('showConfiguration', function () {
  var html = buildLocalConfigHtml(getConfigSettings());
  var url = 'data:text/html;charset=utf-8,' + encodeURIComponent(html);
  console.log('Opening local Config URL');
  Pebble.openURL(url);
});

Pebble.addEventListener('webviewclosed', function (e) {
  console.log('Configuration window closed');

  if (!e.response || e.response === 'CANCELLED' || e.response === 'null' || e.response === '{}') {
    console.log('No configuration data returned');
    return;
  }

  var configData;
  try {
    configData = JSON.parse(decodeURIComponent(e.response));
  } catch (err) {
    console.log('Error parsing configuration response: ' + err);
    try {
      configData = JSON.parse(e.response);
    } catch (err2) {
      console.log('Failed to parse config data even without decoding');
      return;
    }
  }

  if (configData.return_to) {
    delete configData.return_to;
  }

  configData = mergeObjects(getDefaultConfigSettings(), cachedSettings || {}, configData);

  // Save to localStorage for persistence
  localStorage.setItem('solarEarthSettings', JSON.stringify(configData));
  cachedSettings = configData;

  // Convert to proper format and send to watch
  var dict = {};

  var colorKeys = [
    'SETTING_BG_COLOR',
    'SETTING_LINE_COLOR_0', 'SETTING_LINE_COLOR_1', 'SETTING_LINE_COLOR_2',
    'SETTING_LINE_COLOR_3', 'SETTING_LINE_COLOR_4',
    'SETTING_LINE_OUTLINE_0', 'SETTING_LINE_OUTLINE_1', 'SETTING_LINE_OUTLINE_2',
    'SETTING_LINE_OUTLINE_3', 'SETTING_LINE_OUTLINE_4'
  ];

  // Widget string keys — these get Pass 1 applied instead of raw send
  var widgetKeys = [
    'SETTING_WIDGET_UPPER_SECONDARY',
    'SETTING_WIDGET_UPPER_PRIMARY',
    'SETTING_WIDGET_LOWER_PRIMARY',
    'SETTING_WIDGET_LOWER_SECONDARY'
  ];

  // Process color settings
  for (var i = 0; i < colorKeys.length; i++) {
    var key = colorKeys[i];
    if (configData[key]) {
      dict[key] = parseInt(configData[key].replace('#', ''), 16);
    }
  }

  // Process non-color, non-widget settings
  Object.keys(configData).forEach(function (key) {
    if (colorKeys.indexOf(key) === -1 && widgetKeys.indexOf(key) === -1 &&
      key !== 'SETTING_ALT_CITY' && key !== 'SETTING_ALT_LABEL' &&
      key !== 'SETTING_ALT_CITY2' && key !== 'SETTING_ALT_LABEL2') {
      var value = configData[key];
      if (typeof value === 'boolean') {
        dict[key] = value ? 1 : 0;
      } else if (typeof value === 'string' && !isNaN(value)) {
        dict[key] = parseInt(value, 10);
      } else {
        dict[key] = value;
      }
    }
  });

  console.log('Sending non-widget config to Pebble: ' + JSON.stringify(dict));

  Pebble.sendAppMessage(dict,
    function () { console.log('Config sent successfully!'); },
    function (e) { console.log('Error sending config: ' + JSON.stringify(e)); }
  );

  // Now apply Pass 1 to widget strings and send them with current weather/solar data
  sendDataToWatch();
});
