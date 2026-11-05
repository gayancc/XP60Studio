import { beforeAll, describe, expect, it, vi } from 'vitest';
import { readFileSync } from 'node:fs';
import { catalogJson, initSync } from '../wasm/pkg/xppatch.js';
import { parseCatalog, PART_BLOCKS, RHYTHM_NOTE_BLOCKS, type Catalog } from './catalog';
import { changedParams } from './frames';
import { blockLayout, blockNames, newDoc, paramAddressOf, snapshotLength } from './kinds';
import { blockTitle, contextOf, midiNoteName, rhythmKey } from './labels';
import {
  RHYTHM_ENVELOPES,
  envelopeParamIds,
  noteSectionOf,
  partSectionOf,
  performanceCommonSectionOf,
  systemSectionOf,
} from './layout';
import { PatchStore } from './store.svelte';
import { MemoryTransport } from './transport';

let catalog: Catalog;

beforeAll(() => {
  initSync({ module: readFileSync('src/wasm/pkg/xppatch_bg.wasm') });
  catalog = parseCatalog(catalogJson());
});

describe('layout rules for performances and rhythm sets', () => {
  it('every performance common parameter belongs to a section', () => {
    const orphans = catalog.performanceCommon.params.filter((p) => !performanceCommonSectionOf(p.id));
    expect(orphans.map((p) => p.id)).toEqual([]);
  });
  it('every performance part parameter belongs to a section', () => {
    const orphans = catalog.performancePart.params.filter((p) => !partSectionOf(p.id));
    expect(orphans.map((p) => p.id)).toEqual([]);
  });
  it('every rhythm note parameter belongs to a section', () => {
    const orphans = catalog.rhythmNote.params.filter((p) => !noteSectionOf(p.id));
    expect(orphans.map((p) => p.id)).toEqual([]);
  });
  it('rhythm envelope parameters exist and sit in their graphs section', () => {
    for (const spec of RHYTHM_ENVELOPES) {
      for (const id of envelopeParamIds(spec)) {
        expect(catalog.rhythmNote.params.some((p) => p.id === id), id).toBe(true);
        expect(noteSectionOf(id), id).toBe(spec.key);
      }
    }
  });
});

describe('document kinds', () => {
  it('lists the blocks of each kind and lays them out end to end', () => {
    expect(blockNames('patch')).toHaveLength(5);
    expect(blockNames('performance')).toHaveLength(17);
    expect(blockNames('rhythm')).toHaveLength(65);
    expect(snapshotLength(catalog, 'patch')).toBe(73 + 4 * 129);
    expect(snapshotLength(catalog, 'performance')).toBe(66 + 16 * 25);
    expect(snapshotLength(catalog, 'rhythm')).toBe(12 + 64 * 58);
    const layout = blockLayout(catalog, 'performance');
    expect(layout[1]?.start).toBe(66);
    expect(layout[2]?.start).toBe(66 + 25);
  });

  it('the codec snapshot has exactly the layout length', () => {
    for (const kind of ['patch', 'performance', 'rhythm'] as const) {
      expect(newDoc(kind).toBlocks().length).toBe(snapshotLength(catalog, kind));
    }
  });

  it('addresses parameters inside each temporary area', () => {
    expect(paramAddressOf('performance', 'part3', 'part_level')).toBe((0x12 << 7) | 6);
    expect(paramAddressOf('rhythm', 'note44', 'tone_level')).toBe((0x2c << 7) | 41);
    expect(() => paramAddressOf('rhythm', 'note200', 'tone_level')).toThrow();
  });

  it('rhythm keys are numbered 35 to 98 and named like notes', () => {
    expect(RHYTHM_NOTE_BLOCKS[0]).toBe('note35');
    expect(RHYTHM_NOTE_BLOCKS[63]).toBe('note98');
    expect(PART_BLOCKS).toHaveLength(16);
    expect(rhythmKey('note44')).toBe(44);
    expect(rhythmKey('note34')).toBeNull();
    expect(rhythmKey('tone1')).toBeNull();
    expect(midiNoteName(44)).toBe('G#2');
    expect(midiNoteName(60)).toBe('C4');
  });

  it('titles and breadcrumbs name the thing being edited', () => {
    expect(blockTitle('part5', 'performance')).toBe('Part 5');
    expect(blockTitle('note44', 'rhythm')).toBe('Key G#2 (44)');
    expect(blockTitle('common', 'performance')).toBe('Performance');
    expect(contextOf('part2', 'part_level', 'performance')).toBe('Part 2 > Mix');
    expect(contextOf('note40', 'cutoff_frequency', 'rhythm')).toBe('Key E2 (40) > TVF');
    expect(contextOf('common', 'efx_type', 'performance')).toBe('Performance > EFX');
    expect(contextOf('tone2', 'resonance')).toBe('Tone 2 > TVF');
  });

  it('finds changed parameters in a rhythm snapshot', () => {
    const doc = newDoc('rhythm');
    const before = doc.toBlocks();
    doc.set('note50', 'cutoff_frequency', 33);
    doc.set('common', 'rhythm_name_1', 66);
    const changed = changedParams(catalog, 'rhythm', before, doc.toBlocks());
    expect(changed).toEqual([
      { block: 'common', id: 'rhythm_name_1' },
      { block: 'note50', id: 'cutoff_frequency' },
    ]);
  });
});

describe('stores for performances and rhythm sets', () => {
  const make = (kind: 'performance' | 'rhythm') => {
    vi.useFakeTimers();
    const transport = new MemoryTransport();
    const store = new PatchStore(catalog, transport, 0x10, { now: () => Date.now() }, kind);
    return { store, transport };
  };

  it('edits a part, groups undo, and sends the whole performance first, then single parameters', () => {
    const { store, transport } = make('performance');
    store.set('part3', 'part_level', 77);
    vi.advanceTimersByTime(2000);
    expect(store.get('part3', 'part_level')).toBe(77);
    expect(store.items.map((i) => i.label)).toEqual(['Part 3 · Part Level']);
    expect(transport.frames.length).toBeGreaterThanOrEqual(17);
    expect(store.sync).toBe('sent');
    transport.frames.length = 0;
    store.set('part3', 'part_level', 78);
    vi.advanceTimersByTime(100);
    expect(transport.frames).toHaveLength(1);
    const f = transport.frames[0] as Uint8Array;
    expect([...f.slice(5, 9)]).toEqual([0x01, 0x00, 0x12, 0x06]);
    store.undo();
    expect(store.get('part3', 'part_level')).toBe(77);
    vi.useRealTimers();
  });

  it('edits a rhythm note and labels it by key', () => {
    const { store, transport } = make('rhythm');
    store.set('note44', 'tone_level', 90);
    vi.advanceTimersByTime(5000);
    expect(store.items[0]?.label).toBe('Key G#2 (44) · Tone Level');
    // All 64 key blocks go out; the set's Common block is held back until its address is confirmed (ledger H-045).
    expect(transport.frames.length).toBeGreaterThanOrEqual(64);
    expect(transport.frames.length).toBeLessThan(66);
    expect(store.format('note44', 'tone_switch')).toBe('OFF');
    vi.useRealTimers();
  });

  it('a syx bank cannot be opened into a performance', () => {
    const { store } = make('performance');
    store.loadSyx(new Uint8Array(10), 0, 'x.syx');
    expect(store.error).not.toBe('');
    vi.useRealTimers();
  });
});

describe('the system document', () => {
  it('every system parameter belongs to a section', () => {
    const orphans = catalog.systemCommon.params.filter((p) => !systemSectionOf(p.id));
    expect(orphans.map((p) => p.id)).toEqual([]);
  });

  it('has common plus seventeen scale tunes, laid out end to end', () => {
    expect(blockNames('system')).toHaveLength(18);
    expect(snapshotLength(catalog, 'system')).toBe(96 + 17 * 12);
    expect(newDoc('system').toBlocks().length).toBe(snapshotLength(catalog, 'system'));
    expect(blockTitle('tune17', 'system')).toBe('Scale tune, Patch mode');
    expect(blockTitle('tune3', 'system')).toBe('Scale tune, Part 3');
    expect(contextOf('common', 'master_tune', 'system')).toBe('System > Sound and mode');
  });

  it('is edited locally and never sent live', () => {
    vi.useFakeTimers();
    const transport = new MemoryTransport();
    const store = new PatchStore(catalog, transport, 0x10, { now: () => Date.now() }, 'system');
    expect(store.live).toBe(false);
    store.set('common', 'master_tune', 70);
    store.set('tune2', 'scale_tune_for_c_0000', 90);
    vi.advanceTimersByTime(5000);
    expect(transport.frames).toHaveLength(0);
    expect(store.sync).toBe('edited');
    store.setLive(true);
    expect(store.live).toBe(false);
    store.sendPatch();
    expect(store.error).toContain('save ceremony');
    expect(store.get('tune2', 'scale_tune_for_c_0000')).toBe(90);
    store.undo();
    expect(store.get('tune2', 'scale_tune_for_c_0000')).toBe(0);
    vi.useRealTimers();
  });
});
