import { beforeAll, describe, expect, it } from 'vitest';
import { readFileSync } from 'node:fs';
import { catalogJson, initSync } from '../wasm/pkg/xppatch.js';
import { parseCatalog, TONE_BLOCKS, type Catalog } from './catalog';
import { PatchStore } from './store.svelte';
import { MemoryTransport } from './transport';
import { toneIsSolo, toneSoloPress } from './toneSolo.svelte';

let catalog: Catalog;
const BANK = new Uint8Array(readFileSync('../tests/golden/xp60-user-bank.syx'));

beforeAll(() => {
  initSync({ module: readFileSync('src/wasm/pkg/xppatch_bg.wasm') });
  catalog = parseCatalog(catalogJson());
});

const switches = (store: PatchStore): number[] => TONE_BLOCKS.map((b) => store.get(b, 'tone_switch'));

function make(): PatchStore {
  const store = new PatchStore(catalog, new MemoryTransport());
  store.loadSyx(BANK, 17, 'bank.syx');
  store.set('tone1', 'tone_switch', 1);
  store.set('tone2', 'tone_switch', 1);
  store.set('tone3', 'tone_switch', 0);
  store.set('tone4', 'tone_switch', 1);
  return store;
}

describe('Tone solo', () => {
  it('isolates one Tone in a single undo step and restores the earlier switches', () => {
    const store = make();
    const before = switches(store);
    const steps = store.items.length;
    toneSoloPress(store, 'tone2');
    expect(switches(store)).toEqual([0, 1, 0, 0]);
    expect(toneIsSolo(store, 'tone2')).toBe(true);
    expect(toneIsSolo(store, 'tone1')).toBe(false);
    expect(store.items.length).toBe(steps + 1);
    expect(store.items.at(-1)?.label).toBe('Solo Tone 2');

    toneSoloPress(store, 'tone4');
    expect(switches(store)).toEqual([0, 0, 0, 1]);
    expect(toneIsSolo(store, 'tone4')).toBe(true);
    expect(store.items.at(-1)?.label).toBe('Solo Tone 4');

    toneSoloPress(store, 'tone4');
    expect(switches(store)).toEqual(before);
    expect(toneIsSolo(store, 'tone4')).toBe(false);
    expect(store.items.at(-1)?.label).toBe('Clear Tone solo');

    store.undo();
    expect(switches(store)).toEqual([0, 0, 0, 1]);
  });

  it('starts a new session after a Tone switch is edited by hand', () => {
    const store = make();
    toneSoloPress(store, 'tone1');
    store.set('tone3', 'tone_switch', 1);
    expect(toneIsSolo(store, 'tone1')).toBe(false);
    toneSoloPress(store, 'tone2');
    expect(switches(store)).toEqual([0, 1, 0, 0]);
    toneSoloPress(store, 'tone2');
    expect(switches(store)).toEqual([1, 0, 1, 0]);
  });

  it('drops the session when another patch is opened, and does not write the old switches', () => {
    const store = make();
    toneSoloPress(store, 'tone1');
    store.loadSyx(BANK, 11, 'bank.syx');
    expect(toneIsSolo(store, 'tone1')).toBe(false);
    const opened = switches(store);
    toneSoloPress(store, 'tone1');
    toneSoloPress(store, 'tone1');
    expect(switches(store)).toEqual(opened);
  });
});
