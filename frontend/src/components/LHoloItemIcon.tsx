import React from 'react';
import { NativeItemIcon } from '../platform/native';

// The row supplies item identity only. Runtime rendering and private URL syntax
// stay in the platform adapter; unresolved display identities keep an empty slot.
export function LHoloItemIcon({ iconId }: { iconId?: string }) {
  return <div className="lholo-material-icon">
    {iconId ? <NativeItemIcon itemId={iconId} /> : <div className="lholo-material-icon-empty" />}
  </div>;
}
