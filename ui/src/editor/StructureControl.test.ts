import { afterEach, beforeAll, describe, expect, it } from 'vitest';
import { cleanup, fireEvent, render } from '@testing-library/svelte';
import { readFileSync } from 'node:fs';
import { catalogJson, initSync } from '../wasm/pkg/xppatch.js';
import { parseCatalog, type Catalog } from '../model/catalog';
import { PatchStore } from '../model/store.svelte';
import { MemoryTransport } from '../model/transport';
import StructureHarness from './StructureHarness.svelte';

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

describe('the structure control', () => {
  it('shows which structure type the Tones use, as a diagram and a number', () => {
    const store = make();
    store.set('common', 'structure_type_1_2', 4); // type 5
    const { getByRole, container } = render(StructureHarness, { props: { store } });
    expect(getByRole('button', { name: /structure type 5/ })).toBeTruthy();
    expect(container.querySelector('.num')?.textContent).toBe('TYPE 5');
    expect(container.querySelector('svg')).not.toBeNull();
  });

  it('opens a chooser with all ten diagrams, the current one marked', async () => {
    const store = make();
    const { getByRole, getAllByRole } = render(StructureHarness, { props: { store } });
    await fireEvent.click(getByRole('button', { name: /structure type 1/ }));
    const cells = getAllByRole('button').filter((b) => b.classList.contains('cell'));
    expect(cells).toHaveLength(10);
    expect(cells[0]?.getAttribute('aria-pressed')).toBe('true');
    expect(cells[1]?.getAttribute('aria-pressed')).toBe('false');
  });

  it('writes the chosen type (stored from 0) in one undo step and closes', async () => {
    const store = make();
    const { getByRole, getAllByRole, queryByRole } = render(StructureHarness, { props: { store } });
    await fireEvent.click(getByRole('button', { name: /structure type 1/ }));
    const cell7 = getAllByRole('button').find((b) => b.getAttribute('data-type') === '7')!;
    await fireEvent.click(cell7);
    expect(store.get('common', 'structure_type_1_2')).toBe(6);
    expect(queryByRole('dialog')).toBeNull();
    expect(store.canUndo).toBe(true);
    store.undo();
    expect(store.get('common', 'structure_type_1_2')).toBe(0);
    expect(store.canUndo).toBe(false);
  });

  it('closes on Escape without changing anything', async () => {
    const store = make();
    const { getByRole, queryByRole } = render(StructureHarness, { props: { store } });
    await fireEvent.click(getByRole('button', { name: /structure type 1/ }));
    await fireEvent.keyDown(getByRole('dialog'), { key: 'Escape' });
    expect(queryByRole('dialog')).toBeNull();
    expect(store.get('common', 'structure_type_1_2')).toBe(0);
  });

  it('keeps the booster beside the diagram', () => {
    const { container } = render(StructureHarness, { props: { store: make() } });
    expect(container.querySelector('[data-param="common:booster_1_2"]')).not.toBeNull();
  });

  it('dims the booster unless the type is 3 or 4, as the manual says it only applies there', async () => {
    const store = make();
    const { container, rerender } = render(StructureHarness, { props: { store } });
    expect(container.querySelector('.boost.unused')).not.toBeNull(); // type 1
    store.set('common', 'structure_type_1_2', 2); // type 3
    await rerender({ store });
    expect(container.querySelector('.boost.unused')).toBeNull();
    store.set('common', 'structure_type_1_2', 3); // type 4
    await rerender({ store });
    expect(container.querySelector('.boost.unused')).toBeNull();
    store.set('common', 'structure_type_1_2', 4); // type 5
    await rerender({ store });
    expect(container.querySelector('.boost.unused')).not.toBeNull();
  });

  it('says so when one Tone of the pair is off in a joined structure, because the other then plays as type 1', async () => {
    const store = make();
    store.set('common', 'structure_type_1_2', 1); // type 2
    store.set('tone1', 'tone_switch', 1);
    store.set('tone2', 'tone_switch', 0);
    const { container, rerender } = render(StructureHarness, { props: { store } });
    expect(container.querySelector('.note')?.textContent).toContain('plays as type 1');
    store.set('tone2', 'tone_switch', 1);
    await rerender({ store });
    expect(container.querySelector('.note')).toBeNull();
    store.set('tone2', 'tone_switch', 0);
    store.set('common', 'structure_type_1_2', 0); // type 1 itself needs no note
    await rerender({ store });
    expect(container.querySelector('.note')).toBeNull();
  });
});
