import type {SettingsControls} from './useSettings';
export function useHotkeySettings(settings: SettingsControls) {
  return {ready: settings.ready, canCommand: settings.canCommand, bindings: settings.state?.hotkeys || [],
    capturingIndex: settings.state?.hotkeys.find(b => b.capturing)?.slot ?? null,
    beginCapture: (slot: number) => settings.command('capture', slot),
    cancelCapture: settings.cancelCapture,
    resetHotkey: (slot: number) => settings.command('resetHotkey', slot),
    clearHotkey: (slot: number) => settings.command('clearHotkey', slot),
    altWheelOffsetEnabled: settings.values.altWheelOffsetEnabled,
    setAltWheelOffsetEnabled: (v: boolean) => settings.change('altWheelOffsetEnabled', v),
    resetAllHotkeys: () => settings.command('resetHotkeys')};
}
export type HotkeySettingsControls = ReturnType<typeof useHotkeySettings>;
