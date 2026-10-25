import type { CaptureDraft } from '../platform/bridge';

// Cohtml's JavaScript runtime must not be assumed to support BigInt. Each
// int32-coordinate span is exact as a Number; decimal multiplication keeps
// the volume exact as well, even for spans whose product exceeds 2^53.
function multiplyDecimal(value: string, factor: number): string {
  let carry = 0;
  let result = '';
  for (let index = value.length - 1; index >= 0; --index) {
    const product = Number(value[index]) * factor + carry;
    result = String(product % 10) + result;
    carry = Math.floor(product / 10);
  }
  return (carry ? String(carry) : '') + result;
}

export function selectionMetrics(draft: CaptureDraft | null) {
  if (!draft?.first || !draft.second) return null;
  const dimensions = (['x', 'y', 'z'] as const).map(axis =>
    Math.abs(draft.second![axis] - draft.first![axis]) + 1);
  return { dimensions, volume: dimensions.reduce(multiplyDecimal, '1') };
}
