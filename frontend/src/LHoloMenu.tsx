import React, { useState } from 'react';
import { MenuNavigation, type MenuPage } from './components/MenuNavigation';
import { TransformPage } from './pages/TransformPage';
import type { TransformControls } from './application/useTransform';
import { RenderPage } from './pages/RenderPage';
import type { RenderSettingsControls } from './application/useRenderSettings';
import { HudPage } from './pages/HudPage';
import type { HudSettingsControls } from './application/useHudSettings';
import { HotkeysPage } from './pages/HotkeysPage';
import type { HotkeySettingsControls } from './application/useHotkeySettings';
import { InterfacePage } from './pages/InterfacePage';
import type { InterfaceSettingsControls } from './application/useInterfaceSettings';
import { ExperimentalPage } from './pages/ExperimentalPage';
import type { ExperimentalSettingsControls } from './application/useExperimentalSettings';
import { MaterialListModal } from './pages/MaterialListModal';
import type { MaterialsControls } from './application/useMaterials';
import { CreateStructurePage } from './pages/CreateStructurePage';
import type { CaptureControls } from './application/useCapture';
import { ProjectionPage } from './pages/ProjectionPage';
import type { ProjectionControls } from './application/useProjection';
import type { MenuLabels, StructureFileState } from './platform/bridge';
import { NativeScrollView } from './platform/native';
import { LHoloButton } from './components/LHoloControls';

export function LHoloMenu({
  labels,
  file,
  projection,
  capture,
  transform,
  render,
  hud,
  hotkeys,
  interfaceSettings,
  experimental,
  materialsState,
  onBrowse,
  onClose,
}: {
  labels: MenuLabels;
  file: StructureFileState;
  projection: ProjectionControls;
  capture: CaptureControls;
  transform: TransformControls;
  render: RenderSettingsControls;
  hud: HudSettingsControls;
  hotkeys: HotkeySettingsControls;
  interfaceSettings: InterfaceSettingsControls;
  experimental: ExperimentalSettingsControls;
  materialsState: MaterialsControls;
  onBrowse(): void;
  onClose(): void;
}) {
  const [page, setPage] = useState<MenuPage>('projection');
  const selectPage = (next: MenuPage) => {
    materialsState.closeModal();
    experimental.closeModal();
    if (page === 'hotkeys' && next !== 'hotkeys') hotkeys.cancelCapture();
    setPage(next);
  };
  return <div className="lholo-backdrop">
    <div className="lholo-menu">
      <div className="lholo-menu-header">
        <div className="lholo-brand">LHolo</div>
        <LHoloButton label={labels['menu.close']} onClick={onClose} />
      </div>
      <div className="lholo-menu-body">
        <div className="lholo-navigation"><NativeScrollView>
          <MenuNavigation page={page} labels={labels} onSelect={selectPage} />
        </NativeScrollView></div>
        <div className="lholo-content-viewport"><NativeScrollView>
          <div className="lholo-content" key={page}>
            {page === 'projection'
              ? <ProjectionPage labels={labels} file={file} projection={projection} onBrowse={onBrowse} onMaterials={materialsState.openModal} />
              : page === 'createStructure' ? <CreateStructurePage labels={labels} capture={capture} fileBrowsing={file.browsing} />
              : page === 'transform' ? <TransformPage labels={labels} transform={transform} />
              : page === 'render' ? <RenderPage labels={labels} render={render} />
              : page === 'hud' ? <HudPage labels={labels} hud={hud} />
              : page === 'hotkeys' ? <HotkeysPage labels={labels} hotkeys={hotkeys} />
              : page === 'interface' ? <InterfacePage labels={labels} interfaceSettings={interfaceSettings} />
              : <ExperimentalPage labels={labels} experimental={experimental} />}
          </div>
        </NativeScrollView></div>
      </div>
    </div>
    <MaterialListModal
      labels={labels}
      hasLoadedStructure={projection.projection.hasProjection}
      materialsState={materialsState}
    />
  </div>;
}
