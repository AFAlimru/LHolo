import { useCallback, useEffect, useState } from 'react';
import type { ProjectionBridge, ProjectionState } from '../platform/bridge';

type Action = 'load' | 'close' | 'restore';
const initialState: ProjectionState = {
  status: '', loading: false, hasProjection: false, canRestore: false, savedOrigin: '',
};

export function useProjection(bridge: ProjectionBridge) {
  const [projection, setProjection] = useState(initialState);
  const [ready, setReady] = useState(false);
  const [pending, setPending] = useState<Action | null>(null);
  useEffect(() => bridge.subscribeProjection(next => {
    setProjection(next);
    setReady(true);
    setPending(null);
  }), [bridge]);
  const submit = useCallback((action: Action) => {
    if (!ready || pending === 'close') return;
    if (action !== 'close' && (pending || projection.loading)) return;
    if (action === 'restore' && !projection.canRestore) return;
    setPending(action);
    try {
      if (action === 'load') bridge.loadProjection();
      else if (action === 'close') bridge.closeProjection();
      else bridge.restoreProjection();
    } catch { setPending(null); }
  }, [bridge, pending, projection.loading, projection.canRestore, ready]);
  return { projection, ready, busy: projection.loading || pending !== null,
    closing: pending === 'close',
    load: () => submit('load'), close: () => submit('close'), restore: () => submit('restore') };
}
export type ProjectionControls = ReturnType<typeof useProjection>;
