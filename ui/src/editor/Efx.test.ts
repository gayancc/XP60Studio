import { afterEach, beforeAll, describe, expect, it } from 'vitest';
import { cleanup, fireEvent, render } from '@testing-library/svelte';
import { readFileSync } from 'node:fs';
import { catalogJson, initSync } from '../wasm/pkg/xppatch.js';
import { parseCatalog, type Catalog } from '../model/catalog';
import { PatchStore } from '../model/store.svelte';
import { MemoryTransport } from '../model/transport';
import { checked, choose, entries, groupLabels, shown } from '../test/dropdown';
import EfxHarness from './EfxHarness.svelte';

let catalog: Catalog;
beforeAll(() => {
  initSync({ module: readFileSync('src/wasm/pkg/xppatch_bg.wasm') });
  catalog = parseCatalog(catalogJson());
});
afterEach(cleanup);

const make = (efxType: number): PatchStore => {
  const store = new PatchStore(catalog, new MemoryTransport());
  store.newPatch();
  store.set('common', 'efx_type', efxType);
  return store;
};
const knobs = (c: HTMLElement): string[] => [...c.querySelectorAll('[role=slider]')].map((e) => e.getAttribute('aria-label') ?? '');
const choices = (c: HTMLElement): number => c.querySelectorAll('[role=radiogroup], [role=combobox]').length;

describe('the Effects plug-in', () => {
  it('lists the 40 effect types by name in one drop-down list with a heading bar per family', async () => {
    const { getByLabelText } = render(EfxHarness, { props: { store: make(18) } });
    const trigger = getByLabelText('EFX type');
    expect(shown(trigger)).toBe('19 TRIPLE-TAP-DELAY');
    expect(await groupLabels(trigger)).toEqual(['EQ', 'Drive', 'Dynamics', 'Filter', 'Chorus', 'Delay', 'Pitch', 'Reverb', 'Combos']);
    const all = await entries(trigger);
    expect(all).toHaveLength(40);
    expect(all[18]).toBe('19 TRIPLE-TAP-DELAY');
    expect(await checked(trigger)).toBe('19 TRIPLE-TAP-DELAY');
  });

  it('draws only the controls the selected type uses, each named for it', () => {
    const triple = render(EfxHarness, { props: { store: make(18) } }); // all twelve are amounts
    expect(knobs(triple.container)).toHaveLength(12);
    cleanup();
    const eq = render(EfxHarness, { props: { store: make(0) } }); // STEREO-EQ: eleven, two of them two-way choices
    expect(knobs(eq.container)).toHaveLength(9);
    expect(choices(eq.container)).toBe(3); // the type list and two choices
    cleanup();
    const od = render(EfxHarness, { props: { store: make(1) } }); // OVERDRIVE: six, the amp type a choice
    expect(knobs(od.container)).toHaveLength(5);
    expect(choices(od.container)).toBe(2);
  });

  it('changes the controls when the type changes, and the change is one undo step', async () => {
    const store = make(0);
    const { getByRole, container } = render(EfxHarness, { props: { store } });
    expect(knobs(container)).toHaveLength(9);
    await fireEvent.click(getByRole('button', { name: 'Next EFX type' }));
    expect(store.get('common', 'efx_type')).toBe(1);
    expect(knobs(container)).toHaveLength(5);
    store.undo();
    expect(store.get('common', 'efx_type')).toBe(0);
  });

  it('picks an effect from its family list as one undo step', async () => {
    const store = make(18);
    const { getByLabelText } = render(EfxHarness, { props: { store } });
    await choose(getByLabelText('EFX type'), '25 GATE-REVERB');
    expect(store.get('common', 'efx_type')).toBe(24);
    store.undo();
    expect(store.get('common', 'efx_type')).toBe(18);
  });

  it('shows gains in dB, the balance as the direct to wet mix, and pan as L/R', () => {
    const store = make(18); // TRIPLE-TAP-DELAY: parameter 9 Low Gain, 11 Effect Balance
    store.set('common', 'efx_parameter_9', 0);
    store.set('common', 'efx_parameter_11', 50);
    const { container } = render(EfxHarness, { props: { store } });
    const meanings = [...container.querySelectorAll('[role=slider]')].map((e) => e.getAttribute('aria-valuetext'));
    expect(meanings).toContain('-15 dB');
    expect(meanings).toContain('D50:50W');
  });
});
