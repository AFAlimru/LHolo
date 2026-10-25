import React from 'react';
import {
  formatMaterialDisplayName,
  formatMaterialSummary,
  type MaterialsDraftControls,
} from '../application/useMaterialsDraft';
import type { MenuLabels } from '../platform/bridge';
import { NativeScrollView } from '../platform/native';
import { LHoloButton } from '../components/LHoloControls';

export function MaterialListModal({
  labels,
  hasLoadedStructure,
  materialsDraft,
}: {
  labels: MenuLabels;
  hasLoadedStructure: boolean;
  materialsDraft: MaterialsDraftControls;
}) {
  if (!materialsDraft.isOpen) return null;
  const { materials, closeModal } = materialsDraft;

  return <div className="lholo-modal-backdrop">
    <div className="lholo-material-modal">
      <div className="lholo-material-modal-header">
        <div className="lholo-modal-title">{labels['material.listTitle']}</div>
        <LHoloButton label={labels['button.close']} onClick={closeModal} />
      </div>
      <div className="lholo-transform-divider" />
      {!hasLoadedStructure ? (
        <div className="lholo-muted lholo-material-empty">{labels['status.notLoaded']}</div>
      ) : (
        <>
          <div className="lholo-material-summary">
            {formatMaterialSummary(materials, labels)}
          </div>
          <div className="lholo-transform-divider" />
          {materials.length === 0 ? (
            <div className="lholo-muted lholo-material-empty">
              {labels['hint.noPlaceableMaterials']}
            </div>
          ) : (
            <div className="lholo-material-table">
              <div className="lholo-material-table-header">
                <div className="lholo-material-col-item">{labels['column.item']}</div>
                <div className="lholo-material-col-id">{labels['column.identifier']}</div>
                <div className="lholo-material-col-total">{labels['column.total']}</div>
              </div>
              <div className="lholo-material-table-viewport">
                <NativeScrollView>
                  <div className="lholo-material-table-body">
                    {materials.map((item, index) => (
                      <div key={item.typeName + ':' + index} className="lholo-material-table-row">
                        <div className="lholo-material-col-item">
                          {formatMaterialDisplayName(item, labels)}
                        </div>
                        <div className="lholo-material-col-id">{item.typeName}</div>
                        <div className="lholo-material-col-total">{String(item.count)}</div>
                      </div>
                    ))}
                  </div>
                </NativeScrollView>
              </div>
            </div>
          )}
        </>
      )}
    </div>
  </div>;
}
