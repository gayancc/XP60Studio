import { beforeAll, describe, expect, it } from 'vitest';
import { readFileSync } from 'node:fs';
import { catalogJson, initSync } from '../wasm/pkg/xppatch.js';
import { parseCatalog, type Catalog } from './catalog';
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
