import { describe, expect, it } from 'vitest';
import { fadeAmount, formPath, lfoTrace, lfoWave, type LfoSettings } from './lfoShape';

const base: LfoSettings = { form: 0, rate: 64, offset: 2, delay: 0, fadeMode: 0, fade: 0, keySync: true };

describe('LFO forms (manual chart)', () => {
  it('every form stays inside -1..1', () => {
    for (let f = 0; f < 8; f++) for (let i = 0; i < 400; i++) expect(Math.abs(lfoWave(f, i / 37))).toBeLessThanOrEqual(1);
  });
  it('TRI, SIN and SAW start on the centre line and rise (XP-60 display)', () => {
    for (const f of [0, 1, 2]) {
      expect(lfoWave(f, 0)).toBeCloseTo(0);
      expect(lfoWave(f, 0.1)).toBeGreaterThan(0);
    }
    expect(lfoWave(0, 0.25)).toBe(1);
    expect(lfoWave(0, 0.75)).toBe(-1);
    expect(lfoWave(1, 0.25)).toBeCloseTo(1);
    expect(lfoWave(2, 0.49)).toBeGreaterThan(0.9);
  });
  it('SQR has only two levels, TRP holds them', () => {
    const levels = new Set(Array.from({ length: 50 }, (_, i) => lfoWave(3, i / 50)));
    expect([...levels].sort()).toEqual([-1, 1]);
    expect(lfoWave(4, 0.3)).toBe(1);
    expect(lfoWave(4, 0.8)).toBe(-1);
  });
  it('S&H holds each level, RND varies smoothly, CHS changes constantly', () => {
    expect(lfoWave(5, 0.1)).toBe(lfoWave(5, 0.4));
    expect(Math.abs(lfoWave(6, 0.1) - lfoWave(6, 0.1001))).toBeLessThan(0.01);
    expect(new Set(Array.from({ length: 40 }, (_, i) => lfoWave(7, i / 40))).size).toBeGreaterThan(20);
  });
  it('draws a path for each form', () => {
    for (let f = 0; f < 8; f++) expect(formPath(f, 30, 14).startsWith('M')).toBe(true);
  });
});

describe('Fade Mode graphs (manual)', () => {
  it('ON-IN is silent through the delay, then rises to full', () => {
    expect(fadeAmount(0, 0.03, 127, 127)).toBe(0);
    expect(fadeAmount(0, 0.99, 64, 64)).toBe(1);
  });
  it('ON-OUT is full through the delay, then falls to nothing', () => {
    expect(fadeAmount(1, 0.03, 127, 127)).toBe(1);
    expect(fadeAmount(1, 0.99, 64, 64)).toBe(0);
  });
  it('OFF-IN and OFF-OUT start at the key release', () => {
    expect(fadeAmount(2, 0.1, 0, 0)).toBe(0);
    expect(fadeAmount(3, 0.1, 0, 0)).toBe(1);
    expect(fadeAmount(2, 0.99, 0, 0)).toBe(1);
    expect(fadeAmount(3, 0.99, 0, 0)).toBe(0);
  });
  it('a longer fade ramps later', () => {
    expect(fadeAmount(0, 0.5, 0, 127)).toBeLessThan(fadeAmount(0, 0.5, 0, 0));
  });
});

describe('trace follows the settings', () => {
  it('Offset moves the middle line up and down', () => {
    expect(lfoTrace({ ...base, offset: 4 }).centre).toBeGreaterThan(lfoTrace({ ...base, offset: 2 }).centre);
    expect(lfoTrace({ ...base, offset: 0 }).centre).toBeLessThan(0);
    expect(lfoTrace({ ...base, offset: 2 }).centre).toBe(0);
  });
  it('Rate adds cycles except for the chaos wave', () => {
    const crossings = (t: ReturnType<typeof lfoTrace>): number => t.line.filter((p, i) => i > 0 && p.y * (t.line[i - 1]?.y ?? 0) < 0).length;
    expect(crossings(lfoTrace({ ...base, rate: 127 }))).toBeGreaterThan(crossings(lfoTrace({ ...base, rate: 0 })));
    expect(lfoTrace({ ...base, form: 7, rate: 0 }).line.map((p) => p.y)).toEqual(lfoTrace({ ...base, form: 7, rate: 127 }).line.map((p) => p.y));
  });
  it('Key Trigger restarts the cycle at the key press', () => {
    const on = lfoTrace({ ...base, keySync: true }).line[250]?.y;
    const off = lfoTrace({ ...base, keySync: false }).line[250]?.y;
    expect(on).not.toBe(off);
  });
  it('only the OFF modes mark a key release', () => {
    expect(lfoTrace({ ...base, fadeMode: 1 }).keyOff).toBeNull();
    expect(lfoTrace({ ...base, fadeMode: 2 }).keyOff).not.toBeNull();
  });
});
