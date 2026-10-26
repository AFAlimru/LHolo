import type {SettingsControls} from './useSettings';
import type {MenuBridge} from '../platform/bridge';
export function useHudSettings(settings: SettingsControls, bridge: Pick<MenuBridge, 'editHudLayout'>) {
  const v = settings.values;
  return {ready: settings.ready, editLayout: bridge.editHudLayout, hudEnabled: v.hudEnabled,
    hudShowFileName: v.hudShowFileName, hudShowLayer: v.hudShowLayer,
    hudShowOverallProgress: v.hudShowOverallProgress, hudShowProgress: v.hudShowProgress,
    hudShowProjectedBlockName: v.hudShowProjectedBlockName, hudShowWrongState: v.hudShowWrongState,
    hudShowWrongType: v.hudShowWrongType, hudShowExtraBlocks: v.hudShowExtraBlocks,
    materialHudEnabled: v.materialHudEnabled,
    setHudEnabled: (v: boolean) => settings.change('hudEnabled', v),
    setHudShowFileName: (v: boolean) => settings.change('hudShowFileName', v),
    setHudShowLayer: (v: boolean) => settings.change('hudShowLayer', v),
    setHudShowOverallProgress: (v: boolean) => settings.change('hudShowOverallProgress', v),
    setHudShowProgress: (v: boolean) => settings.change('hudShowProgress', v),
    setHudShowProjectedBlockName: (v: boolean) => settings.change('hudShowProjectedBlockName', v),
    setHudShowWrongState: (v: boolean) => settings.change('hudShowWrongState', v),
    setHudShowWrongType: (v: boolean) => settings.change('hudShowWrongType', v),
    setHudShowExtraBlocks: (v: boolean) => settings.change('hudShowExtraBlocks', v),
    setMaterialHudEnabled: (v: boolean) => settings.change('materialHudEnabled', v),
};
}
export type HudSettingsControls = ReturnType<typeof useHudSettings>;
