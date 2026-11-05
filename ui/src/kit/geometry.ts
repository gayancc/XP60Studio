// Knob geometry: angles run clockwise from 12 o'clock, so the usual 7:30 to 4:30 sweep is -135 to +135 degrees.

export const SWEEP_START = -135;
export const SWEEP_END = 135;

export interface Point {
  x: number;
  y: number;
}

export function polar(cx: number, cy: number, r: number, deg: number): Point {
  const rad = (deg * Math.PI) / 180;
  return { x: cx + r * Math.sin(rad), y: cy - r * Math.cos(rad) };
}

const f = (n: number): string => Number(n.toFixed(2)).toString();

/** SVG arc path from `a0` to `a1` degrees (clockwise). Empty when the sweep is effectively zero. */
export function arcPath(cx: number, cy: number, r: number, a0: number, a1: number): string {
  const lo = Math.min(a0, a1);
  const hi = Math.max(a0, a1);
  if (hi - lo < 0.01) return '';
  const start = polar(cx, cy, r, lo);
  const end = polar(cx, cy, r, hi);
  const large = hi - lo > 180 ? 1 : 0;
  return `M ${f(start.x)} ${f(start.y)} A ${f(r)} ${f(r)} 0 ${large} 1 ${f(end.x)} ${f(end.y)}`;
}

export function normToAngle(norm: number): number {
  return SWEEP_START + Math.min(1, Math.max(0, norm)) * (SWEEP_END - SWEEP_START);
}

/**
 * The value arc runs from the origin to the current value. Unipolar knobs start at the left end; bipolar knobs start
 * at `origin` (a normalised position, usually 0.5).
 */
export function valueArc(norm: number, origin: number | null): { from: number; to: number } {
  const to = normToAngle(norm);
  const from = origin === null ? SWEEP_START : normToAngle(origin);
  return { from, to };
}
