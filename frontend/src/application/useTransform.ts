import { useEffect, useState } from 'react';
import { pruneCleanFields } from './fieldDraft';
import type { TransformBridge, TransformEdit, TransformField, TransformState, TransformValues } from '../platform/bridge';

export type OffsetAxis = 'x' | 'y' | 'z';
const fields: TransformField[] = ['rotation', 'mirror', 'x', 'y', 'z'];
const zero: TransformValues = { rotation: 0, mirror: 0, x: 0, y: 0, z: 0 };
let nextId = 0;
interface Editor {
  native: TransformState | null;
  dirty: Partial<TransformValues>;
  text: Partial<Record<OffsetAxis, string>>;
  pending: TransformEdit | null;
  failed: boolean;
}
const cleanDraft = (e: Editor, dirty: Partial<TransformValues>) =>
  pruneCleanFields(dirty, e.native?.values || zero, e.pending ? [e.pending.field] : []);

// Native values own business state; only incomplete text and unacknowledged edits live here.
export function useTransform(bridge: TransformBridge) {
  const [editor, set] = useState<Editor>({ native: null, dirty: {}, text: {}, pending: null, failed: false });
  useEffect(() => bridge.subscribeTransform(native => {
    nextId = Math.max(nextId, native.ack);
    set(previous => {
    if (!previous.native || native.context !== previous.native.context)
      return { native, dirty: {}, text: {}, pending: null, failed: false };
    let { dirty, text, pending } = previous;
    if (pending && native.ack >= pending.id) {
      if (!native.accepted) return { native, dirty: {}, text: {}, pending: null, failed: false };
      if (dirty[pending.field] === pending.value) {
        dirty = { ...dirty }; delete dirty[pending.field];
      }
      pending = null;
    }
    dirty = pruneCleanFields(dirty, native.values, pending ? [pending.field] : []);
    return { ...previous, native, dirty, text, pending };
    });
  }), [bridge]);
  useEffect(() => {
    if (!editor.native || editor.pending || editor.failed) return;
    const field = fields.find(f => editor.dirty[f] !== undefined && editor.dirty[f] !== editor.native!.values[f]);
    if (!field) return;
    const edit: TransformEdit = { id: ++nextId, context: editor.native.context,
      field, expected: editor.native.values[field], value: editor.dirty[field]! };
    set(previous => ({ ...previous, pending: edit }));
    try { bridge.transform(edit); }
    catch (error) {
      set(previous => ({ ...previous, pending: null, failed: true }));
      console.error(error);
    }
  }, [bridge, editor]);
  const values = { ...(editor.native?.values || zero), ...editor.dirty };
  const change = (field: TransformField, value: number) => set(previous => previous.native ? {
    ...previous, failed: false, dirty: cleanDraft(previous, { ...previous.dirty, [field]: value }),
  } : previous);
  const editOffset = (axis: OffsetAxis, text: string) => set(previous => {
    if (!previous.native) return previous;
    const value = /^-?\d+$/.test(text.trim()) ? Number(text) : NaN;
    return { ...previous, failed: false, text: { ...previous.text, [axis]: text },
      dirty: Number.isFinite(value) ? cleanDraft(previous, { ...previous.dirty, [axis]: Math.max(-2147483648, Math.min(2147483647, value)) }) : previous.dirty };
  });
  const commitOffset = (axis: OffsetAxis) => set(previous => {
    const text = { ...previous.text }; delete text[axis];
    return { ...previous, text };
  });
  const stepOffset = (axis: OffsetAxis, delta: number) => set(previous => {
    if (!previous.native) return previous;
    const value = Math.max(-2147483648, Math.min(2147483647, (previous.dirty[axis] ?? previous.native.values[axis]) + delta));
    const text = { ...previous.text }; delete text[axis];
    return { ...previous, failed: false, text, dirty: cleanDraft(previous, { ...previous.dirty, [axis]: value }) };
  });
  return { rotation: values.rotation, mirror: values.mirror, ready: !!editor.native,
    offsets: { x: { text: editor.text.x ?? String(values.x) }, y: { text: editor.text.y ?? String(values.y) }, z: { text: editor.text.z ?? String(values.z) } },
    setRotation: (value: number) => change('rotation', value), setMirror: (value: number) => change('mirror', value),
    editOffset, commitOffset, stepOffset };
}
export type TransformControls = ReturnType<typeof useTransform>;
