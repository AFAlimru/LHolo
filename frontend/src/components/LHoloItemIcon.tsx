import React from 'react';
import { NativeItemIcon } from '../platform/native';

// The row supplies item identity only. Runtime rendering and private URL syntax
// stay in the platform adapter; materials without inventory items keep a slot.
export function LHoloItemIcon({ itemId }: { itemId?: string }) {
  const cleanId = itemId?.startsWith('minecraft:') ? itemId.slice(10) : itemId;
  return <div className="lholo-material-icon">
    {cleanId ? <NativeItemIcon itemId={cleanId} /> : <div className="lholo-material-icon-empty" />}
  </div>;
}
