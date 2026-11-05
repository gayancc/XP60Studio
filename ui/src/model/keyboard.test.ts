import { describe, expect, it } from 'vitest';
import { HeldNotes, isBlack, layoutKeys, noteForCode, velocityAt } from './keyboard';

describe('computer keyboard', () => {
  it('maps the lower row from the base octave and the upper row one octave up', () => {
    expect(noteForCode('KeyZ', 4)).toBe(60);
    expect(noteForCode('KeyS', 4)).toBe(61);
    expect(noteForCode('KeyM', 4)).toBe(71);
    expect(noteForCode('KeyQ', 4)).toBe(72);
    expect(noteForCode('Digit2', 4)).toBe(73);
    expect(noteForCode('KeyI', 4)).toBe(84);
  });
  it('ignores other keys and stays inside 0 to 127', () => {
    expect(noteForCode('KeyP', 4)).toBeNull();
    expect(noteForCode('Space', 4)).toBeNull();
    expect(noteForCode('KeyI', 8)).toBeNull(); // 8 + 1 = 9 octaves up would pass 127
    expect(noteForCode('KeyZ', 0)).toBe(12);
    expect(noteForCode('KeyZ', -1)).toBe(0);
    expect(noteForCode('KeyS', -2)).toBeNull();
  });
});

describe('key layout', () => {
  it('knows black keys', () => {
    expect([60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71].filter(isBlack)).toEqual([61, 63, 66, 68, 70]);
  });
  it('lays out an octave: seven white keys and five black keys between them', () => {
    const keys = layoutKeys(60, 12);
    expect(keys).toHaveLength(12);
    expect(keys.filter((k) => !k.black)).toHaveLength(7);
    expect(keys.filter((k) => k.black)).toHaveLength(5);
    const whites = keys.filter((k) => !k.black).map((k) => k.x);
    expect(whites).toEqual([0, 1, 2, 3, 4, 5, 6]);
    const cSharp = keys.find((k) => k.note === 61)!;
    expect(cSharp.x).toBeGreaterThan(0.5);
    expect(cSharp.x).toBeLessThan(1);
  });
  it('never goes past note 127', () => {
    expect(layoutKeys(120, 30).at(-1)?.note).toBe(127);
    expect(layoutKeys(0, 0)).toEqual([]);
  });
});

describe('velocity', () => {
  it('rises from soft at the top to hard at the bottom and always stays valid', () => {
    expect(velocityAt(0)).toBeLessThan(velocityAt(1));
    expect(velocityAt(0)).toBeGreaterThanOrEqual(1);
    expect(velocityAt(1)).toBeLessThanOrEqual(127);
    expect(velocityAt(-5)).toBe(velocityAt(0));
    expect(velocityAt(9)).toBe(velocityAt(1));
    expect(velocityAt(Number.NaN)).toBeGreaterThan(0);
  });
});

describe('held notes', () => {
  it('reports each press and release once', () => {
    const h = new HeldNotes();
    expect(h.press(60)).toBe(true);
    expect(h.press(60)).toBe(false); // key repeat
    expect(h.has(60)).toBe(true);
    expect(h.release(60)).toBe(true);
    expect(h.release(60)).toBe(false);
    expect(h.size).toBe(0);
  });
  it('releases everything that is still down', () => {
    const h = new HeldNotes();
    h.press(60);
    h.press(64);
    expect(h.releaseAll().sort()).toEqual([60, 64]);
    expect(h.size).toBe(0);
    expect(h.releaseAll()).toEqual([]);
    h.press(40);
    expect(h.list()).toEqual([40]);
    expect(h.size).toBe(1);
  });
});
