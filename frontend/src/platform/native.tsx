import type React from 'react';
import type * as Facet from '@react-facet/core';

export interface InteractiveProps {
  label: string;
  onClick(): void;
  className?: string;
  disabled?: boolean;
  children?: React.ReactNode;
}

// Only the version-specific adapter may resolve private game symbols.
export interface NativeRuntime {
  React: typeof React;
  Facet: typeof Facet;
  Shell: React.ComponentType<{ component: React.ComponentType }>;
  Interactive: React.ComponentType<InteractiveProps>;
  Scroll: React.ComponentType<React.PropsWithChildren>;
  Completion: React.ComponentType;
}

declare global {
  interface Window { __LHoloOreUiRuntime: NativeRuntime; }
}

export const GameShell = window.__LHoloOreUiRuntime.Shell;
export const NativeInteractive = window.__LHoloOreUiRuntime.Interactive;
export const NativeScrollView = window.__LHoloOreUiRuntime.Scroll;
export const FrameCompletion = window.__LHoloOreUiRuntime.Completion;
