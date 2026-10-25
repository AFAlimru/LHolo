import React from 'react';
import type { OpacityField, RenderSettingsControls } from '../application/useRenderSettings';
import type { MenuLabels } from '../platform/bridge';
import { LHoloButton, LHoloCheckbox, LHoloTextField } from '../components/LHoloControls';
import { LHoloSelect } from '../components/LHoloSelect';

export function RenderPage({ labels, render }: { labels: MenuLabels; render: RenderSettingsControls }) {
  const isMaterial = render.layerAxis === 2;
  const axisOptions = [
    labels['combo.layerAxisY'],
    labels['combo.layerAxisX'],
    labels['combo.layerAxisMaterial'],
  ];
  const rangeOptions = isMaterial
    ? [
        labels['combo.materialAll'],
        labels['combo.materialSingle'],
        labels['combo.materialUpToCurrent'],
        labels['combo.materialFromCurrent'],
      ]
    : [
        labels['combo.rangeAll'],
        labels['combo.rangeSingle'],
        labels['combo.rangeUpToCurrent'],
        labels['combo.rangeFromCurrent'],
      ];
  const currentLayerLabel = labels[isMaterial ? 'label.currentMaterial' : 'label.currentLayer'];
  const layerHint = isMaterial
    ? labels['hint.materialOrder'].replace('%d', String(render.maxLayer + 1))
    : labels['hint.layerZeroBased']
        .replace('%d', String(render.maxLayer))
        .replace('%s', render.layerAxis === 1 ? 'X' : 'Y');

  return <>
    <div className="lholo-section-title">{labels['section.projectionStyle']}</div>
    <div className="lholo-render-check">
      <LHoloCheckbox disabled={!render.ready} label={labels['checkbox.renderBounds']}
        value={render.structureBoundsEnabled} onChange={render.setStructureBoundsEnabled} />
    </div>

    <div className="lholo-section-title lholo-render-section">{labels['section.layerSettings']}</div>
    <div className="lholo-transform-row">
      <div className="lholo-transform-label">{labels['label.layerAxis']}</div>
      <LHoloSelect disabled={!render.ready} label={labels['label.layerAxis']} value={render.layerAxis}
        options={axisOptions} onChange={render.setLayerAxis} />
    </div>
    <div className="lholo-transform-row">
      <div className="lholo-transform-label">{labels['label.displayRange']}</div>
      <LHoloSelect disabled={!render.ready} label={labels['label.displayRange']} value={render.layerDisplayMode}
        options={rangeOptions} onChange={render.setLayerDisplayMode} />
    </div>
    <div className="lholo-transform-row">
      <div className="lholo-transform-label">{currentLayerLabel}</div>
      <div className="lholo-stepper">
        <LHoloButton disabled={!render.ready} label="-" className="lholo-stepper-button" onClick={() => render.stepDisplayLayer(-1)} />
        <div className="lholo-stepper-value" style={{ width: Math.max(3.2, Math.min(10, render.layerText.length * 0.7 + 1.2)) + 'rem' }}>
          <LHoloTextField key={currentLayerLabel} disabled={!render.ready} label={currentLayerLabel} value={render.layerText} maxLength={11}
            onChange={render.editDisplayLayer} onBlur={render.commitDisplayLayer} />
        </div>
        <LHoloButton disabled={!render.ready} label="+" className="lholo-stepper-button" onClick={() => render.stepDisplayLayer(1)} />
      </div>
      <div className="lholo-muted lholo-layer-hint">{layerHint}</div>
    </div>

    <div className="lholo-section-title lholo-render-section">{labels['section.correctionStyle']}</div>
    {([
      ['fill', 'label.correctionFill'],
      ['outline', 'label.correctionOutline'],
    ] as const satisfies readonly [OpacityField, string][]).map(([field, labelKey]) => {
      const label = labels[labelKey];
      const item = render.opacity[field];
      return <div className="lholo-opacity-row" key={field}>
        <div className="lholo-opacity-field">
          <LHoloTextField disabled={!render.ready} label={label} value={item.text} maxLength={4}
            onChange={text => render.editOpacity(field, text)} onBlur={() => render.commitOpacity(field)} />
        </div>
        <div className="lholo-opacity-label">{label}</div>
      </div>;
    })}
    <div className="lholo-action-row">
      <LHoloButton disabled={!render.ready} label={labels['button.resetCorrectionStyle']} onClick={render.resetCorrectionStyle} />
    </div>

    <div className="lholo-section-title lholo-render-section">{labels['section.seeThrough']}</div>
    <div className="lholo-render-check">
      <LHoloCheckbox disabled={!render.ready} label={labels['checkbox.correctionSeeThrough']}
        value={render.correctionSeeThrough} onChange={render.setCorrectionSeeThrough} />
    </div>
    <div className="lholo-render-check">
      <LHoloCheckbox disabled={!render.ready} label={labels['checkbox.missingSeeThrough']}
        value={render.missingSeeThrough} onChange={render.setMissingSeeThrough} />
    </div>

    <div className="lholo-muted lholo-vibrant-hint">{labels['hint.vibrantVisuals']}</div>

  </>;
}
