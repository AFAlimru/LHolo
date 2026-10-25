import React from 'react';
import { createRoot } from '@react-facet/dom-fiber';
import labels from '../../src/i18n/lang/zh_CN.json';
import { createPreviewEngine } from './engine';
import { installPreviewRuntime } from './runtime';
import '../src/styles/menu.css';
import './preview.css';

const notice = (text: string) => { document.getElementById('preview-status')!.textContent = text; };
installPreviewRuntime();
const { _meta, ...menuLabels } = labels;
const preview = createPreviewEngine(menuLabels, notice);
window.engine = preview.engine;
document.querySelectorAll<HTMLButtonElement>('[data-scenario]').forEach(button => {
  button.addEventListener('click', () => preview.scenario(button.dataset.scenario!));
});
document.getElementById('preview-scale')!.addEventListener('change', event => {
  document.documentElement.style.fontSize = (event.target as HTMLSelectElement).value + 'px';
});
new EventSource('/esbuild').addEventListener('change', () => location.reload());

// App imports the runtime boundary during module evaluation. Install it first.
void import('../src/App').then(({ App }) => {
  createRoot(document.getElementById('root')!).render(<App />);
});
