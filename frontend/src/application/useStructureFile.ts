import { useCallback, useEffect, useState } from 'react';
import type { StructureFileBridge, StructureFileState } from '../platform/bridge';

const initialState: StructureFileState = { path: '', browsing: false, error: 0 };

// One subscription for the menu lifetime; page navigation cannot lose a reply.
export function useStructureFile(bridge: StructureFileBridge) {
  const [file, setFile] = useState(initialState);
  useEffect(() => bridge.subscribeStructureFile(setFile), [bridge]);
  const browse = useCallback(() => {
    if (file.browsing) return;
    setFile(previous => ({ ...previous, browsing: true, error: 0 }));
    try { bridge.browseStructureFile(); }
    catch {
      setFile(previous => ({ ...previous, browsing: false, error: 1 }));
    }
  }, [bridge, file.browsing]);
  return { file, browse };
}
