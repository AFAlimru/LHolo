import React from 'react';
import type { OffsetAxis, TransformControls } from '../application/useTransform';
import type { MenuLabels } from '../platform/bridge';
import { LHoloButton, LHoloTextField } from '../components/LHoloControls';
import { LHoloSelect } from '../components/LHoloSelect';

export function TransformPage({ labels, transform }: { labels: MenuLabels; transform: TransformControls }) {
  return <>
    <div className="lholo-section-title">{labels['section.transform']}</div>
    <div className="lholo-transform-row">
      <div className="lholo-transform-label">{labels['label.rotation']}</div>
      <LHoloSelect disabled={!transform.ready} label={labels['label.rotation']} value={transform.rotation}
        options={['0°', '90°', '180°', '270°']} onChange={transform.setRotation} />
    </div>
    <div className="lholo-transform-row">
      <div className="lholo-transform-label">{labels['label.mirror']}</div>
      <LHoloSelect disabled={!transform.ready} label={labels['label.mirror']} value={transform.mirror}
        options={[labels['combo.mirrorNone'], 'X', 'Z']} onChange={transform.setMirror} />
    </div>
    <div className="lholo-transform-divider" />
    {(['x', 'y', 'z'] as OffsetAxis[]).map(axis => {
      const label = labels['label.offset' + axis.toUpperCase()];
      const offset = transform.offsets[axis];
      return <div className="lholo-transform-row" key={axis}>
        <div className="lholo-transform-label">{label}</div>
        <div className="lholo-stepper">
          <LHoloButton disabled={!transform.ready} label="-" className="lholo-stepper-button" onClick={() => transform.stepOffset(axis, -1)} />
          <div className="lholo-stepper-value" style={{width: Math.max(3.2, Math.min(10, offset.text.length * 0.7 + 1.2)) + 'rem'}}>
            <LHoloTextField disabled={!transform.ready} label={label} value={offset.text} maxLength={11}
              onChange={text => transform.editOffset(axis, text)} onBlur={() => transform.commitOffset(axis)} />
          </div>
          <LHoloButton disabled={!transform.ready} label="+" className="lholo-stepper-button" onClick={() => transform.stepOffset(axis, 1)} />
        </div>
      </div>;
    })}
  </>;
}
