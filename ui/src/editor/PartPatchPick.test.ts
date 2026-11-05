import { afterEach, beforeAll, describe, expect, it } from 'vitest';
import { cleanup, fireEvent, render } from '@testing-library/svelte';
import { readFileSync } from 'node:fs';
import { catalogJson, initSync } from '../wasm/pkg/xppatch.js';
import { parseCatalog, type Catalog } from '../model/catalog';
import { expansions } from '../model/expansions.svelte';
import { PatchStore } from '../model/store.svelte';
import { MemoryTransport } from '../model/transport';
import { choose, entries, shown } from '../test/dropdown';
import PartHarness from './PartHarness.svelte';

let catalog: Catalog;
beforeAll(() => {
  initSync({ module: readFileSync('src/wasm/pkg/xppatch_bg.wasm') });
  catalog = parseCatalog(catalogJson());
});
afterEach(() => {
  cleanup();
  for (const n of [...expansions.installed]) expansions.set(n, false);
});

const make = (): PatchStore => {
  const store = new PatchStore(catalog, new MemoryTransport(), 0x10, undefined, 'performance');
  store.newPatch();
  return store;
};
const setPatch = (s: PatchStore, type: number, id: number, raw: number): void => {
  s.set('part1', 'patch_group_type', type);
  s.set('part1', 'patch_group_id', id);
  s.set('part1', 'patch_number', raw);
};

describe('the Part patch picker', () => {
  it('offers USER and the four Preset banks, and an expansion board only once it is marked installed', async () => {
    const store = make();
    const { getByLabelText, rerender } = render(PartHarness, { props: { store } });
    const labels = (): Promise<string[]> => entries(getByLabelText('Part 1 patch group'));
    expect(await labels()).toEqual(expect.arrayContaining(['USER', 'PR-A', 'PR-B', 'PR-C', 'PR-D (GM)']));
    expect((await labels()).join('|')).not.toContain('Session');
    expansions.set(9, true);
    await rerender({ store });
    expect((await labels()).join('|')).toContain('Session');
  });

  it('names Patches in a Preset bank from Roland’s list and shows User Patches by number only', async () => {
    const store = make();
    setPatch(store, 0, 3, 0); // PR-A 001
    const { getByLabelText, unmount } = render(PartHarness, { props: { store } });
    expect((await entries(getByLabelText('Part 1 patch')))[0]).toBe('001 64voicePiano');
    unmount();
    setPatch(store, 0, 1, 4); // USER 005
    const view = render(PartHarness, { props: { store } });
    expect((await entries(view.getByLabelText('Part 1 patch')))[4]).toBe('005');
  });

  it('changes group type and ID together in one undo step, and keeps the number inside a 128-patch bank', async () => {
    const store = make();
    expansions.set(9, true);
    setPatch(store, 2, 9, 200); // an expansion board, patch 201
    const { getByLabelText } = render(PartHarness, { props: { store } });
    await choose(getByLabelText('Part 1 patch group'), 'PR-B');
    expect([store.get('part1', 'patch_group_type'), store.get('part1', 'patch_group_id'), store.get('part1', 'patch_number')]).toEqual([0, 4, 127]);
    store.undo();
    expect([store.get('part1', 'patch_group_type'), store.get('part1', 'patch_group_id'), store.get('part1', 'patch_number')]).toEqual([2, 9, 200]);
  });

  it('keeps a Part visible where it already points even if the list does not offer it', () => {
    const store = make();
    setPatch(store, 2, 14, 0); // Asia, not marked installed
    const { getByLabelText } = render(PartHarness, { props: { store } });
    expect(shown(getByLabelText('Part 1 patch group'))).toBe('Asia (not marked installed)');
  });

  it('says "Not set" for the all-zero group a new Performance starts with, because the table has no group 0', () => {
    const store = make();
    expect([store.get('part1', 'patch_group_type'), store.get('part1', 'patch_group_id')]).toEqual([0, 0]);
    const { getByLabelText } = render(PartHarness, { props: { store } });
    expect(shown(getByLabelText('Part 1 patch group'))).toBe('Not set');
  });
});
