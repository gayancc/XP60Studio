// Mapping hardware controllers to parameters (parity rows A6 and A7). Pure data and arithmetic; the service that listens
// to the instrument and applies the moves is in controls.svelte.ts.

import type { BlockName, DocKindName } from './catalog';

/** What kind of message a controller sends: a Control Change, or an NRPN or RPN parameter number. */
export type SourceKind = 'cc' | 'nrpn' | 'rpn';

/** One controller (a mod wheel, a slider, a pedal) driving one parameter. */
export interface Binding {
  kind: DocKindName;
  block: BlockName;
  id: string;
  /** MIDI channel 0..15 the controller was heard on, or null to follow it on any channel. */
  channel: number | null;
  source: SourceKind;
  /** Controller number 1..127 for a CC (0 and 32 are Bank Select and never bound), or the 14-bit parameter number. */
  controller: number;
}

export type BindingMap = readonly Binding[];

const sameTarget = (a: Binding, b: Pick<Binding, 'kind' | 'block' | 'id'>): boolean =>
  a.kind === b.kind && a.block === b.block && a.id === b.id;

/** Does a move of `controller` on `channel` belong to `b`? */
export function hears(b: Binding, channel: number, source: SourceKind, number: number): boolean {
  return b.source === source && b.controller === number && (b.channel === null || b.channel === channel);
}

/**
 * Add a binding. A parameter has at most one controller and a controller (on its channel) drives at most one parameter
 * of a kind of document, so the new binding replaces whatever it collides with and the result is never ambiguous.
 */
export function bind(map: BindingMap, b: Binding): Binding[] {
  if (b.source === 'cc' && (b.controller === 0 || b.controller === 32)) return [...map];
  const kept = map.filter(
    (x) => !sameTarget(x, b) && !(x.kind === b.kind && x.source === b.source && x.controller === b.controller && x.channel === b.channel),
  );
  return [...kept, b];
}

export function unbind(map: BindingMap, target: Pick<Binding, 'kind' | 'block' | 'id'>): Binding[] {
  return map.filter((x) => !sameTarget(x, target));
}

export function find(map: BindingMap, target: Pick<Binding, 'kind' | 'block' | 'id'>): Binding | undefined {
  return map.find((x) => sameTarget(x, target));
}

/** A controller value (0..`sourceMax`, 127 for a CC) spread over a parameter's range, rounded to a whole step. */
export function scaleValue(controllerValue: number, min: number, max: number, sourceMax = 127): number {
  const top = Math.max(1, sourceMax);
  const v = Math.min(top, Math.max(0, Math.round(controllerValue)));
  return Math.round(min + (v / top) * (max - min));
}

/** Short text for a binding: "CC 74", "NRPN 130" or "CC 74 ch 3". */
export function describe(b: Binding): string {
  const name = `${b.source.toUpperCase()} ${b.controller}`;
  return b.channel === null ? name : `${name} ch ${b.channel + 1}`;
}

const KINDS = new Set<string>(['patch', 'performance', 'rhythm', 'system']);
const SOURCES = new Set<string>(['cc', 'nrpn', 'rpn']);

/** Read stored bindings, keeping only the well-formed ones (the storage may be old, edited or damaged). */
export function parseBindings(text: string | null): Binding[] {
  if (!text) return [];
  let raw: unknown;
  try {
    raw = JSON.parse(text);
  } catch {
    return [];
  }
  if (!Array.isArray(raw)) return [];
  let out: Binding[] = [];
  for (const item of raw) {
    if (typeof item !== 'object' || item === null) continue;
    const r = item as Record<string, unknown>;
    // Maps saved before NRPN support have no source: they were plain controllers.
    const source = r.source === undefined ? 'cc' : r.source;
    const top = source === 'cc' ? 127 : 16383;
    const ok =
      typeof r.kind === 'string' &&
      KINDS.has(r.kind) &&
      typeof r.block === 'string' &&
      typeof r.id === 'string' &&
      typeof source === 'string' &&
      SOURCES.has(source) &&
      (r.channel === null || (Number.isInteger(r.channel) && (r.channel as number) >= 0 && (r.channel as number) <= 15)) &&
      Number.isInteger(r.controller) &&
      (r.controller as number) >= (source === 'cc' ? 1 : 0) &&
      (r.controller as number) <= top &&
      !(source === 'cc' && r.controller === 32);
    if (ok) out = bind(out, { ...(r as unknown as Binding), source: source as SourceKind });
  }
  return out;
}

export const serializeBindings = (map: BindingMap): string => JSON.stringify(map);
