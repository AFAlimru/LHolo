import React from 'react';
import type { HotkeySettingsControls } from '../application/useHotkeySettings';
import type { MenuLabels } from '../platform/bridge';
import { LHoloButton, LHoloCheckbox } from '../components/LHoloControls';

export function HotkeysPage({ labels, hotkeys }: { labels: MenuLabels; hotkeys: HotkeySettingsControls }) {
  return <>
    <div className="lholo-section-title">{labels['section.hotkeys']}</div>
    <div className="lholo-hotkey-list">
      {hotkeys.bindings.map((definition, index) => {
        const actionLabel = labels[definition.labelKey];
        const capturing = hotkeys.capturingIndex === index;
        const bindingText = capturing
          ? labels['hint.pressKeys']
          : definition.display;
        return <div className="lholo-hotkey-row" key={definition.slot}>
          <div className="lholo-hotkey-label">{actionLabel}</div>
          <div className="lholo-hotkey-actions">
            <LHoloButton disabled={!hotkeys.canCommand} label={bindingText}
              className={'lholo-hotkey-bind' + (capturing ? ' is-capturing' : '')}
              onClick={() => { if (!capturing) hotkeys.beginCapture(index); }} />
            <LHoloButton disabled={!hotkeys.canCommand} label={labels['button.resetHotkey']}
              onClick={() => hotkeys.resetHotkey(index)} />
            <LHoloButton disabled={!hotkeys.canCommand} label={labels['button.clearHotkey']}
              onClick={() => hotkeys.clearHotkey(index)} />
          </div>
        </div>;
      })}
    </div>
    <div className="lholo-render-check">
      <LHoloCheckbox disabled={!hotkeys.ready} label={labels['checkbox.altWheelOffset']}
        value={hotkeys.altWheelOffsetEnabled} onChange={hotkeys.setAltWheelOffsetEnabled} />
    </div>
    <div className="lholo-action-row">
      <LHoloButton disabled={!hotkeys.canCommand} label={labels['button.resetAllHotkeys']} onClick={hotkeys.resetAllHotkeys} />
    </div>
    <div className="lholo-muted">{labels['hint.chatCommand']}</div>

  </>;
}
