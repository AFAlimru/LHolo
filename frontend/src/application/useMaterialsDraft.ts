import { useState } from 'react';
import type { MenuLabels } from '../platform/bridge';

export interface MaterialItemDraft {
  displayName: string;
  nameKey?: string;
  typeName: string;
  count: number;
}

export const defaultPreviewMaterials: readonly MaterialItemDraft[] = [
  { displayName: '石头', typeName: 'minecraft:stone', count: 128 },
  { displayName: '橡木木板', typeName: 'minecraft:oak_planks', count: 96 },
  { displayName: '玻璃', typeName: 'minecraft:glass', count: 48 },
  { displayName: '圆石楼梯', typeName: 'minecraft:stone_stairs', count: 32 },
  { displayName: '橡木原木', typeName: 'minecraft:oak_log', count: 24 },
  { displayName: '', nameKey: 'material.water', typeName: 'minecraft:water', count: 16 },
  { displayName: '灯笼', typeName: 'minecraft:lantern', count: 8 },
];

export function formatMaterialDisplayName(item: MaterialItemDraft, labels: MenuLabels): string {
  if (item.nameKey && labels[item.nameKey]) {
    return labels[item.nameKey];
  }
  return item.displayName;
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

// Frontend-only material list modal state. Mirrors renderMaterialPopup in
// src/ui/MenuPages.cpp without adding native bridge commands yet.
export function useMaterialsDraft(initialMaterials: readonly MaterialItemDraft[] = defaultPreviewMaterials) {
  const [isOpen, setIsOpen] = useState(false);
  const [materials, setMaterials] = useState<readonly MaterialItemDraft[]>(initialMaterials);

  const openModal = () => setIsOpen(true);
  const closeModal = () => setIsOpen(false);

  return {
    isOpen,
    openModal,
    closeModal,
    materials,
    setMaterials,
  };
}

export type MaterialsDraftControls = ReturnType<typeof useMaterialsDraft>;
