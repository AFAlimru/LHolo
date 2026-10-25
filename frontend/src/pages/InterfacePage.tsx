import React from 'react';
import {
  type InterfaceSettingsControls,
} from '../application/useInterfaceSettings';
import type { MenuLabels } from '../platform/bridge';
import { LHoloButton, LHoloTextField } from '../components/LHoloControls';
import { LHoloSelect } from '../components/LHoloSelect';

export function InterfacePage({ labels, interfaceSettings }: { labels: MenuLabels; interfaceSettings: InterfaceSettingsControls }) {
  const scaleLabel = labels['label.uiScale'];
  const languageLabel = labels['label.language'];
  return <>
    <div className="lholo-section-title">{labels['section.interfaceSettings']}</div>
    <div className="lholo-interface-row lholo-setting-row">
      <div className="lholo-interface-label">{scaleLabel}</div>
      <div className="lholo-stepper">
        <LHoloButton label="-" className="lholo-stepper-button"
          onClick={() => interfaceSettings.stepUiScale(-1)} />
        <div className="lholo-stepper-value"
          style={{ width: Math.max(3.6, Math.min(8, interfaceSettings.uiScale.text.length * 0.7 + 1.4)) + 'rem' }}>
          <LHoloTextField label={scaleLabel} value={interfaceSettings.uiScale.text} maxLength={6}
            onChange={interfaceSettings.editUiScale} onBlur={interfaceSettings.commitUiScale} />
        </div>
        <LHoloButton label="+" className="lholo-stepper-button"
          onClick={() => interfaceSettings.stepUiScale(1)} />
      </div>
    </div>
    <div className="lholo-muted">{labels['oreui.scalePlaceholder']}</div>
    <div className="lholo-interface-row lholo-setting-row">
      <div className="lholo-interface-label">{languageLabel}</div>
      <LHoloSelect label={languageLabel} value={interfaceSettings.language}
        disabled={!interfaceSettings.ready} options={interfaceSettings.languages.map(item => item.name)} onChange={interfaceSettings.setLanguage} />
    </div>

  </>;
}
