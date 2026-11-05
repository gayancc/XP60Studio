import { afterEach, beforeAll, describe, expect, it } from 'vitest';
import { cleanup, render } from '@testing-library/svelte';
import { readFileSync } from 'node:fs';
import { catalogJson, initSync } from '../wasm/pkg/xppatch.js';
import { parseCatalog, type Catalog } from '../model/catalog';
import { PatchStore } from '../model/store.svelte';
import { MemoryTransport } from '../model/transport';
import RoutingHarness from './RoutingHarness.svelte';

let catalog: Catalog;
beforeAll(() => {
  initSync({ module: readFileSync('src/wasm/pkg/xppatch_bg.wasm') });
  catalog = parseCatalog(catalogJson());
});
afterEach(cleanup);

const make = (): PatchStore => {
  const store = new PatchStore(catalog, new MemoryTransport());
  store.newPatch();
  return store;
};
const radios = (c: HTMLElement): string[] => [...c.querySelectorAll('[role=radio]')].map((r) => r.textContent?.trim() ?? '');

describe('the routing panel', () => {
  it('offers only MIX, EFX and DIR for a Tone, never the unlisted OUTPUT-2', () => {
    const { container } = render(RoutingHarness, { props: { store: make() } });
    expect(radios(container).slice(0, 3)).toEqual(['MIX', 'EFX', 'DIR']);
    expect(radios(container)).not.toContain('OUTPUT-2');
  });

  it('still shows an unlisted value a patch already holds, marked as not in the manual', () => {
    const store = make();
    store.set('tone1', 'output_assign', 3);
    const { container } = render(RoutingHarness, { props: { store } });
    expect(radios(container).some((t) => /OUTPUT-2 \(not in manual\)/.test(t))).toBe(true);
  });

  it('dims a send that has no effect and says why in the hint name', () => {
    const store = make();
    store.set('tone1', 'output_assign', 2); // DIR
    store.set('tone1', 'chorus_send_level', 100);
    const { container } = render(RoutingHarness, { props: { store } });
    const inert = container.querySelector('[data-param="tone1:chorus_send_level"]');
    expect(inert?.classList.contains('inert')).toBe(true);
    expect(inert?.querySelector('[role=slider]')?.getAttribute('aria-label')).toMatch(/no effect: Output Assign is DIR/);
  });

  it('draws a path only where sound flows, and dims the first Tone of a combined pair', () => {
    const store = make();
    store.set('common', 'structure_type_1_2', 3);
    const { container } = render(RoutingHarness, { props: { store } });
    expect(container.querySelector('svg[aria-label^="Effects routing"]')).toBeTruthy();
    expect(container.querySelector('.row.ignored')).toBeTruthy();
  });
});
