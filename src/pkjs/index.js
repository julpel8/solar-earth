var SunCalc = require('./suncalc');
var Weather = require('./weather');
var Languages = require('./languages');
var Cities = require('./cities');

// Cached data (in-memory; also persisted to localStorage)
var cachedWeather = null;
var cachedSolar = null;
var cachedSettings = null;

var TIME_FORMAT_STORAGE_KEY = 'halcyonIs24h';
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

var DEFAULT_INFO_LAYOUT = '0:0,1:0,2:1,3:2,4:2';

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
    SETTING_THEME: '0',
    SETTING_NIGHT_THEME: '0',
    SETTING_TIME_COLOR: '000000',
    SETTING_SUBTEXT_PRIMARY_COLOR: '000000',
    SETTING_SUBTEXT_SECONDARY_COLOR: '555555',
    SETTING_BG_COLOR: '000000',
    SETTING_PIP_COLOR_PRIMARY: '000000',
    SETTING_PIP_COLOR_SECONDARY: 'AAAAAA',
    SETTING_RING_STROKE_COLOR: '000000',
    SETTING_RING_NIGHT_COLOR: '0055AA',
    SETTING_RING_DAY_COLOR: '00AAFF',
    SETTING_RING_SUNRISE_COLOR: 'FFAAAA',
    SETTING_RING_SUNSET_COLOR: 'FFAA00',
    SETTING_SUN_STROKE_COLOR: '000000',
    SETTING_SUN_FILL_COLOR: 'FFFF00',
    SETTING_NIGHT_TIME_COLOR: 'FFFFFF',
    SETTING_NIGHT_SUBTEXT_PRIMARY_COLOR: 'FFFFFF',
    SETTING_NIGHT_SUBTEXT_SECONDARY_COLOR: 'AAAAFF',
    SETTING_NIGHT_BG_COLOR: '000000',
    SETTING_NIGHT_PIP_COLOR_PRIMARY: 'FFFFFF',
    SETTING_NIGHT_PIP_COLOR_SECONDARY: '0055AA',
    SETTING_NIGHT_RING_STROKE_COLOR: '000000',
    SETTING_NIGHT_RING_NIGHT_COLOR: '0000AA',
    SETTING_NIGHT_RING_DAY_COLOR: '00AAFF',
    SETTING_NIGHT_RING_SUNRISE_COLOR: '0055FF',
    SETTING_NIGHT_RING_SUNSET_COLOR: '0055FF',
    SETTING_NIGHT_SUN_STROKE_COLOR: '000000',
    SETTING_NIGHT_SUN_FILL_COLOR: 'FFFFFF',
    SETTING_USE_LARGE_FONTS: 0,
    SETTING_USE_PRIMARY_WIDGET_FONT: 0,
    SETTING_USE_NIGHT_THEME: 1,
    SETTING_PIP_VISIBILITY: 0,
    SETTING_SHOW_LEADING_ZERO: 0,
    SETTING_TEMP_UNIT: 0,
    SETTING_LANGUAGE: 0,
    SETTING_ALT_CITY: 'TOKYO',
    SETTING_ALT_LABEL: 'TYO',
    SETTING_ALT_CITY2: 'UTC',
    SETTING_ALT_LABEL2: 'UTC',
    SETTING_REGION: 0,
    SETTING_SHOW_SOLAR_RING: 0,
    SETTING_TEXT_OUTLINE_STYLE: 0,
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
    var match = /^([0-4]):[0-2]$/.exec(part);
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
  var use24h = cachedIs24h;
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

  // Send solar minutes for the ring
  if (cachedSolar) {
    msg['WEATHER_SUNRISE_MINUTE'] = cachedSolar.sunriseMinute;
    msg['WEATHER_SUNSET_MINUTE'] = cachedSolar.sunsetMinute;
  }

  // Send temp unit setting
  msg['SETTING_TEMP_UNIT'] = isImperial ? 1 : 0;

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
  localStorage.setItem('halcyonSolar', JSON.stringify(solar));
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
  var savedSolar = localStorage.getItem('halcyonSolar');
  if (savedSolar) {
    try { cachedSolar = JSON.parse(savedSolar); } catch (e) { }
  }

  // Restore settings
  var savedSettings = localStorage.getItem('halcyonSettings');
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
    'select,input[type=color],input[type=text]{width:100%;min-height:44px;box-sizing:border-box;background:#202020;color:#fff;border:1px solid #444;border-radius:6px;padding:0 10px;font:inherit}',
    'input[type=checkbox]{width:24px;height:24px}',
    '.choice label{font-weight:500;margin:10px 0}',
    '.hint{color:#bbb;font-size:13px;margin-top:6px}',
    '#infoList{display:flex;flex-direction:column;gap:10px}',
    '.info-row{border:1px solid #333;border-radius:8px;padding:10px;background:#171717}',
    '.info-grid{display:grid;grid-template-columns:1fr 110px;gap:8px}',
    '.info-actions{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin-top:8px}',
    '.info-actions button,#addInfo{min-height:38px;background:#2b2b2b;color:#fff}',
    '.info-actions button:disabled,#addInfo:disabled{opacity:.35}',
    '.fmt{margin-top:8px}',
    '.info-group{border-top:1px dashed #2a2a2a;padding-top:10px}',
    '.group-title{font-weight:700;font-size:12px;color:#ff9a3c;text-transform:uppercase;letter-spacing:.6px;margin:2px 0 8px}',
    '.actions{display:flex;gap:10px;margin-top:20px}',
    'button{flex:1;min-height:46px;border:0;border-radius:8px;font:inherit;font-weight:750}',
    '.save{background:#ff7a00;color:#fff}.cancel{background:#333;color:#fff}',
    '</style></head><body><main>',
    '<h1>Solar Earth</h1>',
    '<section><label class="row"><input id="ring" type="checkbox"> Show solar ring and day/night edge</label>',
    '<div class="hint">Also hides the hour pips when turned off.</div></section>',
    '<section><label for="bg">Background color</label><input id="bg" type="color"></section>',
    '<section><label>Text style</label><div class="choice">',
    '<label><input type="radio" name="outline" value="0"> Black text, white outline</label>',
    '<label><input type="radio" name="outline" value="1"> White text, black outline</label>',
    '</div></section>',
    '<section><label>Displayed info</label><div id="infoList"></div>',
    '<button id="addInfo" type="button">Add info</button>',
    '<div class="hint">Up to 5 lines including the time. The time line cannot be removed.</div></section>',
    '<section><label for="lang">Date language</label><select id="lang"></select></section>',
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
    '<div class="actions"><button class="cancel" id="cancel" type="button">Cancel</button>',
    '<button class="save" id="save" type="button">Save</button></div>',
    '</main><script>',
    'var settings=' + settingsJson + ';',
    'var widgetOptions=' + widgetOptionsJson + ';',
    'var languages=' + languagesJson + ';',
    'var DEFAULT_INFO_LAYOUT="' + DEFAULT_INFO_LAYOUT + '";',
    'var slotKeys=' + safeScriptJson(SLOT_KEY_BY_ID) + ';',
    'var optionalIds=[0,1,3,4];',
    'var rows=[];',
    'function $(id){return document.getElementById(id)}',
    'function hex(v){v=String(v||"000000").replace("#","").toUpperCase();return /^[0-9A-F]{6}$/.test(v)?v:"000000"}',
    'function num(v,d){v=parseInt(v,10);return isNaN(v)?d:v}',
    'function clampText(v){return String(v||"").slice(0,47)}',
    'function hasId(id){for(var i=0;i<rows.length;i++){if(rows[i].id===id)return true}return false}',
    'function firstUnusedSlot(){for(var i=0;i<optionalIds.length;i++){if(!hasId(optionalIds[i]))return optionalIds[i]}return null}',
    'function parseLayout(s){var out=[],seen={};String(s||DEFAULT_INFO_LAYOUT).split(",").forEach(function(part){var m=/^([0-4]):([0-2])$/.exec(part);if(!m)return;var id=num(m[1],-1),g=num(m[2],1);if(seen[id])return;seen[id]=1;if(id!==2&&!settings[slotKeys[id]])return;out.push({id:id,group:g})});if(!seen[2])out.splice(Math.min(2,out.length),0,{id:2,group:1});return out.slice(0,5)}',
    'function optionValueFor(v){for(var i=0;i<widgetOptions.length;i++){if(widgetOptions[i].v===v)return v}return "__custom__"}',
    'function fillSelect(sel,value){for(var i=0;i<widgetOptions.length;i++){var o=document.createElement("option");o.value=widgetOptions[i].v;o.textContent=widgetOptions[i].l;sel.appendChild(o)}sel.value=optionValueFor(value)}',
    'function groupSelect(value){var sel=document.createElement("select");[["0","Top"],["1","Center"],["2","Bottom"]].forEach(function(g){var o=document.createElement("option");o.value=g[0];o.textContent=g[1];sel.appendChild(o)});sel.value=String(value);return sel}',
    'function groupIndices(g){var a=[];for(var i=0;i<rows.length;i++){if(rows[i].group===g)a.push(i)}return a}',
    'function moveWithin(idx,dir){var order=groupIndices(rows[idx].group);var pos=order.indexOf(idx);var sw=order[pos+dir];if(sw===undefined)return;var t=rows[idx];rows[idx]=rows[sw];rows[sw]=t;renderRows()}',
    'function makeCard(idx,pos,len){var row=rows[idx];var card=document.createElement("div");card.className="info-row";var grid=document.createElement("div");grid.className="info-grid";var info=document.createElement("select");var fmt=document.createElement("input");fmt.type="text";fmt.className="fmt";fmt.maxLength=47;if(row.id===2){var t=document.createElement("option");t.value="time";t.textContent="Time";info.appendChild(t);info.disabled=true;fmt.value="Time";fmt.disabled=true}else{var key=slotKeys[row.id];fmt.value=clampText(settings[key]);fillSelect(info,settings[key]);info.onchange=function(){if(info.value!=="__custom__"){fmt.value=info.value;settings[key]=fmt.value}else{fmt.focus()}};fmt.oninput=function(){settings[key]=clampText(fmt.value);info.value=optionValueFor(settings[key])}}var grp=groupSelect(row.group);grp.onchange=function(){row.group=num(grp.value,1);renderRows()};grid.appendChild(info);grid.appendChild(grp);card.appendChild(grid);card.appendChild(fmt);var acts=document.createElement("div");acts.className="info-actions";var up=document.createElement("button");up.type="button";up.textContent="Up";up.disabled=pos===0;up.onclick=function(){moveWithin(idx,-1)};var down=document.createElement("button");down.type="button";down.textContent="Down";down.disabled=pos===len-1;down.onclick=function(){moveWithin(idx,1)};var del=document.createElement("button");del.type="button";del.textContent="Delete";del.disabled=row.id===2;del.onclick=function(){if(row.id!==2){settings[slotKeys[row.id]]="";rows.splice(idx,1);renderRows()}};acts.appendChild(up);acts.appendChild(down);acts.appendChild(del);card.appendChild(acts);return card}',
    'function renderRows(){var list=$("infoList");list.innerHTML="";var groups=[[0,"Top"],[1,"Center"],[2,"Bottom"]];for(var gi=0;gi<groups.length;gi++){var g=groups[gi][0];var sec=document.createElement("div");sec.className="info-group";var h=document.createElement("div");h.className="group-title";h.textContent=groups[gi][1];sec.appendChild(h);var order=groupIndices(g);if(order.length===0){var em=document.createElement("div");em.className="hint";em.textContent="\\u2014";sec.appendChild(em)}for(var k=0;k<order.length;k++){sec.appendChild(makeCard(order[k],k,order.length))}list.appendChild(sec)}$("addInfo").disabled=rows.length>=5||firstUnusedSlot()===null}',
    'function saveRows(){var active={};var parts=[];for(var i=0;i<rows.length;i++){var r=rows[i];if(r.id===2){parts.push("2:"+num(r.group,1));continue}var key=slotKeys[r.id];settings[key]=clampText(settings[key]);if(settings[key]){active[r.id]=1;parts.push(r.id+":"+num(r.group,1))}}for(var j=0;j<optionalIds.length;j++){if(!active[optionalIds[j]])settings[slotKeys[optionalIds[j]]]="";}settings.SETTING_INFO_LAYOUT=parts.slice(0,5).join(",")}',
    '$("ring").checked=num(settings.SETTING_SHOW_SOLAR_RING,0)!==0;',
    '$("bg").value="#"+hex(settings.SETTING_BG_COLOR||settings.SETTING_NIGHT_BG_COLOR);',
    '$("region").value=String(num(settings.SETTING_REGION,0));',
    '$("tempUnit").value=String(num(settings.SETTING_TEMP_UNIT,0));',
    'for(var li=0;li<languages.length;li++){var lo=document.createElement("option");lo.value=String(li);lo.textContent=languages[li];$("lang").appendChild(lo)}',
    '$("lang").value=String(num(settings.SETTING_LANGUAGE,0));',
    'var style=String(num(settings.SETTING_TEXT_OUTLINE_STYLE,0));',
    'var picked=document.querySelector("input[name=outline][value=\\"" + style + "\\"]");',
    'if(picked)picked.checked=true;',
    'rows=parseLayout(settings.SETTING_INFO_LAYOUT);renderRows();',
    '$("addInfo").onclick=function(){var id=firstUnusedSlot();if(id===null||rows.length>=5)return;settings[slotKeys[id]]="{local_date}";rows.push({id:id,group:1});renderRows()};',
    '$("cancel").onclick=function(){location.href="pebblejs://close#CANCELLED"};',
    '$("save").onclick=function(){',
    'saveRows();',
    'settings.SETTING_SHOW_SOLAR_RING=$("ring").checked?1:0;',
    'var bg=hex($("bg").value);settings.SETTING_BG_COLOR=bg;settings.SETTING_NIGHT_BG_COLOR=bg;',
    'var o=document.querySelector("input[name=outline]:checked");settings.SETTING_TEXT_OUTLINE_STYLE=o?num(o.value,0):0;',
    'settings.SETTING_REGION=num($("region").value,0);',
    'settings.SETTING_TEMP_UNIT=num($("tempUnit").value,0);',
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
  localStorage.setItem('halcyonSettings', JSON.stringify(configData));
  cachedSettings = configData;

  // Convert to proper format and send to watch
  var dict = {};

  var colorKeys = [
    'SETTING_TIME_COLOR', 'SETTING_BG_COLOR',
    'SETTING_SUBTEXT_PRIMARY_COLOR', 'SETTING_SUBTEXT_SECONDARY_COLOR',
    'SETTING_PIP_COLOR_PRIMARY', 'SETTING_PIP_COLOR_SECONDARY',
    'SETTING_RING_STROKE_COLOR', 'SETTING_RING_NIGHT_COLOR', 'SETTING_RING_DAY_COLOR',
    'SETTING_RING_SUNRISE_COLOR', 'SETTING_RING_SUNSET_COLOR',
    'SETTING_SUN_STROKE_COLOR', 'SETTING_SUN_FILL_COLOR',
    'SETTING_NIGHT_TIME_COLOR', 'SETTING_NIGHT_BG_COLOR',
    'SETTING_NIGHT_SUBTEXT_PRIMARY_COLOR', 'SETTING_NIGHT_SUBTEXT_SECONDARY_COLOR',
    'SETTING_NIGHT_PIP_COLOR_PRIMARY', 'SETTING_NIGHT_PIP_COLOR_SECONDARY',
    'SETTING_NIGHT_RING_STROKE_COLOR', 'SETTING_NIGHT_RING_NIGHT_COLOR', 'SETTING_NIGHT_RING_DAY_COLOR',
    'SETTING_NIGHT_RING_SUNRISE_COLOR', 'SETTING_NIGHT_RING_SUNSET_COLOR',
    'SETTING_NIGHT_SUN_STROKE_COLOR', 'SETTING_NIGHT_SUN_FILL_COLOR'
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
