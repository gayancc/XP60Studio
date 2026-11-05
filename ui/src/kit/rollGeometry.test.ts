import { describe, expect, it } from 'vitest';
import { blackWidth, keyOutline, keyRect, keySpan, noteAtKeyboard, noteAtRow, rowHeight, visibleKeys, type OutlinePoint, type RollView } from './rollGeometry';

const view: RollView = { first: 48, last: 71, height: 24 * 10 }; // two octaves, 10 px per row

describe('piano roll keyboard geometry', () => {
  it('tiles the octave with 1.5 / 2 / 2 / 1.5 / 1.5 / 2 / 2 ... natural spans', () => {
    const heights = [0, 2, 4, 5, 7, 9, 11].map((pc) => {
      const [lo, hi] = keySpan(60 + pc);
      return hi - lo;
    });
    expect(heights).toEqual([1.5, 2, 1.5, 1.5, 2, 2, 1.5]);
  });

  it('has naturals that tile the octave with no gaps', () => {
    let edge = 60;
    for (const pc of [0, 2, 4, 5, 7, 9, 11]) {
      const [lo, hi] = keySpan(60 + pc);
      expect(lo).toBe(edge);
      edge = hi;
    }
    expect(edge).toBe(72);
  });

  it('centres each accidental on a natural boundary and makes it one row tall', () => {
    for (const [pc, centre] of [[1, 61.5], [3, 63.5], [6, 66.5], [8, 68.5], [10, 70.5]] as const) {
      const [lo, hi] = keySpan(60 + pc);
      expect(hi - lo).toBe(1);
      expect((lo + hi) / 2).toBe(centre);
    }
  });

  it('puts higher notes higher on screen', () => {
    const a = keyRect(view, 60);
    const b = keyRect(view, 64);
    expect(a && b && b.y < a.y).toBe(true);
  });

  it('clips a face at the window edge', () => {
    const c = keyRect({ first: 61, last: 71, height: 110 }, 60); // C60 spans rows 60..61.5, only 61..61.5 in view
    expect(c).not.toBeNull();
    expect(c!.h).toBeCloseTo(5, 5);
    expect(keyRect({ first: 62, last: 71, height: 100 }, 60)).toBeNull();
  });

  it('clips at the ends of MIDI', () => {
    const top = keyRect({ first: 120, last: 127, height: 80 }, 127);
    expect(top!.y).toBe(0);
    expect(top!.y + top!.h).toBeCloseTo(15, 5); // G127 spans rows 126.5..128 after clipping 128.5
  });

  it('paints naturals before accidentals', () => {
    const keys = visibleKeys(view);
    const firstAcc = keys.findIndex((k) => k.accidental);
    expect(keys.slice(firstAcc).every((k) => k.accidental)).toBe(true);
    expect(keys.slice(0, firstAcc).every((k) => !k.accidental)).toBe(true);
  });

  it('hit tests accidentals first, and the white tail resolves to the natural', () => {
    const rh = rowHeight(view);
    const kbW = 66;
    const bw = blackWidth(kbW);
    // Middle of row 61 (C#4): black at the left, but the natural beneath at the right of the same row.
    const y = (row: number): number => (view.last + 1 - row) * rh;
    expect(noteAtKeyboard(view, kbW, bw, 4, y(61.5))).toBe(61);
    expect(noteAtKeyboard(view, kbW, bw, kbW - 3, y(61.5))).toBe(62); // the D face covers 61.5..63.5
    expect(noteAtKeyboard(view, kbW, bw, kbW - 3, y(60.5))).toBe(60);
    expect(noteAtKeyboard(view, kbW, bw, 4, y(60.5))).toBe(60); // C is natural: no black key on row 60
  });

  it('hit tests every note centre back to itself', () => {
    const rh = rowHeight(view);
    const kbW = 66;
    const bw = blackWidth(kbW);
    for (let n = view.first; n <= view.last; n++) {
      const [lo, hi] = keySpan(n);
      const row = (Math.max(lo, view.first) + Math.min(hi, view.last + 1)) / 2;
      const x = n % 12 === 1 || n % 12 === 3 || n % 12 === 6 || n % 12 === 8 || n % 12 === 10 ? 3 : kbW - 3;
      expect(noteAtKeyboard(view, kbW, bw, x, (view.last + 1 - row) * rh)).toBe(n);
    }
  });

  it('clamps points outside the keyboard to the window', () => {
    expect(noteAtKeyboard(view, 66, 40, 10, -50)).toBeLessThanOrEqual(71);
    expect(noteAtKeyboard(view, 66, 40, 10, 9999)).toBeGreaterThanOrEqual(48);
  });

  it('maps grid rows uniformly', () => {
    expect(noteAtRow(view, 5)).toBe(71);
    expect(noteAtRow(view, 235)).toBe(48);
  });
});

const area = (pts: readonly OutlinePoint[]): number => {
  let sum = 0;
  pts.forEach((p, i) => {
    const q = pts[(i + 1) % pts.length] as OutlinePoint;
    sum += p[0] * q[1] - q[0] * p[1];
  });
  return Math.abs(sum) / 2;
};

describe('the visible outline of a pressed key', () => {
  const BF = 2 / 3;

  it('an accidental is a plain rectangle that reaches the black-key depth', () => {
    const o = keyOutline(48, 71, 61, BF);
    expect(o).toEqual([
      [61, 0],
      [62, 0],
      [62, BF],
      [61, BF],
    ]);
  });

  it('a white key is its face minus what the black keys cover: its area shrinks by exactly the overlap', () => {
    // [pitch class, rows of black key overlap with this white key, as depth fraction applies]
    const cases: Array<[number, number]> = [
      [60, 0.5], // C: C# covers its top half row
      [62, 0.5 + 0.5], // D: C# covers the bottom half row and D# the top half row
      [64, 0.5], // E: D# covers its bottom half row
      [65, 0.5], // F: F# covers its top half row
      [67, 1], // G: F# and G# each half a row
      [69, 1], // A: G# and A# each half a row
      [71, 0.5], // B: A# covers its bottom half row
    ];
    for (const [note, overlap] of cases) {
      const [lo, hi] = keySpan(note);
      const o = keyOutline(48, 83, note, BF) as OutlinePoint[];
      expect(area(o)).toBeCloseTo((hi - lo) - overlap * BF, 6);
    }
  });

  it('never leaves the face of the key', () => {
    for (let note = 0; note < 128; note++) {
      const o = keyOutline(0, 127, note, BF);
      expect(o).not.toBeNull();
      const [lo, hi] = keySpan(note);
      for (const [a, c] of o as OutlinePoint[]) {
        expect(a).toBeGreaterThanOrEqual(Math.max(0, lo) - 1e-9);
        expect(a).toBeLessThanOrEqual(Math.min(128, hi) + 1e-9);
        expect(c).toBeGreaterThanOrEqual(0);
        expect(c).toBeLessThanOrEqual(1);
      }
    }
  });

  it('is clipped at the window and absent when the key is outside it', () => {
    const o = keyOutline(62, 71, 60, BF); // C60 spans 60..61.5: nothing of it is in 62..71
    expect(o).toBeNull();
    const clipped = keyOutline(61, 71, 60, BF) as OutlinePoint[]; // only 61..61.5 of C is in view, and it is under C#
    expect(Math.max(...clipped.map((p) => p[0]))).toBeLessThanOrEqual(61.5);
    expect(Math.min(...clipped.map((p) => p[0]))).toBeGreaterThanOrEqual(61);
  });
});
