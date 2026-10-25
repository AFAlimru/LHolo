import React from 'react';
import type { HudSettingsControls } from '../application/useHudSettings';
import type { MenuLabels } from '../platform/bridge';
import { LHoloCheckbox } from '../components/LHoloControls';
import { LHoloSelect } from '../components/LHoloSelect';

export function HudPage({ labels, hud }: { labels: MenuLabels; hud: HudSettingsControls }) {
  const positions = [
    labels['corner.topLeft'],
    labels['corner.bottomLeft'],
    labels['corner.topRight'],
    labels['corner.bottomRight'],
  ];
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
    <div className="lholo-render-check">
      <LHoloCheckbox disabled={!hud.ready} label={labels['checkbox.hudEnabled']}
        value={hud.hudEnabled} onChange={hud.setHudEnabled} />
    </div>
    <div className="lholo-transform-row lholo-setting-row">
      <div className="lholo-hud-label">{labels['label.hudPosition']}</div>
      <LHoloSelect label={labels['label.hudPosition']} value={hud.hudPosition}
        options={positions} onChange={hud.setHudPosition} disabled={!hud.ready || !hud.hudEnabled} />
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
    <div className="lholo-transform-row lholo-setting-row">
      <div className="lholo-hud-label">{labels['label.materialHudPosition']}</div>
      <LHoloSelect label={labels['label.materialHudPosition']} value={hud.materialHudPosition}
        options={positions} onChange={hud.setMaterialHudPosition} disabled={!hud.ready || !hud.materialHudEnabled} />
    </div>

  </>;
}
