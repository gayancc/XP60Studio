import { beforeAll, describe, expect, it, vi } from 'vitest';
import { readFileSync } from 'node:fs';
import { catalogJson, initSync } from '../wasm/pkg/xppatch.js';
import { EXPORT_NOUN, exportFileName, exportPatch, exportPatchBytes } from './exportPatch';
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

  it('exports a Performance and a Rhythm Set as files that open again, and the System settings as messages', async () => {
    const never = { bankToSyx: async () => Promise.reject(new Error('not used for these kinds')) };
    const make = (kind: 'performance' | 'rhythm' | 'system'): PatchStore => new PatchStore(parseCatalog(catalogJson()), new MemoryTransport(), 0x10, undefined, kind);
    const perf = make('performance');
    perf.loadSyx(BANK, 4, 'bank.syx');
    const file = await exportPatchBytes(perf, never, 'syx');
    const back = make('performance');
    back.loadSyx(file, 0, 'export.syx');
    expect(back.error).toBe('');
    expect(back.name).toBe(perf.name);
    expect(await exportPatchBytes(make('rhythm'), never, 'syx')).toBeInstanceOf(Uint8Array);
    const sys = await exportPatchBytes(make('system'), never, 'syx');
    expect(sys[0]).toBe(0xf0);
    expect(isMidiFile(await exportPatchBytes(make('system'), never, 'mid'))).toBe(true);
    expect(EXPORT_NOUN.system).toBe('System settings');
    expect(exportFileName('', 'syx', EXPORT_NOUN.rhythm.replace(' ', '-'))).toBe('rhythm-set.syx');
  });
});
