import { useCallback, useEffect, useRef, useState } from 'react';
import type { CaptureAction, CaptureBridge, CaptureDraft, CapturePoint, CaptureState } from '../platform/bridge';

export type PointText = { x: string; y: string; z: string };
const blankPoint = (): PointText => ({ x: '', y: '', z: '' });
const pointText = (point: CapturePoint | null): PointText => point
  ? { x: String(point.x), y: String(point.y), z: String(point.z) } : blankPoint();

function parsePoint(point: PointText): CapturePoint | null | undefined {
  if (Object.values(point).every(value => value.trim() === '')) return null;
  if (!Object.values(point).every(value => /^-?\d+$/.test(value.trim())
    && Number.isInteger(Number(value)) && Number(value) >= -2147483648 && Number(value) <= 2147483647)) return undefined;
  return { x: Number(point.x), y: Number(point.y), z: Number(point.z) };
}

function sameDraft(left: CaptureDraft, right: CaptureDraft) {
  const pointEqual = (a: CapturePoint | null, b: CapturePoint | null) => a === b
    || (!!a && !!b && a.x === b.x && a.y === b.y && a.z === b.z);
  return pointEqual(left.first, right.first) && pointEqual(left.second, right.second)
    && left.includeEntities === right.includeEntities;
}

export function useCapture(bridge: CaptureBridge) {
  const [state, setState] = useState<CaptureState | null>(null);
  const [first, updateFirst] = useState(blankPoint);
  const [second, updateSecond] = useState(blankPoint);
  const [includeEntities, updateIncludeEntities] = useState(false);
  const [pendingAction, setPendingAction] = useState<CaptureAction | null>(null);
  const lastRevision = useRef<number | null>(null);
  const nextId = useRef(0);
  const pending = useRef<{id: number; action: CaptureAction; draft: CaptureDraft} | null>(null);
  const editVersion = useRef(0);
  const failedVersion = useRef<number | null>(null);
  const setFirst = useCallback((point: PointText) => { ++editVersion.current; updateFirst(point); }, []);
  const setSecond = useCallback((point: PointText) => { ++editVersion.current; updateSecond(point); }, []);
  const setIncludeEntities = useCallback((value: boolean) => { ++editVersion.current; updateIncludeEntities(value); }, []);
  useEffect(() => bridge.subscribeCapture(next => {
    const request = pending.current;
    const acknowledged = request !== null && next.ack >= request.id;
    const ownEdit = acknowledged && request.action === 'apply' && !next.error
      && next.worldAvailable && sameDraft(next.draft, request.draft);
    setState(next);
    nextId.current = Math.max(nextId.current, next.ack);
    if (lastRevision.current !== next.revision) {
      lastRevision.current = next.revision;
      // An acknowledgement of an older automatic edit must not overwrite text
      // entered while that request was in flight. External resets still sync.
      if (!ownEdit) {
        updateFirst(pointText(next.draft.first));
        updateSecond(pointText(next.draft.second));
        updateIncludeEntities(next.draft.includeEntities);
      }
    }
    if (acknowledged) { pending.current = null; setPendingAction(null); }
  }), [bridge]);
  const firstPoint = parsePoint(first), secondPoint = parsePoint(second);
  const valid = firstPoint !== undefined && secondPoint !== undefined;
  const draft: CaptureDraft | null = valid
    ? { first: firstPoint!, second: secondPoint!, includeEntities } : null;
  // Like the old menu, incomplete text keeps that point's last native value;
  // valid edits to the other point and the entity flag can still synchronize.
  const syncDraft: CaptureDraft = {
    first: firstPoint || state?.draft.first || null,
    second: secondPoint || state?.draft.second || null,
    includeEntities,
  };
  const send = useCallback((action: CaptureAction, value: CaptureDraft) => {
    if (!state?.worldAvailable || state.busy || pending.current) return;
    const id = ++nextId.current;
    pending.current = { id, action, draft: value };
    setPendingAction(action);
    try { bridge.capture(action, value, state.revision, id); }
    catch {
      failedVersion.current = editVersion.current;
      pending.current = null; setPendingAction(null);
    }
  }, [bridge, state]);
  useEffect(() => {
    if (state?.worldAvailable && !state.busy && !pending.current
      && failedVersion.current !== editVersion.current && !sameDraft(syncDraft, state.draft)) send('apply', syncDraft);
  }, [syncDraft, state, send]);
  const submit = useCallback((action: CaptureAction) => {
    if (action === 'export' && (!draft?.first || !draft.second)) return;
    if (state) send(action, action === 'clear' ? state.draft : (action === 'export' ? draft! : syncDraft));
  }, [draft, syncDraft, state, send]);
  const busy = !!state?.busy || pendingAction !== null;
  // Keep typing enabled during automatic synchronization; only explicit game
  // actions and save dialogs lock the editor. New edits coalesce after ack.
  const editingBusy = busy && pendingAction !== 'apply';
  return { state, first, second, setFirst, setSecond, includeEntities, setIncludeEntities,
    valid, draft, busy, editingBusy, submit };
}
export type CaptureControls = ReturnType<typeof useCapture>;
