import {useEffect, useState} from 'react';
import type {SettingsControls} from './useSettings';
export type AssistedPlacementMode = 'manual' | 'easy' | 'range';
export type ExperimentalConsentTarget = 'view' | AssistedPlacementMode | null;
const modes: AssistedPlacementMode[] = ['manual', 'easy', 'range'];
export function useExperimentalSettings(settings: SettingsControls) {
  const [pendingConsentFeature, setModal] = useState<ExperimentalConsentTarget>(null);
  const [text, setText] = useState<Partial<Record<'placementRadius' | 'autoPlacementCooldown', string>>>({});
  useEffect(() => { setModal(null); setText({}); }, [settings.state?.context]);
  useEffect(() => {
    if (settings.state?.consent && !settings.pending) setModal(null);
    if (settings.state && !settings.state.accepted && !settings.pending) setText({});
  }, [settings.state, settings.pending]);
  const activeNumber = settings.state?.values.activeMode ?? 0;
  const experimentalConsent = settings.state?.consent ?? false;
  const activeMode = activeNumber === 0 ? null : modes[activeNumber - 1];
  const toggleMode = (mode: AssistedPlacementMode, checked: boolean) => {
    if (!settings.canCommand) return;
    if (checked && !experimentalConsent) { setModal(mode); return; }
    settings.change('activeMode', checked ? modes.indexOf(mode) + 1 : 0);
  };
  const confirmConsent = () => {
    if (pendingConsentFeature && settings.canCommand)
      settings.command('consent', undefined, pendingConsentFeature === 'view' ? 0 : modes.indexOf(pendingConsentFeature) + 1);
  };
  const maximum = (field: string) => field === 'placementRadius' ? 4 : 60;
  const minimum = (field: string) => field === 'placementRadius' ? 1 : 0;
  const edit = (field: 'placementRadius' | 'autoPlacementCooldown', value: string) => {
    setText(previous => ({...previous, [field]: value}));
    const n = /^-?\d+$/.test(value.trim()) ? Number(value) : NaN;
    if (Number.isFinite(n)) settings.change(field, Math.max(minimum(field), Math.min(maximum(field), n)));
  };
  const commit = (field: 'placementRadius' | 'autoPlacementCooldown') => setText(previous => {
    const next = {...previous}; delete next[field]; return next;
  });
  const step = (field: 'placementRadius' | 'autoPlacementCooldown', delta: number) => {
    commit(field);
    settings.change(field, Math.max(minimum(field), Math.min(maximum(field), settings.values[field] + delta)));
  };
  return {ready: settings.ready, canCommand: settings.canCommand, experimentalConsent, activeMode, pendingConsentFeature,
    openInfoModal: () => setModal('view'), closeModal: () => setModal(null), confirmConsent, toggleMode,
    placementRadius: {text: text.placementRadius ?? String(settings.values.placementRadius)},
    autoPlacementCooldown: {text: text.autoPlacementCooldown ?? String(settings.values.autoPlacementCooldown)},
    editPlacementRadius: (v: string) => edit('placementRadius', v), commitPlacementRadius: () => commit('placementRadius'),
    stepPlacementRadius: (d: number) => step('placementRadius', d),
    editAutoPlacementCooldown: (v: string) => edit('autoPlacementCooldown', v), commitAutoPlacementCooldown: () => commit('autoPlacementCooldown'),
    stepAutoPlacementCooldown: (d: number) => step('autoPlacementCooldown', d)};
}
export type ExperimentalSettingsControls = ReturnType<typeof useExperimentalSettings>;
