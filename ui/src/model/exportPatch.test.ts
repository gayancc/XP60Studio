import { beforeAll, describe, expect, it, vi } from 'vitest';
import { readFileSync } from 'node:fs';
import { catalogJson, initSync } from '../wasm/pkg/xppatch.js';
import { exportFileName, exportPatch, exportPatchBytes } from './exportPatch';
import { parseCatalog } from './catalog';
import { isMidiFile } from './midiFile';
import { PatchStore } from './store.svelte';
import { MemoryTransport } from './transport';

const BANK = new Uint8Array(readFileSync('../tests/golden/xp60-user-bank.syx'));
const fake = { bankToSyx: vi.fn(async (patches: Array<[number, Uint8Array]>) => new Uint8Array([0xf0, patches.length, patches[0]?.[0] ?? 9, 0xf7])) };

beforeAll(() => {
  initSync({ module: readFileSync('src/wasm/pkg/xppatch_bg.wasm') });
});

const open = (): PatchStore => {
  const s = new PatchStore(parseCatalog(catalogJson()), new MemoryTransport());
  s.loadSyx(BANK, 3, 'bank.syx');
  return s;
};

describe('exporting the open patch', () => {
  it('names the file after the patch, keeping only safe characters', () => {
    expect(exportFileName('ALL RIGHT TA  ', 'syx')).toBe('ALL RIGHT TA.syx');
    expect(exportFileName('a/b:c*?', 'mid')).toBe('abc.mid');
    expect(exportFileName('   ', 'syx')).toBe('patch.syx');
  });

  it('asks for a one-patch bank in slot 1 holding the open patch', async () => {
    const bytes = await exportPatchBytes(open(), fake, 'syx');
    expect(Array.from(bytes)).toEqual([0xf0, 1, 0, 0xf7]);
    expect(fake.bankToSyx.mock.calls[0]?.[0][0]?.[1].length).toBeGreaterThan(100);
  });

  it('reports a failure instead of throwing', async () => {
    const bad = { bankToSyx: async () => Promise.reject(new Error('no device')) };
    expect(await exportPatch(open(), bad, 'syx')).toBe('Export failed: no device');
  });

  it('can wrap a real bank as a .mid', async () => {
    const real = { bankToSyx: async () => BANK };
    expect(isMidiFile(await exportPatchBytes(open(), real, 'mid'))).toBe(true);
  });
});
