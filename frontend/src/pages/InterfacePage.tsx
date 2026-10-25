import React from 'react';
import {
  type InterfaceSettingsControls,
} from '../application/useInterfaceSettings';
import type { MenuLabels } from '../platform/bridge';
import { LHoloSelect } from '../components/LHoloSelect';

export function InterfacePage({ labels, interfaceSettings }: { labels: MenuLabels; interfaceSettings: InterfaceSettingsControls }) {
  const languageLabel = labels['label.language'];
  return <>
    <div className="lholo-section-title">{labels['section.interfaceSettings']}</div>
    <div className="lholo-interface-row lholo-setting-row">
      <div className="lholo-interface-label">{languageLabel}</div>
      <LHoloSelect label={languageLabel} value={interfaceSettings.language}
        disabled={!interfaceSettings.ready} options={interfaceSettings.languages.map(item => item.name)} onChange={interfaceSettings.setLanguage} />
    </div>

  </>;
}
