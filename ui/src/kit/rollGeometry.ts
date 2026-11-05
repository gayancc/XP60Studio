// Piano-roll keyboard geometry, written from our own measurements of the FL piano roll (see
// D:\Codes\GenosStyleStudio\docs\piano-roll\06-keyboard-rendering-correction.md). Units are semitone rows counted
// upwards from MIDI note 0; the vertical roll turns that axis so higher notes are higher on screen.
//
//  - Natural faces tile the octave unevenly: C, E, F, B are 1.5 rows tall, D, G, A are 2 rows tall. Within an octave the
//    natural edges are [0, 1.5, 3.5, 5, 6.5, 8.5, 10.5, 12].
//  - Accidentals are exactly one row tall and centred on the natural boundary they sit across (1.5, 3.5, 6.5, 8.5, 10.5).
//  - Naturals are painted first as full-width faces, accidentals on top, about two thirds of the width, from the left.
//  - Hit testing tries the accidentals first, then the natural; the white tail right of a black key is the natural below.

/** Natural edges inside an octave, in rows. */
export const NATURAL_EDGES = [0, 1.5, 3.5, 5, 6.5, 8.5, 10.5, 12] as const;
const NATURAL_INDEX: Readonly<Record<number, number>> = { 0: 0, 2: 1, 4: 2, 5: 3, 7: 4, 9: 5, 11: 6 };
/** Centre row of each accidental (pitch class -> rows from the octave's C). */
const ACCIDENTAL_CENTRE: Readonly<Record<number, number>> = { 1: 1.5, 3: 3.5, 6: 6.5, 8: 8.5, 10: 10.5 };

/** Accidental test order of the hit test: sharps first, then the naturals. */
export const HIT_ORDER = [1, 3, 6, 8, 10, 0, 2, 4, 5, 7, 9, 11] as const;

export const NOTE_TOP = 127;

export const pitchClass = (note: number): number => ((note % 12) + 12) % 12;
export const isAccidental = (note: number): boolean => pitchClass(note) in ACCIDENTAL_CENTRE;

/** The vertical span [bottom, top] of a key in rows from note 0, before clipping to the visible window. */
export function keySpan(note: number): [number, number] {
  const octave = Math.floor(note / 12) * 12;
  const pc = pitchClass(note);
  const centre = ACCIDENTAL_CENTRE[pc];
  if (centre !== undefined) return [octave + centre - 0.5, octave + centre + 0.5];
  const i = NATURAL_INDEX[pc] as number;
  return [octave + (NATURAL_EDGES[i] as number), octave + (NATURAL_EDGES[i + 1] as number)];
}

export interface RollView {
  /** Lowest and highest note that are fully in view (inclusive). */
  first: number;
  last: number;
  /** Pixel height of the whole roll. */
  height: number;
}

/** Pixels per semitone row. */
export const rowHeight = (v: RollView): number => v.height / (v.last - v.first + 1);

/** Pixel y (from the top) of a position given in rows from note 0. */
export const yOfRow = (v: RollView, row: number): number => (v.last + 1 - row) * rowHeight(v);

export interface KeyRect {
  note: number;
  accidental: boolean;
  /** Pixels from the top of the roll. */
  y: number;
  h: number;
}

/** The face of a note clipped to the visible window, or null when none of it is visible. */
export function keyRect(v: RollView, note: number): KeyRect | null {
  let [lo, hi] = keySpan(note);
  // A natural's face may reach into a neighbouring row; the window clips it at its own edges, and MIDI ends at 0 and 128.
  lo = Math.max(lo, v.first, 0);
  hi = Math.min(hi, v.last + 1, NOTE_TOP + 1);
  if (hi <= lo) return null;
  const y = yOfRow(v, hi);
  return { note, accidental: isAccidental(note), y, h: yOfRow(v, lo) - y };
}

/** Every visible key, naturals first (paint order). */
export function visibleKeys(v: RollView): KeyRect[] {
  const naturals: KeyRect[] = [];
  const accidentals: KeyRect[] = [];
  // One note beyond each edge: a natural next to the window can still show a sliver of its face.
  for (let n = Math.max(0, v.first - 1); n <= Math.min(NOTE_TOP, v.last + 1); n++) {
    const r = keyRect(v, n);
    if (r) (r.accidental ? accidentals : naturals).push(r);
  }
  return [...naturals, ...accidentals];
}

/**
 * The note under a point. `x` is 0 at the left of the keyboard and `kbW` at its right edge; the black faces cover the
 * left `blackW` pixels. Clamped to the visible window.
 */
export function noteAtKeyboard(v: RollView, kbW: number, blackW: number, x: number, y: number): number {
  const row = v.last + 1 - y / rowHeight(v);
  const clamped = Math.min(v.last + 1 - 1e-6, Math.max(v.first, row));
  const octave = Math.floor(clamped / 12) * 12;
  const inOctave = clamped - octave;
  if (x >= 0 && x < Math.min(blackW, kbW)) {
    for (const pc of HIT_ORDER) {
      const c = ACCIDENTAL_CENTRE[pc];
      if (c !== undefined && inOctave >= c - 0.5 && inOctave < c + 0.5) return clampNote(v, octave + pc);
    }
  }
  for (const pc of HIT_ORDER) {
    const i = NATURAL_INDEX[pc];
    if (i !== undefined && inOctave >= (NATURAL_EDGES[i] as number) && inOctave < (NATURAL_EDGES[i + 1] as number)) {
      return clampNote(v, octave + pc);
    }
  }
  return clampNote(v, Math.floor(clamped));
}

const clampNote = (v: RollView, n: number): number => Math.min(v.last, Math.max(v.first, Math.min(NOTE_TOP, Math.max(0, n))));

/** The note whose grid row contains y (the grid has uniform rows, unlike the keyboard). */
export function noteAtRow(v: RollView, y: number): number {
  return clampNote(v, Math.floor(v.last + 1 - y / rowHeight(v)));
}

/** Black face width for a keyboard `kbW` pixels wide: two thirds of the width less a small allowance. */
export const blackWidth = (kbW: number): number => Math.max(4, Math.round(((kbW - 4) * 2) / 3));

/**
 * The note at a position given along the pitch axis (`row`, in rows from note 0) and across the keyboard (`cross`, pixels
 * from the side the black keys start on). Accidentals are tried first, within `blackLen` pixels; otherwise the natural.
 */
export function noteAtPosition(first: number, last: number, row: number, cross: number, blackLen: number): number {
  const clamped = Math.min(last + 1 - 1e-6, Math.max(first, row));
  const octave = Math.floor(clamped / 12) * 12;
  const inOctave = clamped - octave;
  const limit = (n: number): number => Math.min(last, Math.max(first, Math.min(NOTE_TOP, Math.max(0, n))));
  if (cross >= 0 && cross < blackLen) {
    for (const pc of HIT_ORDER) {
      const c = ACCIDENTAL_CENTRE[pc];
      if (c !== undefined && inOctave >= c - 0.5 && inOctave < c + 0.5) return limit(octave + pc);
    }
  }
  for (const pc of HIT_ORDER) {
    const i = NATURAL_INDEX[pc];
    if (i !== undefined && inOctave >= (NATURAL_EDGES[i] as number) && inOctave < (NATURAL_EDGES[i + 1] as number)) return limit(octave + pc);
  }
  return limit(Math.floor(clamped));
}

/** A face clipped to the window, in rows from note 0: [bottom, top], or null when not visible. */
export function clippedSpan(first: number, last: number, note: number): [number, number] | null {
  const [lo, hi] = keySpan(note);
  const a = Math.max(lo, first, 0);
  const b = Math.min(hi, last + 1, NOTE_TOP + 1);
  return b > a ? [a, b] : null;
}

/** A point of a key outline: `a` along the pitch axis in rows from note 0, `c` across the key from 0 (the side the black keys start on) to 1. */
export type OutlinePoint = readonly [a: number, c: number];

/**
 * The visible shape of a key for drawing it pressed or selected. A white key is its full face except where the black keys
 * cover it, so its outline has a notch at each end that a black key overlaps; a black key is a plain rectangle that reaches
 * `blackFrac` of the way across. Returns null when no part of the key is in the window. Points are in drawing order and the
 * shape is closed (the last point joins the first).
 */
export function keyOutline(first: number, last: number, note: number, blackFrac: number): OutlinePoint[] | null {
  const span = clippedSpan(first, last, note);
  if (!span) return null;
  const [a0, a1] = span;
  if (isAccidental(note)) {
    return [
      [a0, 0],
      [a1, 0],
      [a1, blackFrac],
      [a0, blackFrac],
    ];
  }
  // The stretches of this white key that a black key covers (from the notes around it, whatever the window clips).
  const covered: Array<[number, number]> = [];
  for (let n = Math.max(0, note - 2); n <= Math.min(NOTE_TOP, note + 2); n++) {
    if (!isAccidental(n)) continue;
    const [lo, hi] = keySpan(n);
    const s = Math.max(a0, lo);
    const e = Math.min(a1, hi);
    if (e > s) covered.push([s, e]);
  }
  const eps = 1e-6;
  const isCovered = (a: number): boolean => covered.some(([s, e]) => a > s - eps && a < e + eps);
  const edge = (a: number): number => (isCovered(a) ? blackFrac : 0);
  const breaks = [...new Set(covered.flatMap(([s, e]) => [s, e]).filter((b) => b > a0 + eps && b < a1 - eps))].sort((x, y) => y - x);
  const out: OutlinePoint[] = [
    [a0, 1],
    [a1, 1],
    [a1, edge(a1 - eps)],
  ];
  for (const b of breaks) {
    const above = edge(b + eps);
    const below = edge(b - eps);
    if (above !== below) out.push([b, above], [b, below]);
  }
  out.push([a0, edge(a0 + eps)]);
  return out;
}
