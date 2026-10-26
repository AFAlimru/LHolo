import React from 'react';
import type { HudSettingsControls } from '../application/useHudSettings';
import type { MenuLabels } from '../platform/bridge';
import { LHoloButton, LHoloCheckbox } from '../components/LHoloControls';

export function HudPage({ labels, hud, worldAvailable }: { labels: MenuLabels; hud: HudSettingsControls; worldAvailable: boolean }) {
  const hudItems = [
    ['checkbox.hudFileName', hud.hudShowFileName, hud.setHudShowFileName],
    ['checkbox.hudLayer', hud.hudShowLayer, hud.setHudShowLayer],
    ['checkbox.hudOverallProgress', hud.hudShowOverallProgress, hud.setHudShowOverallProgress],
    ['checkbox.hudProgress', hud.hudShowProgress, hud.setHudShowProgress],
    ['checkbox.hudProjectedBlockName', hud.hudShowProjectedBlockName, hud.setHudShowProjectedBlockName],
    ['checkbox.hudWrongState', hud.hudShowWrongState, hud.setHudShowWrongState],
    ['checkbox.hudWrongType', hud.hudShowWrongType, hud.setHudShowWrongType],
    ['checkbox.hudExtraBlocks', hud.hudShowExtraBlocks, hud.setHudShowExtraBlocks],
  ] as const;

  return <>
    <div className="lholo-section-title">{labels['section.hud']}</div>
    <div className="lholo-hud-layout-action">
      <LHoloButton label={labels['hud.layout.edit']} onClick={hud.editLayout} disabled={!hud.ready || !worldAvailable} />
    </div>
    <div className="lholo-render-check">
      <LHoloCheckbox disabled={!hud.ready} label={labels['checkbox.hudEnabled']}
        value={hud.hudEnabled} onChange={hud.setHudEnabled} />
    </div>
    {hudItems.map(([key, value, onChange]) => (
      <div className="lholo-render-check" key={key}>
        <LHoloCheckbox label={labels[key]} value={value}
          onChange={onChange} disabled={!hud.ready || !hud.hudEnabled} />
      </div>
    ))}
    <div className="lholo-transform-divider" />
    <div className="lholo-render-check">
      <LHoloCheckbox disabled={!hud.ready} label={labels['checkbox.materialHudEnabled']}
        value={hud.materialHudEnabled} onChange={hud.setMaterialHudEnabled} />
    </div>
  </>;
}
