import { afterEach, beforeAll, describe, expect, it } from 'vitest';
import { cleanup, fireEvent, render } from '@testing-library/svelte';
import { readFileSync } from 'node:fs';
import { catalogJson, initSync } from '../wasm/pkg/xppatch.js';
import { parseCatalog, type Catalog } from '../model/catalog';
import { EFX_TYPES, efxDefault, efxDefaults, efxValueText } from '../model/efx';
import { runInit } from './toolActions';
import { PatchStore } from '../model/store.svelte';
import { MemoryTransport } from '../model/transport';
import { choose } from '../test/dropdown';
import EfxHarness from './EfxHarness.svelte';

let catalog: Catalog;
beforeAll(() => {
  initSync({ module: readFileSync('src/wasm/pkg/xppatch_bg.wasm') });
  catalog = parseCatalog(catalogJson());
});
afterEach(cleanup);

const used = EFX_TYPES.flatMap((t, stored) => t.params.filter((p) => p.raw).map((p) => ({ stored, type: t.name, p, v: efxDefault(stored, p.index) })));
const all = (re: RegExp, range?: readonly [number, number]) => used.filter((u) => re.test(u.p.name) && (!range || (u.p.raw?.[0] === range[0] && u.p.raw?.[1] === range[1])));

describe('the starting values of every EFX type', () => {
  it('has twelve values for each of the 40 types, each inside its parameter range, and 0 for a slot the type does not use', () => {
    expect(EFX_TYPES).toHaveLength(40);
    for (const [stored, t] of EFX_TYPES.entries()) {
      const d = efxDefaults(stored);
      expect(d, t.name).toHaveLength(12);
      for (let i = 1; i <= 12; i++) {
        const p = t.params.find((x) => x.index === i);
        if (p?.raw) expect(d[i - 1], `${t.name} parameter ${i} ${p.name}`).toBeGreaterThanOrEqual(p.raw[0]);
        if (p?.raw) expect(d[i - 1], `${t.name} parameter ${i} ${p.name}`).toBeLessThanOrEqual(p.raw[1]);
        else expect(d[i - 1], `${t.name} unused slot ${i}`).toBe(0);
      }
    }
  });

  it('centres every single pan, and pans the two pitch-shifter voices left and right', () => {
    const pans = all(/Pan$/, [0, 127]).filter((u) => !/Pan [AB]$/.test(u.p.name));
    expect(pans.length).toBeGreaterThan(10);
    for (const u of pans) expect(efxValueText(u.p.name, [0, 127], u.v), `${u.type} ${u.p.name}`).toBe('0');
    expect(all(/Output Pan A$/).map((u) => u.v)).toEqual([0]);
    expect(all(/Output Pan B$/).map((u) => u.v)).toEqual([127]);
  });

  it('sets every gain to 0 dB, every pitch to 0 semitones and 0 cents, and every output level full', () => {
    for (const u of all(/Gain$/, [0, 30])) expect(efxValueText(u.p.name, [0, 30], u.v), `${u.type} ${u.p.name}`).toBe('0 dB');
    for (const u of all(/^Coarse Pitch/)) expect(efxValueText(u.p.name, [0, 36], u.v), u.type).toBe('0 st');
    for (const u of all(/^Fine Pitch/)) expect(efxValueText(u.p.name, [0, 100], u.v), u.type).toBe('0 ct');
    for (const u of all(/Output Level$/)) expect(u.v, `${u.type} ${u.p.name}`).toBe(127);
  });

  it('starts every effect balance at half dry and half wet, and every dry/wet in the middle', () => {
    for (const u of all(/Balance$/, [0, 100])) expect(u.v, `${u.type} ${u.p.name}`).toBe(50);
  });

  it('starts delay feedback at a gentle +20 % (+0 % in the pitch shifters) and lists on their first or their "off" entry', () => {
    for (const u of all(/Feedback Level$/)) {
      const text = efxValueText(u.p.name, [0, 98], u.v);
      expect(text, `${u.type} ${u.p.name}`).toBe(/PITCH/.test(u.type) ? '0%' : '+20%');
    }
    for (const u of all(/^Filter Type$/, [0, 2])) expect(u.v, u.type).toBe(0); // OFF
    for (const u of all(/Feedback (Mode|Phase)/)) expect(u.v, `${u.type} ${u.p.name}`).toBe(0);
  });

  it('is flat for STEREO-EQ, the way 46 of the 75 factory patches that use it are', () => {
    expect([...efxDefaults(0)]).toEqual([0, 15, 0, 15, 7, 1, 15, 7, 1, 15, 127, 0]);
  });

  it('does not give a threshold, a mix or a sensitivity the value of an output level', () => {
    expect(efxDefault(9, 1)).toBe(64); // LIMITER threshold
    expect(efxDefault(5, 2)).toBe(64); // ENHANCER mix
    expect(efxDefault(6, 4)).toBe(64); // AUTO-WAH sensitivity
    expect(efxDefault(22, 4)).toBe(0); // FBK-PITCH-SHIFTER pre-delay is not the feedback slot
    expect(efxDefault(22, 5)).toBe(49); // its feedback is, and starts at 0 %
  });
});

describe('choosing an effect type', () => {
  const make = (efxType: number): PatchStore => {
    const store = new PatchStore(catalog, new MemoryTransport());
    store.newPatch();
    store.set('common', 'efx_type', efxType);
    return store;
  };
  const params = (s: PatchStore): number[] => Array.from({ length: 12 }, (_, i) => s.get('common', `efx_parameter_${i + 1}`));

  it('loads that type’s own starting values, replacing what the last type left, as one undo step', async () => {
    const store = make(18); // TRIPLE-TAP-DELAY with whatever it holds
    for (let i = 1; i <= 12; i++) store.set('common', `efx_parameter_${i}`, 127); // every slot at the far end, like a stale Level
    const { getByLabelText } = render(EfxHarness, { props: { store } });
    await choose(getByLabelText('EFX type'), '02 OVERDRIVE');
    expect(store.get('common', 'efx_type')).toBe(1);
    expect(params(store)).toEqual([...efxDefaults(1)]);
    expect(store.get('common', 'efx_parameter_2')).toBe(64); // Output Pan: centre, not full right
    store.undo();
    expect(store.get('common', 'efx_type')).toBe(18);
    expect(params(store)).toEqual(Array.from({ length: 12 }, () => 127));
  });

  it('does nothing when the type picked is the one already chosen', async () => {
    const store = make(1);
    store.set('common', 'efx_parameter_2', 100);
    const { getByLabelText } = render(EfxHarness, { props: { store } });
    await choose(getByLabelText('EFX type'), '02 OVERDRIVE');
    expect(store.get('common', 'efx_parameter_2')).toBe(100);
  });

  it('steps through all 40 types with the arrow buttons and every one lands on its own starting values', async () => {
    const store = make(0);
    const { getByRole } = render(EfxHarness, { props: { store } });
    for (let n = 1; n < 40; n++) {
      await fireEvent.click(getByRole('button', { name: 'Next EFX type' }));
      expect(store.get('common', 'efx_type')).toBe(n);
      expect(params(store), EFX_TYPES[n]?.name).toEqual([...efxDefaults(n)]);
    }
  });

  it('Init gives a new sound STEREO-EQ with its flat settings, not the bottom of every range', () => {
    const store = make(7);
    for (let i = 1; i <= 12; i++) store.set('common', `efx_parameter_${i}`, 3);
    runInit(store);
    expect(store.get('common', 'efx_type')).toBe(0);
    expect(params(store)).toEqual([...efxDefaults(0)]);
  });
});
