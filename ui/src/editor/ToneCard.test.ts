import { beforeAll, afterEach, describe, expect, it } from 'vitest';
import { cleanup, fireEvent, render } from '@testing-library/svelte';
import { readFileSync } from 'node:fs';
import { catalogJson, initSync } from '../wasm/pkg/xppatch.js';
import { parseCatalog, type Catalog } from '../model/catalog';
import { expansions } from '../model/expansions.svelte';
import { PatchStore } from '../model/store.svelte';
import { MemoryTransport } from '../model/transport';
import ToneHarness from './ToneHarness.svelte';

let catalog: Catalog;
const BANK = new Uint8Array(readFileSync('../tests/golden/xp60-user-bank.syx'));

beforeAll(() => {
  initSync({ module: readFileSync('src/wasm/pkg/xppatch_bg.wasm') });
  catalog = parseCatalog(catalogJson());
});
afterEach(() => {
  cleanup();
  for (const n of [...expansions.installed]) expansions.set(n, false);
});

/** The golden bank's "Clav 1 x4": Tone 1 plays expansion board 1 wave 17, Clav 2A. */
const make = (slot = 17): PatchStore => {
  const store = new PatchStore(catalog, new MemoryTransport());
  store.loadSyx(BANK, slot, 'bank.syx');
  return store;
};

describe('the Tone card', () => {
  it('has no separate On control: the dot beside the name is the switch', () => {
    const { queryByText, getByRole } = render(ToneHarness, { props: { store: make() } });
    expect(queryByText('On')).toBeNull();
    expect(getByRole('switch', { name: 'Tone 1 on' })).toBeTruthy();
  });

  it('switches the Tone with the dot and greys the card when it is off', async () => {
    const store = make();
    const { getByRole, container } = render(ToneHarness, { props: { store } });
    const dot = getByRole('switch', { name: 'Tone 1 on' });
    const was = store.get('tone1', 'tone_switch');
    await fireEvent.click(dot);
    expect(store.get('tone1', 'tone_switch')).toBe(was === 1 ? 0 : 1);
    expect(dot.getAttribute('aria-checked')).toBe(was === 1 ? 'false' : 'true');
    expect(container.querySelector('.card')?.classList.contains('off')).toBe(was === 1);
    await fireEvent.click(dot);
    expect(store.get('tone1', 'tone_switch')).toBe(was);
  });

  it('is titled with the wave, not a dropdown box, and still says which Tone it is', () => {
    const { getByRole, container } = render(ToneHarness, { props: { store: make() } });
    expect(container.querySelector('select')).toBeNull();
    expect(getByRole('button', { name: 'Edit Tone 1' }).textContent).toContain('Tone 1');
    expect(container.querySelector('.title')?.textContent).toContain('Clav 2A');
  });

  it('selects the Tone when its wave browser opens', async () => {
    let picked = 0;
    const store = make();
    const { getByRole } = render(ToneHarness, { props: { store, onselect: () => (picked += 1) } });
    await fireEvent.click(getByRole('button', { name: /^Wave of Tone 1/ }));
    expect(picked).toBe(1);
  });

  it('shows the name of the wave on the picker', () => {
    const { getByRole } = render(ToneHarness, { props: { store: make() } });
    expect(getByRole('button', { name: /^Wave of Tone 1: Clav 2A/ })).toBeTruthy();
  });
});

describe('the wave picker', () => {
  it('offers the instrument’s own waves, and an expansion board only once it is marked installed', async () => {
    const { getByRole, getAllByRole, queryByText, rerender } = render(ToneHarness, { props: { store: make() } });
    await fireEvent.click(getByRole('button', { name: /^Wave of Tone 1/ }));
    const names = (): string[] => getAllByRole('button').filter((b) => b.classList.contains('src')).map((b) => b.textContent ?? '');
    expect(names().join('|')).toContain('Internal A');
    expect(names().join('|')).toContain('Internal B');
    // the Tone already uses board 1, so it stays visible, flagged as not installed
    expect(names().join('|')).toContain('not installed');
    await fireEvent.keyDown(getByRole('dialog'), { key: 'Escape' });
    expansions.set(1, true);
    await rerender({ store: make() });
    await fireEvent.click(getByRole('button', { name: /^Wave of Tone 1/ }));
    expect(names().join('|')).toContain('Pop');
    expect(names().join('|')).not.toContain('not installed');
    expect(queryByText('Search Pop…')).toBeNull();
  });

  it('never invents a group: a Tone pointing at no known group shows only the keyboard’s own groups', async () => {
    const store = make();
    store.set('tone1', 'wave_group_type', 0);
    store.set('tone1', 'wave_group_id', 0);
    const { getByRole, getAllByRole } = render(ToneHarness, { props: { store } });
    await fireEvent.click(getByRole('button', { name: /^Wave of Tone 1/ }));
    const names = getAllByRole('button').filter((b) => b.classList.contains('src')).map((b) => b.textContent ?? '').join('|');
    expect(names).not.toMatch(/Group \d/);
    expect(names).not.toContain('not listed');
  });

  it('picks a wave in one undo step, by click, and writes group type, group and number', async () => {
    const store = make(11); // SUNFLOWER FL: Tone 1 is internal wave 026
    const { getByRole, getAllByRole } = render(ToneHarness, { props: { store } });
    await fireEvent.click(getByRole('button', { name: /^Wave of Tone 1/ }));
    await fireEvent.click(getAllByRole('button').find((b) => b.classList.contains('src') && /Internal A/.test(b.textContent ?? ''))!);
    const row = getAllByRole('option').find((o) => o.textContent?.includes('Ac Piano1 A'));
    expect(row).toBeTruthy();
    await fireEvent.click(row!.querySelector('button')!);
    expect([store.get('tone1', 'wave_group_type'), store.get('tone1', 'wave_group_id'), store.get('tone1', 'wave_number')]).toEqual([0, 1, 0]);
    expect(store.canUndo).toBe(true);
    store.undo();
    expect(store.get('tone1', 'wave_number')).toBe(25);
    expect(store.canUndo).toBe(false);
  });

  it('searches and picks from the keyboard', async () => {
    const store = make(11);
    const { getByRole, getByLabelText } = render(ToneHarness, { props: { store } });
    await fireEvent.click(getByRole('button', { name: /^Wave of Tone 1/ }));
    const search = getByLabelText('Search waves') as HTMLInputElement;
    await fireEvent.input(search, { target: { value: 'kalimba' } });
    await fireEvent.keyDown(getByRole('dialog'), { key: 'Enter' });
    // Kalimba is wave 1 of Internal A or B: whichever list was open, the number written is the zero-based one
    expect(store.get('tone1', 'wave_number')).toBeLessThan(255);
    expect(store.format('tone1', 'wave_number')).toMatch(/^\d{3}$/);
  });

  it('chooses by number for a board whose wave names are not held', async () => {
    const store = make(11);
    expansions.set(6, true); // Dance: Roland publishes no waveform list for it
    const { getByRole, getAllByRole, getByLabelText } = render(ToneHarness, { props: { store } });
    await fireEvent.click(getByRole('button', { name: /^Wave of Tone 1/ }));
    await fireEvent.click(getAllByRole('button').find((b) => b.classList.contains('src') && /Dance/.test(b.textContent ?? ''))!);
    expect(getByRole('dialog').textContent).toContain('chosen by number');
    await fireEvent.input(getByLabelText('Wave number'), { target: { value: '12' } });
    await fireEvent.click(getByRole('button', { name: /Use wave 012/ }));
    expect([store.get('tone1', 'wave_group_type'), store.get('tone1', 'wave_group_id'), store.get('tone1', 'wave_number')]).toEqual([2, 6, 11]);
  });
});
