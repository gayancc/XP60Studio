import { beforeAll, describe, expect, it } from 'vitest';
import { readFileSync } from 'node:fs';
import { catalogJson, initSync } from '../wasm/pkg/xppatch.js';
import { parseCatalog, type Catalog } from './catalog';
import { planRename, summarise } from './nameReplace';
import { PatchStore } from './store.svelte';
import { describeSound } from './textExport';
import { MemoryTransport } from './transport';

let catalog: Catalog;
const BANK = new Uint8Array(readFileSync('../tests/golden/xp60-user-bank.syx'));

beforeAll(() => {
  initSync({ module: readFileSync('src/wasm/pkg/xppatch_bg.wasm') });
  catalog = parseCatalog(catalogJson());
});

describe('copy as text', () => {
  it('names the sound and lists every block with parameters as the user sees them', () => {
    const store = new PatchStore(catalog, new MemoryTransport());
    store.loadSyx(BANK, 0, 'bank.syx');
    const text = describeSound(store);
    const first = text.split('\n')[0] ?? '';
    expect(first).toContain(store.name.trimEnd());
    expect(first).toContain('bank.syx, slot 1');
    for (const heading of ['Patch', 'Tone 1', 'Tone 2', 'Tone 3', 'Tone 4']) expect(text).toMatch(new RegExp(`^${heading}`, 'm'));
    expect(text).toContain(`  Cutoff Frequency: ${store.format('tone1', 'cutoff_frequency')}`);
    expect(text).toContain(`  Tone Switch: ${store.format('tone1', 'tone_switch')}`);
    expect(text.endsWith('\n')).toBe(true);
  });

  it('covers a performance and a rhythm set too, without printing name characters as numbers', () => {
    for (const kind of ['performance', 'rhythm'] as const) {
      const store = new PatchStore(catalog, new MemoryTransport(), 0x10, undefined, kind);
      const text = describeSound(store);
      expect(text).toContain(kind === 'performance' ? 'Part 1' : 'Key');
      expect(text).not.toMatch(/Name \d+:/);
    }
  });
});

describe('find and replace in names', () => {
  const rows = [
    { id: 1, name: 'Brass Hit   ' },
    { id: 2, name: 'BRASS Pad   ' },
    { id: 3, name: 'Strings     ' },
    { id: 4, name: 'Big Brass 1 ' },
  ];

  it('finds in any case unless asked to match it, and keeps the rest of the name as it was', () => {
    const plans = planRename(rows, 'brass', 'Horn');
    expect(plans.map((p) => [p.id, p.to])).toEqual([[1, 'Horn Hit'], [2, 'Horn Pad'], [4, 'Big Horn 1']]);
    expect(planRename(rows, 'Brass', 'Horn', true).map((p) => p.id)).toEqual([1, 4]);
  });

  it('replaces every occurrence in a name', () => {
    expect(planRename([{ id: 1, name: 'aa a' }], 'a', 'b')[0]?.to).toBe('bb b');
  });

  it('skips, and says why, a result that is too long, empty or not plain text', () => {
    const plans = planRename([{ id: 1, name: 'Brass Hit' }, { id: 2, name: 'Brass' }, { id: 3, name: 'X' }], 'Brass', 'Orchestral Brass');
    expect(plans.find((p) => p.id === 1)?.skip).toBe('too long');
    expect(planRename([{ id: 2, name: 'Brass' }], 'Brass', '')[0]?.skip).toBe('empty');
    expect(planRename([{ id: 3, name: 'Hi' }], 'Hi', 'Hé')[0]?.skip).toBe('not plain text');
  });

  it('plans nothing for an empty search or when nothing changes', () => {
    expect(planRename(rows, '', 'x')).toEqual([]);
    expect(planRename(rows, 'Brass', 'Brass', true)).toEqual([]);
    expect(summarise([])).toBe('No shown name would change.');
  });

  it('summarises what will happen', () => {
    const plans = planRename([{ id: 1, name: 'Brass' }, { id: 2, name: 'Brass 2' }, { id: 3, name: 'Big Brass' }], 'Brass', 'Orchestra');
    expect(summarise(plans)).toBe('2 names will change; 1 skipped (too long).');
    expect(summarise(planRename([{ id: 1, name: 'Brass' }], 'Brass', 'Horn'))).toBe('1 name will change.');
  });
});
