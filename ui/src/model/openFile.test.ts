import { beforeAll, describe, expect, it } from 'vitest';
import { readFileSync } from 'node:fs';
import { catalogJson, initSync } from '../wasm/pkg/xppatch.js';
import { parseCatalog, type Catalog } from './catalog';
import { toMidiFile } from './midiFile';
import { carriesFiles, dropMeaning, MAX_FILE_BYTES, openPatchFile, previewFile } from './openFile';
import { PatchStore } from './store.svelte';
import { MemoryTransport } from './transport';
import { ui } from './ui.svelte';

let catalog: Catalog;
const BANK = new Uint8Array(readFileSync('../tests/golden/xp60-user-bank.syx'));

beforeAll(() => {
  initSync({ module: readFileSync('src/wasm/pkg/xppatch_bg.wasm') });
  catalog = parseCatalog(catalogJson());
});

const make = (): PatchStore => new PatchStore(catalog, new MemoryTransport());

describe('opening a picked or dropped file', () => {
  it('opens the first patch of a .syx bank and remembers the bank for the slot list', () => {
    const store = make();
    openPatchFile(store, BANK, 'bank.syx');
    expect(store.error).toBe('');
    expect(ui.bank?.names).toHaveLength(128);
    expect(ui.bank?.file).toBe('bank.syx');
    expect(store.origin).toContain('bank.syx, slot 1');
  });

  it('opens a .mid the same way', () => {
    const store = make();
    openPatchFile(store, toMidiFile(BANK), 'bank.mid');
    expect(store.error).toBe('');
    expect(ui.bank?.names).toHaveLength(128);
  });

  it('says what is wrong with a file that is not a bank, and forgets the previous bank', () => {
    const store = make();
    openPatchFile(store, BANK, 'bank.syx');
    openPatchFile(store, new Uint8Array([1, 2, 3, 4, 5]), 'junk.bin');
    expect(store.error).not.toBe('');
    expect(ui.bank).toBeNull();
  });

  it('refuses a huge file without reading it as a bank', () => {
    const store = make();
    openPatchFile(store, new Uint8Array(MAX_FILE_BYTES + 1), 'big.syx');
    expect(store.error).toContain('larger than');
  });

  it('previews the patch names of a file before anything is loaded', () => {
    const p = previewFile(BANK);
    expect(p.names).toHaveLength(128);
    expect(previewFile(toMidiFile(BANK)).names).toEqual(p.names);
    expect(() => previewFile(new Uint8Array([9, 9]))).toThrow();
  });

  it('knows where a drop means something and when a drag carries files', () => {
    expect(dropMeaning('patch')).toBeNull();
    expect(dropMeaning('library')).toBeNull();
    expect(dropMeaning('bank')).toBeNull();
    expect(dropMeaning('system')).toContain('Patch, Library or Bank');
    expect(carriesFiles(['Files'])).toBe(true);
    expect(carriesFiles(['text/xp-slot'])).toBe(false);
    expect(carriesFiles(undefined)).toBe(false);
  });
});

describe('opening a whole-memory file in the Performance and Rhythm Set workspaces', () => {
  const memory = new Uint8Array(readFileSync('../tests/golden/xp60-user-bank.syx'));

  it('lists the 32 Performances and opens the first', () => {
    const store = new PatchStore(catalog, new MemoryTransport(), 0x10, undefined, 'performance');
    openPatchFile(store, memory, 'memory.syx');
    expect(store.error).toBe('');
    expect(ui.bank?.kind).toBe('performance');
    expect(ui.bank?.names).toHaveLength(32);
    expect(store.origin).toContain('memory.syx, slot 1');
    expect(store.name.trim()).toBe(ui.bank?.names[0]?.[1].trim());
  });

  it('lists the 2 Rhythm Sets', () => {
    const store = new PatchStore(catalog, new MemoryTransport(), 0x10, undefined, 'rhythm');
    openPatchFile(store, memory, 'memory.syx');
    expect(ui.bank?.names).toHaveLength(2);
  });

  it('says so when the file holds none of what the workspace edits', () => {
    const store = new PatchStore(catalog, new MemoryTransport(), 0x10, undefined, 'system');
    openPatchFile(store, memory, 'memory.syx');
    expect(store.error).not.toBe('');
  });
});
