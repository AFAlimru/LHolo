import { useEffect, useState } from 'react';
import { pruneCleanFields } from './fieldDraft';
import type { RenderBridge, RenderEdit, RenderField, RenderState, RenderValues } from '../platform/bridge';

export type OpacityField = 'fill' | 'outline';
type NumericField = OpacityField | 'layer';
const fields: RenderField[] = ['bounds', 'axis', 'mode', 'layer', 'fill', 'outline', 'correctionSeeThrough', 'missingSeeThrough'];
const initial: RenderValues = { bounds: true, axis: 0, mode: 0, layer: 0, fill: 15, outline: 100,
  correctionSeeThrough: false, missingSeeThrough: false };
let nextId = 0;
const clamp = (n: number, max: number) => Math.max(0, Math.min(max, n));
interface Editor {
  native: RenderState | null;
  dirty: Partial<RenderValues>;
  text: Partial<Record<NumericField, string>>;
  pending: RenderEdit | null;
  failed: boolean;
}
const valuesOf = (e: Editor): RenderValues => ({ ...(e.native?.values || initial), ...e.dirty });
const cleanDraft = (e: Editor, dirty: Partial<RenderValues>) =>
  pruneCleanFields(dirty, e.native?.values || initial, e.pending?.changes.map(c => c.field) || []);

// Owns editing/acknowledgement only. World bounds, settings and defaults after
// reopening come from the native application snapshot, never from this module.
export function useRenderSettings(bridge: RenderBridge) {
  const [editor, set] = useState<Editor>({native: null, dirty: {}, text: {}, pending: null, failed: false});
  useEffect(() => bridge.subscribeRender(native => {
    nextId = Math.max(nextId, native.ack);
    set(previous => {
      if (!previous.native || native.context !== previous.native.context)
        return {native, dirty: {}, text: {}, pending: null, failed: false};
      let dirty = {...previous.dirty}, text = {...previous.text}, pending = previous.pending;
      const acknowledged = pending && native.ack >= pending.id;
      if (acknowledged && pending) {
        if (!native.accepted) return {native, dirty: {}, text: {}, pending: null, failed: false};
        for (const change of pending.changes)
          if (dirty[change.field] === change.value) delete dirty[change.field];
        pending = null;
      } else if (native.values.axis !== previous.native.values.axis) {
        // A layer typed for Y cannot silently become an X/material index.
        delete dirty.axis; delete dirty.layer; delete dirty.mode; delete text.layer;
      }
      dirty = pruneCleanFields(dirty, native.values, pending?.changes.map(c => c.field) || []);
      return {...previous, native, dirty, text, pending};
    });
  }), [bridge]);
  useEffect(() => {
    if (!editor.native || editor.pending || editor.failed) return;
    const changes: RenderEdit['changes'] = [];
    for (const field of fields) {
      const value = editor.dirty[field];
      if (value !== undefined && value !== editor.native.values[field])
        changes.push({field, expected: editor.native.values[field], value});
    }
    if (!changes.length) return;
    const edit: RenderEdit = {id: ++nextId, context: editor.native.context, axis: editor.native.values.axis, changes};
    set(previous => ({...previous, pending: edit}));
    try { bridge.renderSettings(edit); }
    catch (error) {
      set(previous => ({...previous, pending: null, failed: true}));
      console.error(error);
    }
  }, [bridge, editor]);
  const change = <F extends RenderField>(field: F, value: RenderValues[F]) => set(previous => previous.native
    ? {...previous, dirty: cleanDraft(previous, {...previous.dirty, [field]: value}), failed: false} : previous);
  const setLayerAxis = (axis: number) => set(previous => {
    if (!previous.native) return previous;
    axis = clamp(axis, 2);
    const text = {...previous.text}; delete text.layer;
    return {...previous, failed: false, text, dirty: cleanDraft(previous, {...previous.dirty, axis,
      layer: clamp(valuesOf(previous).layer, previous.native.maximum[axis])})};
  });
  const editNumber = (field: NumericField, text: string) => set(previous => {
    if (!previous.native) return previous;
    const values = valuesOf(previous);
    const n = /^-?\d+$/.test(text.trim()) ? Number(text) : NaN;
    const material = field === 'layer' && values.axis === 2;
    const maximum = field === 'layer' ? previous.native.maximum[values.axis] : 100;
    const value = clamp(n - (material ? 1 : 0), maximum);
    return {...previous, failed: false, text: {...previous.text, [field]: text},
      dirty: Number.isFinite(n) ? cleanDraft(previous, {...previous.dirty, [field]: value}) : previous.dirty};
  });
  const commitNumber = (field: NumericField) => set(previous => {
    const text = {...previous.text}; delete text[field];
    return {...previous, text};
  });
  const stepDisplayLayer = (delta: number) => set(previous => {
    if (!previous.native) return previous;
    const values = valuesOf(previous), text = {...previous.text}; delete text.layer;
    return {...previous, failed: false, text, dirty: cleanDraft(previous, {...previous.dirty,
      layer: clamp(values.layer + delta, previous.native.maximum[values.axis])})};
  });
  const resetCorrectionStyle = () => set(previous => {
    if (!previous.native) return previous;
    const text = {...previous.text}; delete text.fill; delete text.outline;
    return {...previous, failed: false, text, dirty: cleanDraft(previous, {...previous.dirty, fill: 15, outline: 100})};
  });
  const values = valuesOf(editor);
  const maxLayer = editor.native?.maximum[values.axis] ?? 0;
  return {
    ready: !!editor.native, structureBoundsEnabled: values.bounds, setStructureBoundsEnabled: (v: boolean) => change('bounds', v),
    layerAxis: values.axis, setLayerAxis, layerDisplayMode: values.mode, setLayerDisplayMode: (v: number) => change('mode', clamp(v, 3)),
    displayLayer: values.layer, maxLayer, layerText: editor.text.layer ?? String(values.layer + (values.axis === 2 ? 1 : 0)),
    editDisplayLayer: (t: string) => editNumber('layer', t), commitDisplayLayer: () => commitNumber('layer'), stepDisplayLayer,
    opacity: {fill: {text: editor.text.fill ?? String(values.fill)}, outline: {text: editor.text.outline ?? String(values.outline)}},
    editOpacity: (f: OpacityField, t: string) => editNumber(f, t), commitOpacity: (f: OpacityField) => commitNumber(f), resetCorrectionStyle,
    correctionSeeThrough: values.correctionSeeThrough, setCorrectionSeeThrough: (v: boolean) => change('correctionSeeThrough', v),
    missingSeeThrough: values.missingSeeThrough, setMissingSeeThrough: (v: boolean) => change('missingSeeThrough', v),
  };
}
export type RenderSettingsControls = ReturnType<typeof useRenderSettings>;
