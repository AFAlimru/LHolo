import { useCallback, useEffect, useState } from 'react';
import type { MaterialItem, MaterialsBridge, MaterialsState, MenuLabels } from '../platform/bridge';

export type MaterialItemDraft = MaterialItem;

const initialState: MaterialsState = { ready: false, items: [] };

export function formatMaterialDisplayName(item: MaterialItemDraft, labels: MenuLabels): string {
  if (item.nameKey && labels[item.nameKey]) {
    return labels[item.nameKey];
  }
  return item.displayName || item.typeName;
}

export function formatMaterialSummary(
  materials: readonly MaterialItemDraft[],
  labels: MenuLabels
): string {
  let total = 0;
  for (const item of materials) {
    total += item.count;
  }
  return labels['label.materialSummary']
    .replace('%llu', String(total))
    .replace('%zu', String(materials.length));
}

// Native material list modal state backed by StructureUiState / MaterialTracker.
export function useMaterialsDraft(bridge: MaterialsBridge) {
  const [isOpen, setIsOpen] = useState(false);
  const [state, setState] = useState<MaterialsState>(initialState);
  useEffect(() => bridge.subscribeMaterials(setState), [bridge]);

  const openModal = useCallback(() => {
    setIsOpen(true);
    try { bridge.requestMaterials(); } catch {}
  }, [bridge]);
  const closeModal = useCallback(() => setIsOpen(false), []);

  return {
    isOpen,
    ready: state.ready,
    openModal,
    closeModal,
    materials: state.items,
  };
}

export type MaterialsDraftControls = ReturnType<typeof useMaterialsDraft>;
