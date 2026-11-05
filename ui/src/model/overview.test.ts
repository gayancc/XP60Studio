import { beforeAll, describe, expect, it } from 'vitest';
import { readFileSync } from 'node:fs';
import { catalogJson, initSync } from '../wasm/pkg/xppatch.js';
import { parseCatalog, type Catalog } from './catalog';
import { summarisePatch } from './overview';
import { PatchStore } from './store.svelte';
import { MemoryTransport } from './transport';

let catalog: Catalog;
const BANK = new Uint8Array(readFileSync('../tests/golden/xp60-user-bank.syx'));

beforeAll(() => {
  initSync({ module: readFileSync('src/wasm/pkg/xppatch_bg.wasm') });
  catalog = parseCatalog(catalogJson());
});

const make = (): PatchStore => new PatchStore(catalog, new MemoryTransport());

describe('patch overview', () => {
  it('summarises a real patch with the values the editor shows', () => {
    const store = make();
    store.loadSyx(BANK, 1, 'bank.syx');
    const s = summarisePatch(store);
    expect(s.name).toBe(store.name.trimEnd());
    expect(s.tones).toHaveLength(4);
    s.tones.forEach((t, i) => {
      const b = `tone${i + 1}`;
      expect(t.number).toBe(i + 1);
      expect(t.on).toBe(store.get(b, 'tone_switch') === 1);
      expect(t.level).toBe(store.format(b, 'tone_level'));
      const base = `${store.format(b, 'wave_group_type')} ${store.format(b, 'wave_number')}`;
      // the group and number first, then Roland's name for the wave when the lists have it
      expect(t.wave === base || t.wave.startsWith(`${base} · `)).toBe(true);
    });
    expect(s.effects.map((e) => e.label)).toEqual(['EFX', 'Chorus level', 'Reverb']);
    expect(s.level).toBe(store.format('common', 'patch_level'));
  });

  it('names the wave of each Tone as the sounds themselves imply (checked against the real bank)', () => {
    const waves = (slot: number): string[] => {
      const store = make();
      store.loadSyx(BANK, slot, 'bank.syx');
      return summarisePatch(store).tones.map((t) => t.wave.split(' · ')[1] ?? '');
    };
    // Patches named for what they contain use exactly the waves those names imply, from the instrument and from expansion board 1.
    expect(waves(17)).toEqual(['Clav 2A', 'Clav 3A', 'Clav 3B', 'Clav 4A']);
    expect(waves(88)).toEqual(["60's Organ 1", "60's Organ 2", "60's Organ 3", "60's Organ 4"]);
    expect(waves(8).slice(0, 2)).toEqual(['Alto Sax 1A', 'Alto Sax 1C']);
    expect(waves(46)[0]).toBe('Clav 1A');
  });

  it('says "all keys" and "all velocities" for full ranges and names narrower ones', () => {
    const store = make();
    store.set('tone1', 'keyboard_range_lower', 0);
    store.set('tone1', 'keyboard_range_upper', 127);
    store.set('tone1', 'velocity_range_lower', 1);
    store.set('tone1', 'velocity_range_upper', 127);
    store.set('tone2', 'keyboard_range_lower', 36);
    store.set('tone2', 'keyboard_range_upper', 60);
    store.set('tone2', 'velocity_range_lower', 64);
    store.set('tone2', 'velocity_range_upper', 127);
    store.set('tone3', 'keyboard_range_lower', 60);
    store.set('tone3', 'keyboard_range_upper', 60);
    const t = summarisePatch(store).tones;
    expect(t[0]?.keys).toBe('all keys');
    expect(t[0]?.velocity).toBe('all velocities');
    expect(t[1]?.keys).toBe('C2 to C4');
    expect(t[1]?.velocity).toBe('64 to 127');
    expect(t[2]?.keys).toBe('C4');
  });

  it('follows edits', () => {
    const store = make();
    store.set('tone2', 'tone_switch', 1);
    expect(summarisePatch(store).tones[1]?.on).toBe(true);
    store.set('tone2', 'tone_switch', 0);
    expect(summarisePatch(store).tones[1]?.on).toBe(false);
  });
});
