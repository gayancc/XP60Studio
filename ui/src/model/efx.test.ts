import { describe, expect, it } from 'vitest';
import { efxParam, efxShortName, efxTypeLabel, efxTypeOf, EFX_TYPES, efxValueText } from './efx';

describe('the EFX parameter names', () => {
  it('has all 40 types, numbered 1 to 40 in order', () => {
    expect(EFX_TYPES).toHaveLength(40);
    EFX_TYPES.forEach((t, i) => expect(t.type).toBe(i + 1));
    expect(efxTypeLabel(EFX_TYPES[18] as (typeof EFX_TYPES)[number])).toBe('19 TRIPLE-TAP-DELAY');
  });

  it('numbers each type’s parameters 1.. without gaps, and none past twelve', () => {
    for (const t of EFX_TYPES) {
      expect(t.params.length, t.name).toBeGreaterThan(4);
      expect(t.params.length, t.name).toBeLessThanOrEqual(12);
      t.params.forEach((p, i) => expect(p.index, `${t.name} #${i}`).toBe(i + 1));
    }
  });

  it('names the Triple Tap Delay’s twelve parameters in the order Patch Base shows them', () => {
    // The order is the stored order: parameter 1 is Delay L, ... 12 is Level (read from Roland's table, and matching the instrument-side editor's own screen).
    const names = (EFX_TYPES[18]?.params ?? []).map((p) => efxShortName(p.name));
    expect(names).toEqual(['Delay L', 'Delay R', 'Delay C', 'Feedback', 'HF Damp', 'Level L', 'Level R', 'Level C', 'Low Gain', 'High Gain', 'Balance', 'Level']);
  });

  it('keeps raw ranges inside what a parameter can store, and flags unused slots', () => {
    for (const t of EFX_TYPES) {
      for (const p of t.params) {
        if (p.raw === null) {
          expect(p.name).toMatch(/not used/i);
          continue;
        }
        expect(p.raw[0], `${t.name}/${p.name}`).toBeGreaterThanOrEqual(0);
        expect(p.raw[1], `${t.name}/${p.name}`).toBeLessThanOrEqual(127);
        expect(p.raw[1]).toBeGreaterThan(p.raw[0]);
      }
    }
  });

  it('looks a parameter up by slot, and has none for an unused slot or a slot past the end', () => {
    expect(efxParam(0, 1)?.name).toBe('Low Frequency'); // type 1, STEREO-EQ
    expect(efxParam(0, 12)).toBeUndefined(); // STEREO-EQ has eleven
    expect(efxParam(25, 6)).toBeUndefined(); // type 26 OVERDRIVE→CHORUS: parameter 6 is not used
    expect(efxTypeOf(40)).toBeUndefined();
  });

  it('shortens names without losing which parameter they are', () => {
    expect(efxShortName('Delay Time Left')).toBe('Delay L');
    expect(efxShortName('Output Level')).toBe('Level');
    expect(efxShortName('Effect Balance')).toBe('Balance');
    expect(efxShortName('Cutoff Frequency')).toBe('Cutoff Freq');
    expect(efxShortName('High Frequency Slow Rate')).toBe('High Freq Slow Rate');
    expect(efxShortName('Resonance')).toBe('Resonance');
  });
});

describe('the meaning of an EFX value', () => {
  it('shows a gain over 0 to 30 as -15 to +15 dB', () => {
    expect(efxValueText('Low Gain', [0, 30], 0)).toBe('-15 dB');
    expect(efxValueText('High Gain', [0, 30], 15)).toBe('0 dB');
    expect(efxValueText('Band3 Gain', [0, 30], 30)).toBe('+15 dB');
  });

  it('shows the effect balance as the direct to wet mix', () => {
    expect(efxValueText('Effect Balance', [0, 100], 0)).toBe('D100:0W');
    expect(efxValueText('Effect Balance', [0, 100], 50)).toBe('D50:50W');
    expect(efxValueText('Effect Balance', [0, 100], 100)).toBe('D0:100W');
  });

  it('shows the high-frequency damp as a frequency, ending in BYPASS', () => {
    expect(efxValueText('HF Damp', [0, 17], 0)).toBe('200');
    expect(efxValueText('Delay HF Damp', [0, 17], 16)).toBe('8000');
    expect(efxValueText('HF Damp', [0, 17], 17)).toBe('BYPASS');
  });

  it('leaves every other value as its raw number, because the manual’s tables for them are not linear', () => {
    expect(efxValueText('Rate', [0, 125], 60)).toBe('60');
    expect(efxValueText('Delay Time Left', [0, 125], 40)).toBe('40');
    expect(efxValueText('Level', [0, 127], 100)).toBe('100');
    // a gain with a different range is not assumed to be in dB
    expect(efxValueText('Low Gain', [0, 127], 100)).toBe('100');
  });
});
