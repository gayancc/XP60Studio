import { describe, expect, it } from 'vitest';
import { panelLabel, patchNumber, posOf, slotOf, soundSelect } from './bankPanel';

describe('front-panel patch selection', () => {
  it('matches the manual: a11 is 001, a88 is 064, b11 is 065, b88 is 128', () => {
    expect(panelLabel(0)).toBe('a11');
    expect(panelLabel(7)).toBe('a18');
    expect(panelLabel(8)).toBe('a21');
    expect(panelLabel(63)).toBe('a88');
    expect(panelLabel(64)).toBe('b11');
    expect(panelLabel(127)).toBe('b88');
    expect(patchNumber(0)).toBe('001');
    expect(patchNumber(127)).toBe('128');
  });

  it('is a bijection over all 128 patches', () => {
    const seen = new Set<string>();
    for (let s = 0; s < 128; s++) {
      expect(slotOf(posOf(s))).toBe(s);
      seen.add(panelLabel(s));
    }
    expect(seen.size).toBe(128);
  });

  it('clamps indexes outside the range', () => {
    expect(posOf(-3)).toEqual({ sub: 0, bank: 1, num: 1 });
    expect(posOf(500)).toEqual({ sub: 1, bank: 8, num: 8 });
  });
});

describe('bank select for the panel groups', () => {
  it("matches the manual's table (p.181)", () => {
    expect(soundSelect('USER', 0)).toEqual({ msb: 80, lsb: 0, program: 0 });
    expect(soundSelect('PR-A', 127)).toEqual({ msb: 81, lsb: 0, program: 127 });
    expect(soundSelect('PR-B', 9)).toEqual({ msb: 81, lsb: 1, program: 9 });
    expect(soundSelect('PR-C', 0)?.lsb).toBe(2);
    expect(soundSelect('PR-D', 0)?.lsb).toBe(3);
    expect(soundSelect('XP-A', 0)).toMatchObject({ msb: 84, lsb: 0 });
    expect(soundSelect('XP-D', 0)).toMatchObject({ msb: 84, lsb: 6 });
  });

  it('refuses an unknown group or a slot outside 0..127', () => {
    expect(soundSelect('CARD', 0)).toBeNull();
    expect(soundSelect('USER', -1)).toBeNull();
    expect(soundSelect('USER', 128)).toBeNull();
    expect(soundSelect('USER', 1.5)).toBeNull();
  });
});
