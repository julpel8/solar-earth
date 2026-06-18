import React from 'react';
import { useConfig } from '../context/PebbleConfigContext';
import { CITIES, formatStandardOffset, getCityByName } from '../data/cities';
import { Section } from './Section';
import { Select } from './Select';
import { TextInput } from './TextInput';

type AltTimezoneCityKey = 'SETTING_ALT_CITY' | 'SETTING_ALT_CITY2';
type AltTimezoneLabelKey = 'SETTING_ALT_LABEL' | 'SETTING_ALT_LABEL2';

interface AltTimezoneSectionProps {
  title: string;
  cityKey: AltTimezoneCityKey;
  labelKey: AltTimezoneLabelKey;
}

const ALT_LABEL_MAX_LENGTH = 6;

const normalizeAltLabel = (value: string) =>
  value
    .toUpperCase()
    .replace(/[^A-Z0-9]/g, '')
    .slice(0, ALT_LABEL_MAX_LENGTH);

const cityOptions = CITIES.map((city) => ({
  label: `${city.displayName} (${formatStandardOffset(city.offset)})`,
  value: city.name,
}));

export const AltTimezoneSection: React.FC<AltTimezoneSectionProps> = ({
  title,
  cityKey,
  labelKey,
}) => {
  const { settings, updateSetting } = useConfig();
  const city = getCityByName(settings[cityKey]);

  const handleCityChange = (value: string) => {
    const nextCity = getCityByName(value);

    updateSetting(cityKey, nextCity.name);
    updateSetting(labelKey, nextCity.abbreviation);
  };

  return (
    <Section title={title}>
      <Select
        label="City"
        messageKey={cityKey}
        options={cityOptions}
        value={city.name}
        onChange={handleCityChange}
        className="halite-alt-time-control"
      />
      <TextInput
        label="Label"
        messageKey={labelKey}
        value={String(settings[labelKey] ?? city.abbreviation)}
        maxLength={ALT_LABEL_MAX_LENGTH}
        normalizeValue={normalizeAltLabel}
        spellCheck={false}
        className="halite-alt-time-control"
      />
    </Section>
  );
};
