// Widget type definitions — shared between the config page Select dropdowns
// and the WatchPreview component. Previews are computed at render time so they
// reflect the user's selected language.

import { renderPreview } from './i18nPreview';

export interface WidgetOption {
  value: string;
  label: string;
  preview: string;
  category?: string;
}

export interface WidgetToken {
  token: string;
  label: string;
  category: string;
  requires?: 'health' | 'hrm';
  // Short, human-readable explanation shown in the help modal.
  description?: string;
}

export type WidgetTokenId = string;

interface WidgetOptionTemplate {
  value: string;
  label: string;
  category?: string;
}

const WIDGET_TEMPLATES: WidgetOptionTemplate[] = [
  { value: '', label: 'None' },
  // Date/time
  { value: '{local_date}', label: 'Date', category: 'Date & Time' },
  { value: '{alt_tz}', label: 'Alternate Time Zone 1', category: 'World Time' },
  { value: '{alt_tz2}', label: 'Alternate Time Zone 2', category: 'World Time' },
  { value: '{year}-{month_num}-{day0}', label: 'Numeric Date', category: 'Date & Time' },
  { value: '{t:DAY} {day_of_year}', label: 'Day Number', category: 'Date & Time' },
  { value: '{t:WEEK} {week_of_year}', label: 'Week Number', category: 'Date & Time' },
  { value: '{year}', label: 'Year', category: 'Date & Time' },
  // Solar
  { value: '{next_solar}', label: 'Next sunrise/sunset', category: 'Solar' },
  // Health
  { value: '{steps} {t:STEPS}', label: 'Steps', category: 'Health' },
  { value: '{dist} {dist_unit}', label: 'Distance Walked', category: 'Health' },
  { value: '{hr} {t:BPM}', label: 'Current Heart Rate', category: 'Health' },
  // Device
  { value: '{t:BATTERY} {batt}%', label: 'Battery %', category: 'Device' },
  // Weather
  {
    value: '{temp}° ({thi}°/{tlo}°)',
    label: 'Temperature (Current & Forecast)',
    category: 'Weather',
  },
  { value: '{cond}', label: 'Current Condition', category: 'Weather' },
  { value: '{hum}% {t:HUMIDITY}', label: 'Humidity', category: 'Weather' },
  { value: '{wind} {wind_unit} {wind_dir}', label: 'Wind', category: 'Weather' },
  { value: '{t:UV} {uv}', label: 'UV Index', category: 'Weather' },
  { value: '{t:DPT} {dew}°', label: 'Dew Point', category: 'Weather' },
  { value: '{t:RAIN} {pop}%', label: 'Chance of Rain', category: 'Weather' },
  // Custom
  { value: '__custom__', label: 'Custom…', category: 'Custom' },
];

const HEALTH_LABELS = new Set(['Steps', 'Distance Walked']);
const HRM_LABELS = new Set(['Current Heart Rate']);

export const WIDGET_TOKENS: WidgetToken[] = [
  { token: '{local_date}', label: 'Local Date', category: 'Date & Time', description: 'Full localized date, using the date format of the selected language.' },
  { token: '{alt_tz}', label: 'Alt TZ Full', category: 'World Time', description: 'Time zone 1: label, time, and day (shown only when it differs from today).' },
  { token: '{alt_tz_label}', label: 'Alt TZ Label', category: 'World Time', description: 'Time zone 1 label only.' },
  { token: '{alt_tz_time}', label: 'Alt TZ Time', category: 'World Time', description: 'Time zone 1 time only.' },
  { token: '{alt_tz_day}', label: 'Alt TZ Day', category: 'World Time', description: 'Time zone 1 weekday only.' },
  { token: '{alt_tz2}', label: 'Alt TZ 2 Full', category: 'World Time', description: 'Time zone 2: label, time, and day (shown only when it differs from today).' },
  { token: '{alt_tz2_label}', label: 'Alt TZ 2 Label', category: 'World Time', description: 'Time zone 2 label only.' },
  { token: '{alt_tz2_time}', label: 'Alt TZ 2 Time', category: 'World Time', description: 'Time zone 2 time only.' },
  { token: '{alt_tz2_day}', label: 'Alt TZ 2 Day', category: 'World Time', description: 'Time zone 2 weekday only.' },
  { token: '{day_name}', label: 'Day Name', category: 'Date & Time', description: 'Abbreviated weekday name (e.g. MON).' },
  { token: '{month_name}', label: 'Month Name', category: 'Date & Time', description: 'Abbreviated month name (e.g. JAN).' },
  { token: '{day0}', label: 'Day (Leading Zero)', category: 'Date & Time', description: 'Day of the month, zero-padded (01–31).' },
  { token: '{day}', label: 'Day', category: 'Date & Time', description: 'Day of the month (1–31).' },
  { token: '{month_num}', label: 'Month No.', category: 'Date & Time', description: 'Month number, zero-padded (01–12).' },
  { token: '{year}', label: 'Year', category: 'Date & Time', description: 'Four-digit year.' },
  { token: '{day_of_year}', label: 'Day No.', category: 'Date & Time', description: 'Day number within the year (1–366).' },
  { token: '{week_of_year}', label: 'Week No.', category: 'Date & Time', description: 'ISO week number (1–53).' },
  { token: '{sunrise}', label: 'Sunrise', category: 'Solar', description: "Today's sunrise time." },
  { token: '{sunset}', label: 'Sunset', category: 'Solar', description: "Today's sunset time." },
  { token: '{next_solar}', label: 'Next sunrise/sunset', category: 'Solar', description: 'Next solar event: label and time.' },
  { token: '{next_solar_label}', label: 'Rise/Set Label', category: 'Solar', description: 'Label of the next solar event (RISE or SET).' },
  { token: '{next_solar_time}', label: 'Solar Time', category: 'Solar', description: 'Time of the next solar event.' },
  { token: '{steps}', label: 'Steps', category: 'Health & Device', requires: 'health', description: 'Step count for today.' },
  { token: '{dist}', label: 'Distance', category: 'Health & Device', requires: 'health', description: 'Distance walked today (value only).' },
  { token: '{dist_unit}', label: 'Dist. Unit', category: 'Health & Device', requires: 'health', description: 'Distance unit (KM or MI).' },
  { token: '{hr}', label: 'Heart Rate', category: 'Health & Device', requires: 'hrm', description: 'Current heart rate (value only).' },
  { token: '{batt}', label: 'Battery', category: 'Health & Device', description: 'Battery level percentage (value only).' },
  { token: '{temp}', label: 'Temp', category: 'Weather', description: 'Current temperature (value only).' },
  { token: '{thi}', label: 'High', category: 'Weather', description: 'Forecast high temperature (value only).' },
  { token: '{tlo}', label: 'Low', category: 'Weather', description: 'Forecast low temperature (value only).' },
  { token: '{cond}', label: 'Condition', category: 'Weather', description: 'Current weather condition.' },
  { token: '{cond_day}', label: 'Day Cond.', category: 'Weather', description: 'Daytime weather condition.' },
  { token: '{hum}', label: 'Humidity', category: 'Weather', description: 'Relative humidity (value only).' },
  { token: '{wind}', label: 'Wind', category: 'Weather', description: 'Wind speed (value only).' },
  { token: '{wind_unit}', label: 'Wind Unit', category: 'Weather', description: 'Wind speed unit (KM/H or MPH).' },
  { token: '{wind_dir}', label: 'Wind Dir.', category: 'Weather', description: 'Wind direction (cardinal, e.g. NW).' },
  { token: '{uv}', label: 'UV', category: 'Weather', description: 'UV index (value only).' },
  { token: '{rain}', label: 'Rain', category: 'Weather', description: 'Rain amount (value only).' },
  { token: '{pop}', label: 'Rain %', category: 'Weather', description: 'Chance of rain percentage (value only).' },
  { token: '{dew}', label: 'Dew Point', category: 'Weather', description: 'Dew point temperature (value only).' },
  { token: '{temp_unit}', label: 'Temp Unit', category: 'Weather', description: 'Temperature unit (°C or °F).' },
];

export const ALT_TIMEZONE_WIDGET_IDS = ['alt_tz', 'alt_tz_label', 'alt_tz_time', 'alt_tz_day'];

export const ALT_TIMEZONE2_WIDGET_IDS = ['alt_tz2', 'alt_tz2_label', 'alt_tz2_time', 'alt_tz2_day'];

export const WEATHER_WIDGET_IDS = WIDGET_TOKENS.filter((token) => token.category === 'Weather').map(
  (token) => token.token,
);

const normalizeWidgetTokenId = (id: WidgetTokenId) =>
  id.startsWith('{') && id.endsWith('}') ? id : `{${id}}`;

export const containsWidgets = (
  widgetValues: Array<string | null | undefined>,
  tokenIds: WidgetTokenId[],
): boolean => {
  const tokens = tokenIds.map(normalizeWidgetTokenId);
  return widgetValues.some((value) => tokens.some((token) => value?.includes(token)));
};

export const getWidgetOptions = (
  lang: number,
  hasHealth: boolean,
  hasHrm: boolean,
  isImperial: boolean = false,
  altLabel: string = 'TYO',
  altLabel2: string = 'UTC',
): WidgetOption[] => {
  return WIDGET_TEMPLATES.filter((t) => hasHealth || !HEALTH_LABELS.has(t.label))
    .filter((t) => hasHrm || !HRM_LABELS.has(t.label))
    .map((t) => {
      const preview =
        t.value && t.value !== '__custom__'
          ? renderPreview(t.value, lang, isImperial, altLabel, altLabel2)
          : '';
      return { value: t.value, label: t.label, preview, category: t.category };
    });
};

// Look up the rendered preview for a stored format string. Used by WatchPreview
// when the saved value matches a preset (or even if it doesn't — we just
// substitute tokens against the current language).
export const getPreviewForValue = (
  value: string | undefined,
  lang: number,
  isImperial: boolean = false,
  altLabel: string = 'TYO',
  altLabel2: string = 'UTC',
): string => {
  if (!value) return '';
  if (value === '__custom__') return '';
  return renderPreview(value, lang, isImperial, altLabel, altLabel2);
};
