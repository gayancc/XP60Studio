// Pure value maths for every continuous control. No DOM, no framework: this is what the gesture tests pin down.

export interface Range {
  min: number;
  max: number;
  /** Smallest change. XP-60 values are integers, so the default is 1. */
  step?: number;
}

/** Pixels of vertical drag that sweep the whole range (our own default; FL's absolute figure is unknown). */
export const DRAG_PX_FULL = 160;
/** Ctrl or Shift while dragging: measured from FL as about 0.28 of a normal drag. */
export const FINE_RATIO = 0.28;
/** One wheel notch is equivalent to this many pixels of dragging (measured from FL). */
export const WHEEL_NOTCH_PX = 13.5;

const stepOf = (r: Range): number => (r.step && r.step > 0 ? r.step : 1);

export function clamp(v: number, lo: number, hi: number): number {
  // NaN would otherwise propagate through every control; treat it as the low end.
  if (!Number.isFinite(v)) return Number.isNaN(v) || v < 0 ? lo : hi;
  return Math.min(hi, Math.max(lo, v));
}

/** Snap to the step grid anchored at `min`, then clamp. */
export function snap(value: number, r: Range): number {
  const s = stepOf(r);
  const snapped = r.min + Math.round((value - r.min) / s) * s;
  // Drop float dust from fractional steps.
  return clamp(Number(snapped.toFixed(10)), r.min, r.max);
}

export function toNorm(value: number, r: Range): number {
  return r.max === r.min ? 0 : clamp((value - r.min) / (r.max - r.min), 0, 1);
}

export function fromNorm(norm: number, r: Range): number {
  return snap(r.min + clamp(norm, 0, 1) * (r.max - r.min), r);
}

/**
 * Value after dragging `dy` pixels from the value at the start of the drag. Up (negative dy in screen space) increases.
 * Computed from the drag start, not incrementally, so no rounding error accumulates and returning the pointer to the
 * start restores the original value.
 */
export function dragValue(
  startValue: number,
  dy: number,
  r: Range,
  fine = false,
  pxFull: number = DRAG_PX_FULL,
): number {
  const ratio = fine ? FINE_RATIO : 1;
  const norm = toNorm(startValue, r) + (-dy / (pxFull > 0 ? pxFull : DRAG_PX_FULL)) * ratio;
  return fromNorm(norm, r);
}

/**
 * Value after `notches` wheel notches (positive = up = increase). Each notch moves at least one step, so small integer
 * ranges stay usable; fine mode uses the fine ratio but still never less than one step.
 */
export function wheelValue(current: number, notches: number, r: Range, fine = false): number {
  if (notches === 0) return current;
  const sign = Math.sign(notches);
  const s = stepOf(r);
  const span = r.max - r.min;
  const perNotch = (WHEEL_NOTCH_PX / DRAG_PX_FULL) * span * (fine ? FINE_RATIO : 1);
  const amount = Math.max(s, Math.round(perNotch / s) * s) * Math.abs(notches);
  return snap(current + sign * amount, r);
}

export type KeyIntent = 'up' | 'down' | 'pageUp' | 'pageDown' | 'home' | 'end';

/** Keyboard operation; page keys move about a tenth of the range. */
export function keyValue(current: number, intent: KeyIntent, r: Range): number {
  const s = stepOf(r);
  const page = Math.max(s, Math.round(((r.max - r.min) / 10) / s) * s);
  switch (intent) {
    case 'up':
      return snap(current + s, r);
    case 'down':
      return snap(current - s, r);
    case 'pageUp':
      return snap(current + page, r);
    case 'pageDown':
      return snap(current - page, r);
    case 'home':
      return r.min;
    case 'end':
      return r.max;
  }
}

export function intentFromKey(key: string): KeyIntent | null {
  switch (key) {
    case 'ArrowUp':
    case 'ArrowRight':
      return 'up';
    case 'ArrowDown':
    case 'ArrowLeft':
      return 'down';
    case 'PageUp':
      return 'pageUp';
    case 'PageDown':
      return 'pageDown';
    case 'Home':
      return 'home';
    case 'End':
      return 'end';
    default:
      return null;
  }
}
