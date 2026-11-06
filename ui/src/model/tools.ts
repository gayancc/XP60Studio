// Sound-design tools: init, randomize, mutate, morph, compare and copy/paste. They work on plain lists of changes, so they
// are the same for every kind of document and are tested without the instrument or the codec's internals.

import type { BlockInfo, BlockName, Catalog, DocKindName, ParamInfo } from './catalog';
import { blockInfo, isNameCharacter } from './catalog';
import { blockNames, blockLayout } from './kinds';
import { bipolarOrigin } from './controlKind';
import { efxDefault } from './efx';

export interface Change {
  block: BlockName;
  id: string;
  value: number;
}

/** A small seeded generator (xorshift32), so a randomised sound can be reproduced from its seed. */
export class Rng {
  private s: number;

  constructor(seed: number) {
    // Never start at zero, which would stay zero.
    this.s = (Math.trunc(seed) >>> 0) || 0x9e3779b9;
  }

  /** A float in [0, 1). */
  next(): number {
    let x = this.s;
    x ^= x << 13;
    x >>>= 0;
    x ^= x >>> 17;
    x ^= x << 5;
    x >>>= 0;
    this.s = x;
    return x / 0x1_0000_0000;
  }

  /** An integer in [lo, hi], both included. */
  int(lo: number, hi: number): number {
    return lo + Math.floor(this.next() * (hi - lo + 1));
  }
}

/** Names and routing, and the ranges and switches that decide whether something sounds at all. No tool changes these. */
export function isRoutingProtected(p: ParamInfo): boolean {
  if (isNameCharacter(p)) return true;
  return /(^|_)output(_|$)|^efx_output|assign$|^clock_source$|^midi_channel$|^receive_|^transmit_|^local_switch$|^(keyboard_range_|velocity_range_|tone_switch$)/.test(
    p.id,
  );
}

/**
 * What Randomize and Mutate leave alone: the above, plus the mix levels, so a roll never silences a Tone, Part or patch.
 * (Morph blends levels on purpose and uses only the routing set.)
 */
export function isProtected(p: ParamInfo): boolean {
  return isRoutingProtected(p) || /^(tone|part|patch)_level$/.test(p.id);
}

/** Which blocks of a document the tool applies to. */
export type Scope = 'all' | BlockName;

function targets(catalog: Catalog, kind: DocKindName, scope: Scope): Array<{ block: BlockName; info: BlockInfo }> {
  const blocks = scope === 'all' ? blockNames(kind) : [scope];
  return blocks.map((block) => ({ block, info: blockInfo(catalog, kind, block) }));
}

const clamp = (v: number, lo: number, hi: number): number => Math.min(hi, Math.max(lo, v));

/**
 * Replace each unprotected parameter, with probability `strength` (0 to 1), by a value drawn uniformly from its whole
 * range or option list. Returns only the parameters whose value actually changes.
 */
export function randomize(
  catalog: Catalog,
  kind: DocKindName,
  scope: Scope,
  get: (block: BlockName, id: string) => number,
  strength: number,
  seed: number,
  isProtectedParam: (p: ParamInfo) => boolean = isProtected,
): Change[] {
  const rng = new Rng(seed);
  const out: Change[] = [];
  for (const { block, info } of targets(catalog, kind, scope)) {
    for (const p of info.params) {
      if (isProtectedParam(p)) continue;
      // Draw both numbers for every parameter so a seed gives the same sound however the strength is set.
      const roll = rng.next();
      const value = rng.int(p.min, p.max);
      if (roll < strength && value !== get(block, p.id)) out.push({ block, id: p.id, value });
    }
  }
  return out;
}

/** Nudge each unprotected parameter by up to `amount` (0 to 1) of its range, up or down. */
export function mutate(
  catalog: Catalog,
  kind: DocKindName,
  scope: Scope,
  get: (block: BlockName, id: string) => number,
  amount: number,
  seed: number,
  isProtectedParam: (p: ParamInfo) => boolean = isProtected,
): Change[] {
  const rng = new Rng(seed);
  const out: Change[] = [];
  for (const { block, info } of targets(catalog, kind, scope)) {
    for (const p of info.params) {
      if (isProtectedParam(p)) continue;
      const delta = Math.round((rng.next() * 2 - 1) * amount * (p.max - p.min));
      const cur = get(block, p.id);
      const value = clamp(cur + delta, p.min, p.max);
      if (value !== cur) out.push({ block, id: p.id, value });
    }
  }
  return out;
}

/**
 * Blend two snapshots of the same kind of document: at `t` = 0 the result is A, at 1 it is B. Numbers are interpolated;
 * choices (option lists) switch at the halfway point. Protected parameters and names stay as they are in `current`.
 */
export function morph(
  catalog: Catalog,
  kind: DocKindName,
  a: Uint8Array,
  b: Uint8Array,
  t: number,
  get: (block: BlockName, id: string) => number,
  isProtectedParam: (p: ParamInfo) => boolean = isRoutingProtected,
): Change[] {
  const k = clamp(t, 0, 1);
  const out: Change[] = [];
  for (const slot of blockLayout(catalog, kind)) {
    for (const p of slot.info.params) {
      if (isProtectedParam(p)) continue;
      const va = readParam(a, slot.start, p);
      const vb = readParam(b, slot.start, p);
      const value =
        p.display.kind === 'options' ? (k < 0.5 ? va : vb) : clamp(Math.round(va + (vb - va) * k), p.min, p.max);
      if (value !== get(slot.block, p.id)) out.push({ block: slot.block, id: p.id, value });
    }
  }
  return out;
}

/** Read a parameter's raw value straight from a snapshot (7-bit, ASCII or nibble encodings). */
export function readParam(snapshot: Uint8Array, blockStart: number, p: ParamInfo): number {
  if (p.bytes === 1) return snapshot[blockStart + p.offset] ?? 0;
  // Multi-byte parameters are stored as low nibbles, most significant first.
  let v = 0;
  for (let i = 0; i < p.bytes; i++) v = (v << 4) | ((snapshot[blockStart + p.offset + i] ?? 0) & 0x0f);
  return v;
}

export interface DiffEntry {
  block: BlockName;
  id: string;
  name: string;
  a: number;
  b: number;
}

/** Every parameter that differs between two snapshots of the same kind of document. */
export function diff(catalog: Catalog, kind: DocKindName, a: Uint8Array, b: Uint8Array): DiffEntry[] {
  const out: DiffEntry[] = [];
  for (const slot of blockLayout(catalog, kind)) {
    for (const p of slot.info.params) {
      const va = readParam(a, slot.start, p);
      const vb = readParam(b, slot.start, p);
      if (va !== vb) out.push({ block: slot.block, id: p.id, name: p.name, a: va, b: vb });
    }
  }
  return out;
}

// ---- Init ----

/**
 * A neutral starting sound for `kind`: every parameter at its minimum, except that signed and pan parameters sit at
 * their zero or centre, level and cutoff parameters and the upper ends of key and velocity ranges are fully open, and the first Tone, Part or Key is on. These are the
 * application's own defaults, not the factory ones (ledger H-052).
 */
export function initChanges(catalog: Catalog, kind: DocKindName): Change[] {
  const out: Change[] = [];
  const first = blockNames(kind)[1];
  for (const block of blockNames(kind)) {
    const info = blockInfo(catalog, kind, block);
    for (const p of info.params) {
      if (isNameCharacter(p)) continue;
      const value = initValue(p, block === first);
      if (value !== p.min) out.push({ block, id: p.id, value });
    }
  }
  return out;
}

/** Every value of one block (or only `ids` of it) back at its neutral setting, written out in full so nothing is skipped. */
export function resetChanges(catalog: Catalog, kind: DocKindName, block: BlockName, ids?: readonly string[]): Change[] {
  const first = blockNames(kind)[1];
  return blockInfo(catalog, kind, block)
    .params.filter((p) => !isNameCharacter(p) && (!ids || ids.includes(p.id)))
    .map((p) => ({ block, id: p.id, value: initValue(p, block === first) }));
}

function initValue(p: ParamInfo, isFirstItem: boolean): number {
  // An initialised sound has EFX type 1 (STEREO-EQ), so its twelve effect parameters start at that type's own settings, not at the bottom of their ranges.
  const efxSlot = /^efx_parameter_(\d+)$/.exec(p.id);
  if (efxSlot) return efxDefault(0, Number(efxSlot[1]));
  if (/^(tone_switch|receive_switch)$/.test(p.id)) return isFirstItem ? 1 : 0;
  // The wave group an initialised Tone points at is the instrument's own INT-A (group ID 1); ID 0 names no group the keyboard has.
  if (p.id === 'wave_group_id') return 1;
  // A range that starts empty would play nothing: the upper ends open to the top of the keyboard and of the velocities.
  if (/_range_upper$/.test(p.id)) return p.max;
  const origin = bipolarOrigin(p);
  if (origin !== null) return Math.round(p.min + origin * (p.max - p.min));
  if (/(^|_)level$|^cutoff_frequency$|level_\d$|^(tone|part|patch)_level$/.test(p.id) && !/send|bias|lfo|mix_efx/.test(p.id)) {
    return p.max;
  }
  return p.min;
}

// ---- Copy and paste ----

export interface Clip {
  kind: DocKindName;
  /** What was copied, for the menu ("Tone 2", "Tone 2 TVF"). */
  label: string;
  values: Array<{ id: string; value: number }>;
}

/** Copy `ids` of one block, or the whole block when `ids` is omitted. */
export function copyBlock(
  catalog: Catalog,
  kind: DocKindName,
  block: BlockName,
  get: (block: BlockName, id: string) => number,
  label: string,
  ids?: readonly string[],
): Clip {
  const info = blockInfo(catalog, kind, block);
  const wanted = new Set(ids ?? info.params.map((p) => p.id));
  return {
    kind,
    label,
    values: info.params.filter((p) => wanted.has(p.id) && !isNameCharacter(p)).map((p) => ({ id: p.id, value: get(block, p.id) })),
  };
}

/** The changes needed to paste a clip into `block`. Only parameters that exist there and differ are returned. */
export function pasteClip(
  catalog: Catalog,
  kind: DocKindName,
  clip: Clip,
  block: BlockName,
  get: (block: BlockName, id: string) => number,
): Change[] {
  if (clip.kind !== kind) return [];
  const info = blockInfo(catalog, kind, block);
  const known = new Map(info.params.map((p) => [p.id, p]));
  const out: Change[] = [];
  for (const { id, value } of clip.values) {
    const p = known.get(id);
    if (!p || value < p.min || value > p.max) continue;
    if (value !== get(block, id)) out.push({ block, id, value });
  }
  return out;
}
