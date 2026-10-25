import type {SettingsControls} from './useSettings';
export function useHudSettings(settings: SettingsControls) {
  const v = settings.values;
  return {ready: settings.ready, hudEnabled: v.hudEnabled, hudPosition: v.hudPosition,
    hudShowFileName: v.hudShowFileName, hudShowLayer: v.hudShowLayer,
    hudShowOverallProgress: v.hudShowOverallProgress, hudShowProgress: v.hudShowProgress,
    hudShowProjectedBlockName: v.hudShowProjectedBlockName, hudShowWrongState: v.hudShowWrongState,
    hudShowWrongType: v.hudShowWrongType, hudShowExtraBlocks: v.hudShowExtraBlocks,
    materialHudEnabled: v.materialHudEnabled, materialHudPosition: v.materialHudPosition,
    setHudEnabled: (v: boolean) => settings.change('hudEnabled', v),
    setHudPosition: (v: number) => settings.change('hudPosition', v),
    setHudShowFileName: (v: boolean) => settings.change('hudShowFileName', v),
    setHudShowLayer: (v: boolean) => settings.change('hudShowLayer', v),
    setHudShowOverallProgress: (v: boolean) => settings.change('hudShowOverallProgress', v),
    setHudShowProgress: (v: boolean) => settings.change('hudShowProgress', v),
    setHudShowProjectedBlockName: (v: boolean) => settings.change('hudShowProjectedBlockName', v),
    setHudShowWrongState: (v: boolean) => settings.change('hudShowWrongState', v),
    setHudShowWrongType: (v: boolean) => settings.change('hudShowWrongType', v),
    setHudShowExtraBlocks: (v: boolean) => settings.change('hudShowExtraBlocks', v),
    setMaterialHudEnabled: (v: boolean) => settings.change('materialHudEnabled', v),
    setMaterialHudPosition: (v: number) => settings.change('materialHudPosition', v)};
}
export type HudSettingsControls = ReturnType<typeof useHudSettings>;
