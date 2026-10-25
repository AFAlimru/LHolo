import type { CaptureState, ProjectionState, StructureFileState, MenuLabels, TransformState, TransformEdit, RenderState, RenderEdit } from '../src/platform/bridge';
import { createPreviewSettings } from './settings';
import type {SettingsIntent} from '../src/platform/settingsBridge';

// Development-only native-boundary simulation. Never included by src/index.tsx.
export function createPreviewEngine(labels: MenuLabels, notice: (message: string) => void) {
  const listeners = new Map<string, Set<(payload: string) => void>>();
  let generation = 0;
  let projectionRequest = 0;
  let transform: TransformState = { values: { rotation: 0, mirror: 0, x: 0, y: 0, z: 0 }, context: 0, ack: 0, accepted: false };
  let render: RenderState = {values: {bounds: true, axis: 0, mode: 0, layer: 0, fill: 15, outline: 100,
    correctionSeeThrough: false, missingSeeThrough: false}, maximum: [0, 0, 0], context: 0, ack: 0, accepted: false};
  let file: StructureFileState = { path: '', browsing: false, error: 0 };
  let projection: ProjectionState = {
    status: labels['status.notLoaded'], loading: false, hasProjection: false, canRestore: false, savedOrigin: '',
  };
  let capture: CaptureState = {
    draft: { first: null, second: null, includeEntities: false }, revision: 0, ack: 0,
    worldAvailable: true, busy: false, status: labels['captureStatus.needTwoPoints'], error: '',
  };
  const emit = (name: string, value: unknown) => {
    const payload = JSON.stringify(value);
    listeners.get(name)?.forEach(listener => listener(payload));
  };
  const publish = () => {
    emit('lholo:structure-file', file);
    emit('lholo:projection', projection);
    emit('lholo:capture', capture);
    emit('lholo:transform', transform);
    emit('lholo:render', render);
    settings.publish();
  };
  const settings = createPreviewSettings(emit, publish);
  const later = (callback: () => void) => {
    const current = generation;
    setTimeout(() => { if (generation === current) { callback(); publish(); } }, 350);
  };
  const browse = () => {
    if (file.browsing) return;
    file = { ...file, browsing: true, error: 0 }; publish();
    const picker = document.createElement('input');
    picker.type = 'file'; picker.accept = '.mcstructure,.litematic';
    const current = generation;
    let finished = false;
    const finish = (selected?: File) => {
      if (finished) return;
      if (generation !== current) { picker.remove(); return; }
      finished = true;
      if (selected && !/\.(mcstructure|litematic)$/i.test(selected.name)) file = { ...file, error: 1 };
      else if (selected) file = { ...file, path: '预览目录\\' + selected.name, error: 0 };
      file = { ...file, browsing: false }; publish(); picker.remove();
    };
    picker.addEventListener('change', () => finish(picker.files?.[0]));
    picker.addEventListener('cancel', () => finish());
    picker.style.display = 'none'; document.body.appendChild(picker); picker.click();
    notice('浏览器只提供文件名；预览不会读取或解析结构文件。');
  };
  const engine: typeof window.engine = {
    on(name, callback) {
      if (!listeners.has(name)) listeners.set(name, new Set());
      listeners.get(name)!.add(callback);
    },
    off(name, callback) { listeners.get(name)?.delete(callback); },
    trigger(name, ...args) {
      if (name === 'facet:discard') return;
      if (name === 'facet:request' && args[0] === 'lholo.settings.intent') {
        settings.request(JSON.parse((args[2] as {request: string}).request) as SettingsIntent); return;
      }
      if (name === 'facet:request' && args[0] === 'lholo.render.intent') {
        const edit: RenderEdit = JSON.parse((args[2] as {request: string}).request);
        const layers = edit.changes.some(c => ['axis', 'mode', 'layer'].includes(c.field));
        const accepted = edit.context === render.context && (!layers || edit.axis === render.values.axis)
          && edit.changes.every(c => render.values[c.field] === c.expected);
        const values = {...render.values};
        if (accepted) {
          for (const c of edit.changes) Object.assign(values, {[c.field]: c.value});
          if (layers) values.layer = Math.min(values.layer, render.maximum[values.axis]);
        }
        render = {...render, values, ack: edit.id, accepted};
        queueMicrotask(publish); return;
      }
      if (name === 'facet:request' && args[0] === 'lholo.transform.intent') {
        const edit: TransformEdit = JSON.parse((args[2] as { request: string }).request);
        const accepted = edit.context === transform.context && transform.values[edit.field] === edit.expected;
        transform = { ...transform, ack: edit.id, accepted,
          values: accepted ? { ...transform.values, [edit.field]: edit.value } : transform.values };
        queueMicrotask(publish); return;
      }
      if (name === 'facet:request' && args[0] === 'lholo.capture.intent') {
        const request = JSON.parse((args[2] as { request: string }).request);
        if (capture.busy) return;
        if (!capture.worldAvailable || request.revision !== capture.revision) {
          capture = { ...capture, ack: request.id, error: labels['oreui.captureChanged'] }; publish(); return;
        }
        capture = { ...capture, draft: request.draft, ack: request.id, error: '' };
        if (request.action === 'clear') capture.draft = { first: null, second: null, includeEntities: false };
        else if (request.action === 'first') capture.draft.first = { x: -79, y: -60, z: 43 };
        else if (request.action === 'second') capture.draft.second = { x: -80, y: -60, z: 29 };
        capture = { ...capture, revision: capture.revision + 1,
          status: labels[capture.draft.first && capture.draft.second
            ? 'captureStatus.selectionReady' : 'captureStatus.needTwoPoints'] };
        if (request.action === 'export') {
          capture = { ...capture, busy: true }; publish();
          later(() => { capture = { ...capture, busy: false, status: '开发预览：已模拟导出，未生成结构文件' }; });
        } else publish();
        return;
      }
      const command = name.replace('query:subscribe/', '');
      switch (command) {
      case 'lholo.hotkeys.cancel': settings.cancel(); break;
      case 'lholo.menu.ready':
        // Match native asynchronous delivery after all effects subscribe.
        queueMicrotask(() => { emit('lholo:menu-labels', labels); publish(); }); break;
      case 'lholo.menu.close': notice('这是浏览器开发预览。关闭此标签页即可退出。'); break;
      case 'lholo.structure.browse': browse(); break;
      case 'lholo.projection.load':
      case 'lholo.projection.restore': {
        settings.contextChanged();
        if (projection.loading) return;
        projection = { ...projection, loading: true, status: '开发预览：模拟加载中…' }; publish();
        const request = ++projectionRequest;
        later(() => { if (request !== projectionRequest) return;
          render = {...render, context: render.context + 1, maximum: [11, 15, 4], values: {...render.values, axis: 0, mode: 0, layer: 0}};
          projection = { ...projection, loading: false, hasProjection: true, canRestore: true,
          savedOrigin: '原点 X -79  Y -60  Z 43', status: '开发预览：已模拟加载结构' }; }); break;
      }
      case 'lholo.projection.close':
        settings.contextChanged();
        ++projectionRequest;
        projection = { ...projection, loading: false, hasProjection: false, status: labels['status.projectionClosed'] };
        publish(); break;
      }
    },
  };
  return {
    engine,
    scenario(name: string) {
      ++generation;
      settings.contextChanged();
      render = {...render, context: render.context + 1};
      transform = { ...transform, context: transform.context + 1, values: { rotation: 0, mirror: 0, x: 0, y: 0, z: 0 } };
      file = { ...file, browsing: false };
      capture = { ...capture, busy: false, revision: capture.revision + 1, error: '' };
      projection = { ...projection, loading: false };
      if (name === 'loaded') {
        render = {...render, maximum: [11, 15, 4]};
        file = { path: '预览目录\\示例结构.mcstructure', browsing: false, error: 0 };
        projection = {
          status: '已加载: 示例结构.mcstructure (16 × 12 × 16)',
          loading: false,
          hasProjection: true,
          canRestore: true,
          savedOrigin: '原点 X -79  Y -60  Z 43',
        };
      } else if (name === 'selection') {
        capture = { ...capture, worldAvailable: true, status: labels['captureStatus.selectionReady'],
          draft: { first: { x: -79, y: -60, z: 43 }, second: { x: -80, y: -60, z: 29 }, includeEntities: false } };
      } else if (name === 'no-world') {
        render = {...render, maximum: [0, 0, 0], values: {...render.values, layer: 0}};
        capture = { ...capture, worldAvailable: false, status: labels['captureStatus.noWorld'],
          draft: { first: null, second: null, includeEntities: false } };
      } else if (name === 'saved') {
        file = { path: '预览目录\\示例结构.mcstructure', browsing: false, error: 0 };
        projection = { ...projection, canRestore: true, savedOrigin: '原点 X -79  Y -60  Z 43' };
      } else if (name === 'reset') {
        render = {...render, maximum: [0, 0, 0], values: {...render.values, axis: 0, mode: 0, layer: 0}};
        file = { path: '', browsing: false, error: 0 };
        projection = { status: labels['status.notLoaded'], loading: false, hasProjection: false, canRestore: false, savedOrigin: '' };
        capture = { ...capture, worldAvailable: true, status: labels['captureStatus.needTwoPoints'],
          draft: { first: null, second: null, includeEntities: false } };
      }
      publish(); notice('开发预览：已切换模拟状态。');
    },
  };
}
