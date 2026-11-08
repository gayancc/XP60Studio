import { describe, expect, it } from 'vitest';
import { patchNameEnergyHint, waveEnergyFromName, waveformBaseEnergy, WAVE_ENERGY_NEUTRAL } from './waveEnergy';

describe('waveEnergy', () => {
  it('ranks acid/dist above soft winds and mariachi', () => {
    expect(waveEnergyFromName('Dist Lead').energy).toBeGreaterThan(waveEnergyFromName('Flute').energy);
    expect(waveEnergyFromName('TB-303 Bass').energy).toBeGreaterThan(waveEnergyFromName('Mariachi').energy);
  });

  it('uses neutral energy when the name is missing and no patch hint', () => {
    expect(waveEnergyFromName(undefined).energy).toBe(WAVE_ENERGY_NEUTRAL);
    expect(waveEnergyFromName(undefined).category).toBe('unknown');
  });

  it('raises EXP-unknown waves via patch title (FLASH SAX / Dist TB-303)', () => {
    expect(patchNameEnergyHint('FLASH SAX xp')!.energy).toBeGreaterThan(0.95);
    expect(waveformBaseEnergy(2, 11, 3, 'Dist TB-303').energy).toBeGreaterThan(1.0);
  });

  it('resolves INT-A wave numbers through the factory name list', () => {
    const w = waveformBaseEnergy(0, 1, 0);
    expect(w.name).toBeTruthy();
    expect(w.energy).toBeGreaterThan(0.1);
  });
});
