import {useEffect, useState} from 'react';
import type {ProjectionBridge, ProjectionState} from '../platform/bridge';

export function useProjectionState(bridge: ProjectionBridge) {
  const [state, setState] = useState<ProjectionState>({worldAvailable:false, loading:false, hasProjection:false});
  useEffect(() => bridge.subscribeProjection(setState), [bridge]);
  return state;
}
