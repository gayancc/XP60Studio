import { beforeAll, describe, expect, it, vi } from 'vitest';
import { readFileSync } from 'node:fs';
import { initSync } from '../wasm/pkg/xppatch.js';
import type { Backend } from './backend';
import { LibraryService } from './library.svelte';
import { toMidiFile } from './midiFile';

const BANK = new Uint8Array(readFileSync('../tests/golden/xp60-user-bank.syx'));

beforeAll(() => {
  initSync({ module: readFileSync('src/wasm/pkg/xppatch_bg.wasm') });
});

function backend(overrides: Partial<Backend> = {}): Backend {
  return {
    libraryImport: vi.fn(async () => ({ added: 128, duplicates: 0 })),
    libraryList: vi.fn(async () => []),
    libraryCount: vi.fn(async () => 0),
    libraryTags: vi.fn(async () => []),
    ...overrides,
  } as unknown as Backend;
}

describe('previewing a file before it is imported', () => {
  it('shows what a .syx holds and imports nothing yet', () => {
    const b = backend();
    const lib = new LibraryService(b);
    expect(lib.stage('bank.syx', BANK)).toBe(true);
    expect(lib.staged?.names).toHaveLength(128);
    expect(lib.staged?.name).toBe('bank.syx');
    expect(b.libraryImport).not.toHaveBeenCalled();
  });

  it('shows a .mid the same way, and imports the SysEx inside it', async () => {
    const b = backend();
    const lib = new LibraryService(b);
    lib.stage('bank.mid', toMidiFile(BANK));
    expect(lib.staged?.names).toHaveLength(128);
    await lib.importStaged();
    const sent = (b.libraryImport as ReturnType<typeof vi.fn>).mock.calls[0]?.[0] as Uint8Array;
    expect(sent).toEqual(BANK);
  });

  it('imports once confirmed, reports the counts and clears the preview', async () => {
    const b = backend();
    const lib = new LibraryService(b);
    lib.stage('bank.syx', BANK);
    const result = await lib.importStaged();
    expect(result).toEqual({ added: 128, duplicates: 0 });
    expect(lib.staged).toBeNull();
    expect(lib.notice).toContain('Imported 128');
  });

  it('keeps the preview when the import fails, so it can be retried or cancelled', async () => {
    const b = backend({ libraryImport: vi.fn(async () => Promise.reject('disk full')) });
    const lib = new LibraryService(b);
    lib.stage('bank.syx', BANK);
    expect(await lib.importStaged()).toBeNull();
    expect(lib.staged).not.toBeNull();
    expect(lib.error).toContain('disk full');
    lib.cancelStaged();
    expect(lib.staged).toBeNull();
  });

  it('says what is wrong with a file that holds no patches and shows no preview', () => {
    const lib = new LibraryService(backend());
    expect(lib.stage('junk.bin', new Uint8Array([1, 2, 3]))).toBe(false);
    expect(lib.staged).toBeNull();
    expect(lib.error).not.toBe('');
  });

  it('importing with nothing staged does nothing', async () => {
    const b = backend();
    const lib = new LibraryService(b);
    expect(await lib.importStaged()).toBeNull();
    expect(b.libraryImport).not.toHaveBeenCalled();
  });
});
