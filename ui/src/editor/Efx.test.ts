import { afterEach, beforeAll, describe, expect, it } from 'vitest';
import { cleanup, fireEvent, render } from '@testing-library/svelte';
import { readFileSync } from 'node:fs';
import { catalogJson, initSync } from '../wasm/pkg/xppatch.js';
import { parseCatalog, type Catalog } from '../model/catalog';
import { PatchStore } from '../model/store.svelte';
import { MemoryTransport } from '../model/transport';
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

describe('the EFX type and parameter controls', () => {
  it('lists the 40 effect types by name', () => {
    const { getByLabelText } = render(EfxHarness, { props: { store: make(0) } });
    const select = getByLabelText('EFX type') as HTMLSelectElement;
    expect(select.options).toHaveLength(40);
    expect(select.options[18]?.text).toBe('19 TRIPLE-TAP-DELAY');
  });

  it('draws only the knobs the selected type uses, named for it', () => {
    const triple = render(EfxHarness, { props: { store: make(18) } });
    expect(knobs(triple.container)).toHaveLength(12);
    cleanup();
    const eq = render(EfxHarness, { props: { store: make(0) } }); // STEREO-EQ has eleven
    expect(knobs(eq.container)).toHaveLength(11);
    cleanup();
    const od = render(EfxHarness, { props: { store: make(1) } }); // OVERDRIVE has six
    expect(knobs(od.container)).toHaveLength(6);
  });

  it('changes the knobs when the type changes, and the change is one undo step', async () => {
    const store = make(18);
    const { getByLabelText, container } = render(EfxHarness, { props: { store } });
    await fireEvent.change(getByLabelText('EFX type'), { target: { value: '1' } });
    expect(store.get('common', 'efx_type')).toBe(1);
    expect(knobs(container)).toHaveLength(6);
    store.undo();
    expect(store.get('common', 'efx_type')).toBe(18);
  });

  it('shows gains in dB and the balance as the direct to wet mix', () => {
    const store = make(18); // TRIPLE-TAP-DELAY: parameter 9 Low Gain, 11 Effect Balance
    store.set('common', 'efx_parameter_9', 0);
    store.set('common', 'efx_parameter_11', 50);
    const { container } = render(EfxHarness, { props: { store } });
    const meanings = [...container.querySelectorAll('[role=slider]')].map((e) => e.getAttribute('aria-valuetext'));
    expect(meanings).toContain('-15 dB');
    expect(meanings).toContain('D50:50W');
  });
});
