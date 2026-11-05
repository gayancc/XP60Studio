// Chooses the control for a parameter from its display kind and range. Pure, so the choice is tested.

import type { ParamInfo } from './catalog';

export type ControlChoice =
  | { type: 'toggle' }
  | { type: 'segmented' }
  | { type: 'dropdown' }
  | { type: 'stepper' }
  | { type: 'knob'; origin: number | null };

/** Normalised (0..1) position of the "zero" of a signed or pan parameter, or null for plain unipolar values. */
export function bipolarOrigin(p: ParamInfo): number | null {
  const span = p.max - p.min;
  if (span <= 0) return null;
  const d = p.display;
  if (d.kind === 'pan') return (d.centre - p.min) / span;
  if (d.kind === 'numeric' && d.first < 0 && d.step > 0) {
    const zeroRaw = p.min + -d.first / d.step;
    return Number.isInteger(zeroRaw) ? (zeroRaw - p.min) / span : null;
  }
  return null;
}

/** "-100", "+30", "0", "1200": a number with an optional sign and nothing else. */
export const isPlainNumber = (label: string): boolean => /^[+\-\u2212]?\d+(\.\d+)?$/.test(label.trim());

export function controlFor(p: ParamInfo): ControlChoice {
  const d = p.display;
  if (d.kind === 'options') {
    const n = d.labels.length;
    if (n === 2 && d.labels[0] === 'OFF' && d.labels[1] === 'ON') return { type: 'toggle' };
    // Short labels fit as a switch; long lists need a drop-down.
    if (n <= 5 && d.labels.every((l) => l.length <= 8)) return { type: 'segmented' };
    // A long list of plain numbers (key follow -100 to +100, velocity times) is stepped through with minus and plus, not picked from a menu.
    if (d.labels.every(isPlainNumber)) return { type: 'stepper' };
    return { type: 'dropdown' };
  }
  return { type: 'knob', origin: bipolarOrigin(p) };
}
