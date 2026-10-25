import type { SettingsControls } from './useSettings';

// Language codes and choices come from native state.
export function useInterfaceSettings(settings: SettingsControls) {
  const languages = settings.state?.languages || [];
  const language = Math.max(0, languages.findIndex(item => item.code === settings.values.language));
  const setLanguage = (index: number) => {
    if (languages[index]) settings.change('language', languages[index].code);
  };
  return { ready: settings.ready, languages, language, setLanguage };
}

export type InterfaceSettingsControls = ReturnType<typeof useInterfaceSettings>;
