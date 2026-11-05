import { describe, expect, it } from 'vitest';
import { EFX_TYPES, efxChoices, efxValueText } from './efx';
import { EFX_FAMILIES, efxBlurb, familyOf, sectionsOf } from './efxFamilies';

describe('effect families', () => {
  it('place each of the 40 types in exactly one family', () => {
    const all = EFX_FAMILIES.flatMap((f) => f.types).sort((a, b) => a - b);
    expect(all).toEqual(Array.from({ length: 40 }, (_, i) => i + 1));
    expect(familyOf(19).id).toBe('delay');
    expect(familyOf(40).id).toBe('combo');
  });

  it('put every parameter a type uses in exactly one section, in no section if unused', () => {
    for (const t of EFX_TYPES) {
      const used = t.params.filter((p) => p.raw).map((p) => p.index).sort((a, b) => a - b);
      const placed = sectionsOf(t).flatMap((s) => s.params.map((p) => p.index)).sort((a, b) => a - b);
      expect(placed, t.name).toEqual(used);
    }
  });

  it('split a combination by stage', () => {
    const t = EFX_TYPES[25]!; // 26 OVERDRIVE→CHORUS
    expect(sectionsOf(t).map((s) => s.title)).toEqual(['Overdrive', 'Chorus', 'Output']);
    expect(efxBlurb(t)).toMatch(/into/);
    expect(efxBlurb(EFX_TYPES[37]!)).toMatch(/side by side/);
  });
});

describe('manual-backed value text', () => {
  it('lists words for the discrete parameters', () => {
    expect(efxChoices('Reverb Type', [0, 5])).toHaveLength(6);
    expect(efxValueText('Amp Simulator Type', [0, 3], 2)).toBe('2-STACK');
    expect(efxValueText('Speed', [0, 1], 1)).toBe('FAST');
    expect(efxChoices('Filter Type', [0, 2])).toEqual(['OFF', 'LPF', 'HPF']);
  });
  it('converts pan, pitch, feedback and phase', () => {
    expect(efxValueText('Output Pan', [0, 127], 0)).toBe('L64');
    expect(efxValueText('Output Pan', [0, 127], 64)).toBe('0');
    expect(efxValueText('Output Pan A', [0, 127], 127)).toBe('63R');
    expect(efxValueText('Coarse Pitch A', [0, 36], 0)).toBe('-24 st');
    expect(efxValueText('Coarse Pitch', [0, 36], 36)).toBe('+12 st');
    expect(efxValueText('Fine Pitch B', [0, 100], 100)).toBe('+100 ct');
    expect(efxValueText('Feedback Level', [0, 98], 98)).toBe('+98%');
    expect(efxValueText('Phase', [0, 90], 90)).toBe('180°');
    expect(efxValueText('Peaking1 Frequency', [0, 16], 16)).toBe('8000 Hz');
  });
});
