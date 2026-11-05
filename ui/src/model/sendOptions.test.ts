import { afterEach, beforeAll, beforeEach, describe, expect, it, vi } from 'vitest';
import { readFileSync } from 'node:fs';
import { catalogJson, initSync } from '../wasm/pkg/xppatch.js';
import { parseCatalog, type Catalog } from './catalog';
import { clampChunk, cutFrames, MAX_CHUNK, MIN_CHUNK, sendOptions, setChunk } from './sendOptions.svelte';
import { PatchStore } from './store.svelte';
import { MemoryTransport } from './transport';

let catalog: Catalog;
const BANK = new Uint8Array(readFileSync('../tests/golden/xp60-user-bank.syx'));
/** A DT1 frame is F0 41 dd 6A 12, four address bytes, the data, a checksum and F7: 11 bytes around the data. */
const dataLen = (frame: Uint8Array): number => frame.length - 11;

beforeAll(() => {
  initSync({ module: readFileSync('src/wasm/pkg/xppatch_bg.wasm') });
  catalog = parseCatalog(catalogJson());
});

beforeEach(() => {
  vi.useFakeTimers();
  globalThis.localStorage?.clear();
  setChunk(MAX_CHUNK);
});
afterEach(() => vi.useRealTimers());

function sendWhole(): Uint8Array[] {
  const transport = new MemoryTransport();
  const store = new PatchStore(catalog, transport, 0x10, { now: () => Date.now() });
  store.loadSyx(BANK, 0, 'bank.syx');
  store.sendPatch();
  vi.advanceTimersByTime(60_000);
  return transport.frames as Uint8Array[];
}

describe('chunk size of whole-patch sends', () => {
  it('accepts whole numbers in range and otherwise falls back to the instrument maximum', () => {
    expect(clampChunk(MIN_CHUNK)).toBe(MIN_CHUNK);
    expect(clampChunk(64)).toBe(64);
    expect(clampChunk('48')).toBe(48);
    for (const bad of [0, 15, 129, 1.5, NaN, null, undefined, 'x', -4]) expect(clampChunk(bad)).toBe(MAX_CHUNK);
  });

  it('the default sends the same frames as before: none above 128 data bytes', () => {
    const frames = sendWhole();
    expect(frames.length).toBeGreaterThan(4);
    expect(Math.max(...frames.map(dataLen))).toBeLessThanOrEqual(128);
    expect(Math.max(...frames.map(dataLen))).toBeGreaterThan(32);
  });

  it('a smaller chunk sends more, smaller frames carrying the same number of data bytes', () => {
    const whole = sendWhole();
    setChunk(32);
    const cut = sendWhole();
    expect(cut.length).toBeGreaterThan(whole.length);
    expect(Math.max(...cut.map(dataLen))).toBeLessThanOrEqual(32);
    const total = (fs: Uint8Array[]): number => fs.reduce((n, f) => n + dataLen(f), 0);
    expect(total(cut)).toBe(total(whole));
  });

  it('the setting is remembered and is held to its range', () => {
    setChunk(48);
    expect(sendOptions.chunk).toBe(48);
    expect(globalThis.localStorage.getItem('xppatch.chunk.v1')).toBe('48');
    setChunk(5000);
    expect(sendOptions.chunk).toBe(MAX_CHUNK);
  });

  it('cutting at the maximum returns the very same bytes', () => {
    const frames = new Uint8Array([0xf0, 0x41, 0x10, 0x6a, 0x12, 0, 0, 0, 0, 1, 0x7f, 0xf7]);
    expect(cutFrames(frames, MAX_CHUNK)).toBe(frames);
  });
});
