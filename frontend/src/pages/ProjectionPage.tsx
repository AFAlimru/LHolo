import React from 'react';
import type { ProjectionControls } from '../application/useProjection';
import type { MenuLabels, StructureFileState } from '../platform/bridge';
import { LHoloButton } from '../components/LHoloControls';

export function ProjectionPage({ labels, file, projection: controls, onBrowse, onMaterials }: {
  labels: MenuLabels; file: StructureFileState; projection: ProjectionControls; onBrowse(): void; onMaterials(): void;
}) {
  const { projection, ready, busy } = controls;
  return <>
    <div className="lholo-section-title">{labels['section.projectionFile']}</div>
    <div className="lholo-status">{projection.status || labels['status.notLoaded']}</div>
    <div className="lholo-file-row">
      <div className="lholo-file-field" title={file.path}>{file.path}</div>
      <LHoloButton label={file.browsing ? labels['oreui.browsing'] : labels['button.browse']}
        disabled={file.browsing || busy} onClick={onBrowse} />
    </div>
    <div className="lholo-muted">{labels['label.structurePath']}</div>
    {file.error ? <div className="lholo-notice">{labels['oreui.browseFailed']}</div> : null}
    <div className="lholo-action-row">
      <LHoloButton label={labels['button.load']} disabled={!ready || busy || file.browsing || !file.path} onClick={controls.load} />
      <LHoloButton label={labels['button.closeProjection']} disabled={!ready || controls.closing || (!projection.hasProjection && !busy)} onClick={controls.close} />
      <LHoloButton label={labels['material.listTitle']} onClick={onMaterials} />
    </div>
    <div className="lholo-restore-row">
      <LHoloButton label={labels['button.restoreLastProjection']} disabled={!ready || busy || file.browsing || !projection.canRestore} onClick={controls.restore} />
    </div>
    <div className="lholo-muted">{projection.canRestore ? projection.savedOrigin : labels['hint.noSavedProjection']}</div>
  </>;
}
