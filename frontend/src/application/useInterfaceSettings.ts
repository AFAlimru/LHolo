import { useState } from 'react';
import type { SettingsControls } from './useSettings';

const minScale = 1.0;
const maxScale = 5.0;
const stepScale = 0.1;
const defaultScale = 2.0;

const clampAndRoundScale = (value: number) => {
  const rounded = Math.round(value * 10) / 10;
  return Math.max(minScale, Math.min(maxScale, rounded));
};

const formatScale = (value: number) => value.toFixed(1);

// OreUI scale stays a local placeholder. Language codes and choices come from native state.
export function useInterfaceSettings(settings: SettingsControls) {
  const [uiScale, setUiScale] = useState({
    text: formatScale(defaultScale),
    value: defaultScale,
  });
  const languages = settings.state?.languages || [];
  const language = Math.max(0, languages.findIndex(item => item.code === settings.values.language));

  const editUiScale = (text: string) =>
    setUiScale(previous => {
      const trimmed = text.trim();
      const parsed = /^-?\d+(?:\.\d+)?$/.test(trimmed) ? Number(trimmed) : NaN;
      return {
        text,
        value: Number.isFinite(parsed)
          ? clampAndRoundScale(parsed)
          : previous.value,
      };
    });

  const commitUiScale = () =>
    setUiScale(previous => ({
      ...previous,
      text: formatScale(previous.value),
    }));

  const stepUiScale = (direction: -1 | 1) =>
    setUiScale(previous => {
      const value = clampAndRoundScale(previous.value + direction * stepScale);
      return { text: formatScale(value), value };
    });

  const setLanguage = (index: number) => {
    if (languages[index]) settings.change('language', languages[index].code);
  };

  return {
    ready: settings.ready, languages,
    uiScale,
    editUiScale,
    commitUiScale,
    stepUiScale,
    language,
    setLanguage,
  };
}

export type InterfaceSettingsControls = ReturnType<typeof useInterfaceSettings>;
