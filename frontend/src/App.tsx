import React, { useEffect, useState } from 'react';
import { LHoloMenu } from './LHoloMenu';
import { useStructureFile } from './application/useStructureFile';
import { useTransform } from './application/useTransform';
import { useRenderSettings } from './application/useRenderSettings';
import { useSettings } from './application/useSettings';
import { useHudSettings } from './application/useHudSettings';
import { useHotkeySettings } from './application/useHotkeySettings';
import { useInterfaceSettings } from './application/useInterfaceSettings';
import { useExperimentalSettings } from './application/useExperimentalSettings';
import { useMaterialsDraft } from './application/useMaterialsDraft';
import { useCapture } from './application/useCapture';
import { useProjection } from './application/useProjection';
import { menuBridge, type MenuLabels } from './platform/bridge';
import { FrameCompletion, GameShell } from './platform/native';
import { LHoloButton } from './components/LHoloControls';

function MenuPage() {
  const [labels, setLabels] = useState<MenuLabels | null>(null);
  const { file, browse } = useStructureFile(menuBridge);
  const projection = useProjection(menuBridge);
  const capture = useCapture(menuBridge);
  const transform = useTransform(menuBridge);
  const render = useRenderSettings(menuBridge);
  const settings = useSettings(menuBridge);
  const hud = useHudSettings(settings);
  const hotkeys = useHotkeySettings(settings);
  const interfaceSettings = useInterfaceSettings(settings);
  const experimental = useExperimentalSettings(settings);
  const materialsDraft = useMaterialsDraft(menuBridge);
  useEffect(() => menuBridge.subscribe(setLabels), []);
  return <>
    {labels ? <LHoloMenu labels={labels} file={file} projection={projection} capture={capture}
      transform={transform} render={render} hud={hud} hotkeys={hotkeys}
      interfaceSettings={interfaceSettings} experimental={experimental} materialsDraft={materialsDraft}
      onBrowse={browse} onClose={menuBridge.close} />
      : <div className="lholo-loading"><div>LHolo</div>
        <LHoloButton label="×" onClick={menuBridge.close} /></div>}
    <FrameCompletion />
  </>;
}

export function App() {
  return <GameShell component={MenuPage} />;
}
