import { describe, expect, it } from 'vitest';
import { centsOf, EQUAL_RAW, formatCents, isEqualScale, presetOf, rawOf, SCALE_PRESETS, targetName } from './scaleTune';

describe('the keyboard scale', () => {
  it('stores equal temperament as 64 and converts both ways within the 0 to 127 range', () => {
    expect(EQUAL_RAW).toBe(64);
    expect(centsOf(64)).toBe(0);
    expect(centsOf(0)).toBe(-64);
    expect(centsOf(127)).toBe(63);
    expect(rawOf(0)).toBe(64);
    expect(rawOf(-64)).toBe(0);
    expect(rawOf(63)).toBe(127);
    expect(rawOf(200)).toBe(127);
    expect(rawOf(-200)).toBe(0);
    expect(rawOf(4.6)).toBe(69);
  });

  it('writes cents with a sign and a true minus', () => {
    expect(formatCents(0)).toBe('0');
    expect(formatCents(16)).toBe('+16');
    expect(formatCents(-8)).toBe('−8');
  });

  it('has the manual’s three example scales, every note inside the instrument’s range', () => {
    expect(SCALE_PRESETS.map((p) => p.id)).toEqual(['equal', 'pure', 'arabian']);
    for (const p of SCALE_PRESETS) {
      expect(p.cents).toHaveLength(12);
      for (const c of p.cents) expect(c >= -64 && c <= 63, `${p.name}: ${c}`).toBe(true);
    }
  });

  it('matches the printed table (Owner’s Manual p.89)', () => {
    const pure = SCALE_PRESETS[1]?.cents;
    // C, C#, D, D#, E, F, F#, G, G#, A, A#, B
    expect(pure).toEqual([0, -8, 4, 16, -14, -2, -10, 2, 14, -16, 14, -12]);
    const arabian = SCALE_PRESETS[2]?.cents;
    expect(arabian).toEqual([-6, 45, -2, -12, -51, -8, 43, -4, 47, 0, -10, -49]);
  });

  it('recognises a preset from raw values, and equal temperament', () => {
    const raws = SCALE_PRESETS[1]?.cents.map(rawOf) ?? [];
    expect(presetOf(raws)?.id).toBe('pure');
    expect(presetOf(Array(12).fill(64))?.id).toBe('equal');
    expect(presetOf([...raws.slice(0, 11), 70])).toBeUndefined();
    expect(isEqualScale(Array(12).fill(64))).toBe(true);
    expect(isEqualScale(raws)).toBe(false);
  });

  it('names the targets', () => {
    expect(targetName(0)).toBe('Part 1');
    expect(targetName(15)).toBe('Part 16');
    expect(targetName(16)).toBe('Patch mode');
  });
});
