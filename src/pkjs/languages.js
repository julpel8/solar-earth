// ---- Localized strings for PebbleKit JS ----
//
// Language data lives in src/pkjs/languages/<code>.js.
// LANG_ORDER maps the numeric index sent from the watch to a language code.

var LANG_ORDER = [
  'en-us',
  'fr-fr',
  'de-de',
  'es-es',
  'it-it',
  'nl-nl',
  'tr-tr',
  'cs-cz',
  'pt-pt',
  'el-gr',
  'sv-se',
  'pl-pl',
  'sk-sk',
  'vi-vn',
  'ro-ro',
  'ca-es',
  'nb-no',
  'ru-ru',
  'et-ee',
  'eu-es',
  'fi-fi',
  'da-dk',
  'lt-lt',
  'sl-si',
  'hu-hu',
  'hr-hr',
  'ga-ie',
  'lv-lv',
  'sr-rs',
  'zh-cn',
  'id-id',
  'uk-ua',
  'cy-gb',
  'gl-es',
  'ja-jp',
  'ko-kr',
  'he-il',
  'en-gb'
];

var LANGUAGES = {
  'en-us': require('./languages/en-us'),
  'fr-fr': require('./languages/fr-fr'),
  'de-de': require('./languages/de-de'),
  'es-es': require('./languages/es-es'),
  'it-it': require('./languages/it-it'),
  'nl-nl': require('./languages/nl-nl'),
  'tr-tr': require('./languages/tr-tr'),
  'cs-cz': require('./languages/cs-cz'),
  'pt-pt': require('./languages/pt-pt'),
  'el-gr': require('./languages/el-gr'),
  'sv-se': require('./languages/sv-se'),
  'pl-pl': require('./languages/pl-pl'),
  'sk-sk': require('./languages/sk-sk'),
  'vi-vn': require('./languages/vi-vn'),
  'ro-ro': require('./languages/ro-ro'),
  'ca-es': require('./languages/ca-es'),
  'nb-no': require('./languages/nb-no'),
  'ru-ru': require('./languages/ru-ru'),
  'et-ee': require('./languages/et-ee'),
  'eu-es': require('./languages/eu-es'),
  'fi-fi': require('./languages/fi-fi'),
  'da-dk': require('./languages/da-dk'),
  'lt-lt': require('./languages/lt-lt'),
  'sl-si': require('./languages/sl-si'),
  'hu-hu': require('./languages/hu-hu'),
  'hr-hr': require('./languages/hr-hr'),
  'ga-ie': require('./languages/ga-ie'),
  'lv-lv': require('./languages/lv-lv'),
  'sr-rs': require('./languages/sr-rs'),
  'zh-cn': require('./languages/zh-cn'),
  'id-id': require('./languages/id-id'),
  'uk-ua': require('./languages/uk-ua'),
  'cy-gb': require('./languages/cy-gb'),
  'gl-es': require('./languages/gl-es'),
  'ja-jp': require('./languages/ja-jp'),
  'ko-kr': require('./languages/ko-kr'),
  'he-il': require('./languages/he-il'),
  'en-gb': require('./languages/en-gb')
};

function getLang(idx) {
  var i = parseInt(idx, 10) || 0;
  if (i < 0 || i >= LANG_ORDER.length) i = 0;
  return LANGUAGES[LANG_ORDER[i]] || LANGUAGES['en-us'];
}

module.exports = {
  getLang: getLang,
  LANG_ORDER: LANG_ORDER,
  LANGUAGES: LANGUAGES
};
