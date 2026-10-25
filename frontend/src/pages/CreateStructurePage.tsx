import React from 'react';
import type { CaptureControls } from '../application/useCapture';
import { selectionMetrics } from '../application/selectionMetrics';
import type { MenuLabels } from '../platform/bridge';
import { LHoloButton, LHoloCheckbox, LHoloTextField } from '../components/LHoloControls';

export function CreateStructurePage({ labels, capture, fileBrowsing }: {
  labels: MenuLabels; capture: CaptureControls; fileBrowsing: boolean;
}) {
  const { state, first, second, setFirst, setSecond, includeEntities, setIncludeEntities, valid, draft, busy, editingBusy, submit } = capture;
  const disabled = !state?.worldAvailable || busy;
  const editorDisabled = !state?.worldAvailable || editingBusy;
  let size = '', volume = '';
  const metrics = selectionMetrics(draft);
  if (metrics) {
    let index = 0;
    size = labels['label.captureSize'].replace(/%llu/g, () => String(metrics.dimensions[index++]));
    volume = labels['label.captureVolume'].replace('%llu', metrics.volume);
  }
  return <>
    <div className="lholo-section-title">{labels['section.captureSource']}</div>
    <div className="lholo-mode-row"><div>{labels['label.captureMode']}</div>
      <div className="lholo-mode-value">{labels['combo.captureModeClient']}</div></div>
    <div className="lholo-muted">{labels['hint.captureClientOnly']}</div>
    <div className="lholo-section-title lholo-capture-selection">{labels['section.captureSelection']}</div>
    <div className="lholo-status">{state?.status || labels['captureStatus.noWorld']}</div>
    {([['first', first, setFirst, 'label.capturePoint1'], ['second', second, setSecond, 'label.capturePoint2']] as const)
      .map(([slot, point, setPoint, label]) => <div className="lholo-capture-point" key={slot}>
        <div className="lholo-capture-point-title">{labels[label]}</div>
        <div className="lholo-coordinate-row">
          <div className="lholo-coordinate-fields">{(['x', 'y', 'z'] as const).map(axis =>
            <div className="lholo-coordinate" key={axis}>
              <div className="lholo-coordinate-label">{axis.toUpperCase()}</div>
              <LHoloTextField label={labels[label] + ' ' + axis.toUpperCase()}
                value={point[axis]} onChange={text => setPoint({ ...point, [axis]: text })}
                disabled={editorDisabled} maxLength={11} />
            </div>)}</div>
          <LHoloButton label={labels['button.usePlayerPosition']}
            disabled={disabled} onClick={() => submit(slot)} />
        </div>
      </div>)}
    {!valid && <div className="lholo-notice">{labels['oreui.invalidCoordinates']}</div>}
    {size && <div className="lholo-muted">{size}</div>}
    {volume && <div className="lholo-muted">{volume}</div>}
    <div className="lholo-capture-entities"><LHoloCheckbox label={labels['checkbox.includeEntities']}
      value={includeEntities} onChange={setIncludeEntities} disabled={editorDisabled} /></div>
    <div className="lholo-action-row">
      <LHoloButton label={labels['button.clearSelection']} disabled={disabled} onClick={() => submit('clear')} />
      <LHoloButton label={labels['button.exportMcstructure']}
        disabled={disabled || fileBrowsing || !valid || !draft?.first || !draft.second} onClick={() => submit('export')} />
    </div>
    {editingBusy && <div className="lholo-notice">{labels['oreui.captureBusy']}</div>}
    {state?.error && <div className="lholo-notice">{state.error}</div>}
  </>;
}
