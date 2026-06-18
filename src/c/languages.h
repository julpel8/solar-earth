#pragma once

#define LANGUAGE_EN 0  // English
#define LANGUAGE_FR 1  // French
#define LANGUAGE_DE 2  // German
#define LANGUAGE_ES 3  // Spanish
#define LANGUAGE_IT 4  // Italian
#define LANGUAGE_NL 5  // Dutch
#define LANGUAGE_TR 6  // Turkish
#define LANGUAGE_CS 7  // Czech
#define LANGUAGE_PT 8  // Portuguese
#define LANGUAGE_EL 9  // Greek
#define LANGUAGE_SV 10 // Swedish
#define LANGUAGE_PL 11 // Polish
#define LANGUAGE_SK 12 // Slovak
#define LANGUAGE_VI 13 // Vietnamese
#define LANGUAGE_RO 14 // Romanian
#define LANGUAGE_CA 15 // Catalan
#define LANGUAGE_NO 16 // Norwegian
#define LANGUAGE_RU 17 // Russian
#define LANGUAGE_ET 18 // Estonian
#define LANGUAGE_EU 19 // Basque
#define LANGUAGE_FI 20 // Finnish
#define LANGUAGE_DA 21 // Danish
#define LANGUAGE_LT 22 // Lithuanian
#define LANGUAGE_SL 23 // Slovenian
#define LANGUAGE_HU 24 // Hungarian
#define LANGUAGE_HR 25 // Croatian
#define LANGUAGE_GA 26 // Irish
#define LANGUAGE_LV 27 // Latvian
#define LANGUAGE_SR 28 // Serbian
#define LANGUAGE_ZH 29 // Chinese
#define LANGUAGE_ID 30 // Indonesian
#define LANGUAGE_UK 31 // Ukrainian
#define LANGUAGE_CY 32 // Welsh
#define LANGUAGE_GL 33 // Galician
#define LANGUAGE_JA 34 // Japanese
#define LANGUAGE_KO 35 // Korean
#define LANGUAGE_HE 36 // Hebrew
#define LANGUAGE_EN_GB 37 // English (UK)
#define LANGUAGE_COUNT 38

/* day names in many different languages! */
extern const char dayNames[LANGUAGE_COUNT][7][8];

/* month names in many different languages! */
extern const char monthNames[LANGUAGE_COUNT][12][8];

/* idiomatic date format string per language. Used by widgets.c to expand the
   {local_date} super-token at render time. */
extern const char *const defaultDateFormat[LANGUAGE_COUNT];

/* decimal separator per language (e.g. '.' or ',') */
extern const char decimalSeparator[LANGUAGE_COUNT];
