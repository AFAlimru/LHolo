import React from 'react';
import type { MenuLabels } from '../platform/bridge';
import { LHoloButton } from './LHoloControls';

export const menuPages = ['projection', 'createStructure', 'transform', 'render',
  'hud', 'hotkeys', 'interface', 'experimental'] as const;
export type MenuPage = typeof menuPages[number];

export function MenuNavigation({ page, labels, onSelect }: {
  page: MenuPage; labels: MenuLabels; onSelect(page: MenuPage): void;
}) {
  return <>{menuPages.map(item => <LHoloButton key={item}
    label={labels['page.' + item] ?? item}
    className={'lholo-navigation-item' + (item === page ? ' is-selected' : '')}
    onClick={() => onSelect(item)} />)}</>;
}
