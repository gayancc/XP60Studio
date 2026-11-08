import { afterEach, beforeAll, describe, expect, it } from 'vitest';
import { cleanup, fireEvent, render } from '@testing-library/svelte';
import { readFileSync } from 'node:fs';
import { catalogJson, initSync } from '../wasm/pkg/xppatch.js';
import { parseCatalog, type Catalog } from '../model/catalog';
import { PatchStore } from '../model/store.svelte';
import { MemoryTransport } from '../model/transport';
import AllTonesHarness from './AllTonesHarness.svelte';

let catalog: Catalog;
const BANK = new Uint8Array(readFileSync('../tests/golden/xp60-user-bank.syx'));
beforeAll(() => {
  initSync({ module: readFileSync('src/wasm/pkg/xppatch_bg.wasm') });
  catalog = parseCatalog(catalogJson());
});
afterEach(cleanup);

const make = (slot = 17): PatchStore => {
  const store = new PatchStore(catalog, new MemoryTransport());
  store.loadSyx(BANK, slot, 'bank.syx');
  return store;
};

describe('the four-Tone view of a sound section', () => {
  it('shows the section once for every Tone, each headed with its name and wave', () => {
    const { container } = render(AllTonesHarness, { props: { store: make(), part: 'pitch' } });
    const cols = container.querySelectorAll('.col');
    expect(cols).toHaveLength(4);
    expect(cols[0]?.textContent).toContain('Tone 1');
    expect(cols[0]?.textContent).toContain('Clav 2A'); // "Clav 1 x4": Tone 1 plays expansion wave 17
    expect(cols[3]?.textContent).toContain('Clav 4A');
    // the same controls in every column
    const counts = [...cols].map((c) => c.querySelectorAll('[role=slider]').length);
    expect(new Set(counts).size).toBe(1);
    expect(counts[0]).toBeGreaterThan(5);
  });

  it('dims a Tone that is off and leaves the others alone', () => {
    const store = make();
    store.set('tone2', 'tone_switch', 0);
    store.set('tone1', 'tone_switch', 1);
    const { container } = render(AllTonesHarness, { props: { store } });
    const cols = container.querySelectorAll('.col');
    expect(cols[1]?.classList.contains('off')).toBe(true);
    expect(cols[0]?.classList.contains('off')).toBe(false);
  });

  it('edits that Tone only: a change in one column does not touch the others', async () => {
    const store = make();
    const before = [1, 2, 3, 4].map((n) => store.get(`tone${n}`, 'random_pan_depth'));
    const { container } = render(AllTonesHarness, { props: { store, part: 'tva' } });
    const col2 = container.querySelectorAll('.col')[1] as HTMLElement;
    const knob = [...col2.querySelectorAll('[role=slider]')].find((e) => e.getAttribute('aria-label') === 'Random Pan Depth') as HTMLElement;
    expect(knob).toBeTruthy();
    await fireEvent.keyDown(knob, { key: 'ArrowUp' });
    const after = [1, 2, 3, 4].map((n) => store.get(`tone${n}`, 'random_pan_depth'));
    expect(after[1]).not.toBe(before[1]);
    expect([after[0], after[2], after[3]]).toEqual([before[0], before[2], before[3]]);
  });

  it('selects a Tone when its heading is clicked', async () => {
    let picked = -1;
    const { container } = render(AllTonesHarness, { props: { store: make(), onselect: (i: number) => (picked = i) } });
    await fireEvent.click(container.querySelectorAll('.col .head')[2] as HTMLElement);
    expect(picked).toBe(2);
  });

  it('selects a Tone when a control in its column is pressed', async () => {
    let picked = -1;
    const { container } = render(AllTonesHarness, { props: { store: make(), onselect: (i: number) => (picked = i) } });
    const col3 = container.querySelectorAll('.col')[2] as HTMLElement;
    await fireEvent.pointerDown(col3.querySelector('.body') as HTMLElement);
    expect(picked).toBe(2);
  });

  it('marks the selected Tone column and keeps the others secondary', () => {
    const { container } = render(AllTonesHarness, { props: { store: make(), selected: 1 } });
    const cols = container.querySelectorAll('.col');
    expect(cols[1]?.classList.contains('sel')).toBe(true);
    expect(cols[0]?.classList.contains('sel')).toBe(false);
    expect(cols[1]?.getAttribute('data-tone-focus')).toBe('1');
  });

  it('shows the merged sections: wave and pitch together, filter and amp together, LFOs with the controller routing', () => {
    const text = (part: 'wavepitch' | 'filteramp' | 'lfomod'): string => {
      const { container } = render(AllTonesHarness, { props: { store: make(), part } });
      const t = (container.querySelector('.col') as HTMLElement).textContent ?? '';
      cleanup();
      return t;
    };
    const wp = text('wavepitch');
    for (const title of ['Wave', 'Pitch']) expect(wp, title).toContain(title);
    const fa = text('filteramp');
    for (const title of ['TVF (filter)', 'TVA (level and pan)']) expect(fa, title).toContain(title);
    const lm = text('lfomod');
    for (const title of ['LFO 1', 'LFO 2', 'Controllers']) expect(lm, title).toContain(title);
  });
});
