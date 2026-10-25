// A value equal to the current native snapshot is still an edit when an older
// write to that field is in flight. Dropping it would lose a user's revert.
export function pruneCleanFields<T extends object>(draft: Partial<T>, native: T, inFlight: readonly (keyof T)[]): Partial<T> {
  const next = {...draft};
  for (const field of Object.keys(next) as (keyof T)[])
    if (next[field] === native[field] && !inFlight.includes(field)) delete next[field];
  return next;
}
