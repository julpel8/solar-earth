import React from 'react';
import { useConfig } from '../context/PebbleConfigContext';
import { Settings } from '../context/types';
import { FormItem } from './FormItem';

interface TextInputProps {
  label: string;
  description?: string;
  messageKey: keyof Settings;
  value?: string;
  maxLength?: number;
  normalizeValue?: (value: string) => string;
  spellCheck?: boolean;
  className?: string;
}

export const TextInput: React.FC<TextInputProps> = ({
  label,
  description,
  messageKey,
  value: controlledValue,
  maxLength,
  normalizeValue,
  spellCheck = true,
  className = '',
}) => {
  const { settings, updateSetting } = useConfig();
  const inputId = React.useId();
  const value = controlledValue ?? String(settings[messageKey] ?? '');

  return (
    <FormItem
      label={label}
      description={description}
      className={`halite-text-input ${className}`.trim()}
      htmlFor={inputId}
    >
      <input
        id={inputId}
        className="halite-input"
        value={value}
        maxLength={maxLength}
        onChange={(event) => {
          const nextValue = normalizeValue
            ? normalizeValue(event.target.value)
            : event.target.value;
          updateSetting(messageKey, nextValue);
        }}
        spellCheck={spellCheck}
      />
    </FormItem>
  );
};
