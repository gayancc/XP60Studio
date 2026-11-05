import { beforeAll, describe, expect, it } from 'vitest';
import { readFileSync } from 'node:fs';
import { initSync } from '../wasm/pkg/xppatch.js';
import { asSyx, isMidiFile, toMidiFile } from './midiFile';
import { PatchStore } from './store.svelte';

const BANK = new Uint8Array(readFileSync('../tests/golden/xp60-user-bank.syx'));

beforeAll(() => {
  initSync({ module: readFileSync('src/wasm/pkg/xppatch_bg.wasm') });
});

describe('.mid files', () => {
  it('recognises a Standard MIDI File by its header, and a .syx by its absence', () => {
    expect(isMidiFile(toMidiFile(BANK))).toBe(true);
    expect(isMidiFile(BANK)).toBe(false);
    expect(isMidiFile(new Uint8Array([0x4d, 0x54]))).toBe(false);
  });

  it('turns the golden bank into a .mid and back without changing a byte', () => {
    expect(asSyx(toMidiFile(BANK))).toEqual(BANK);
  });

  it('leaves a .syx alone', () => {
    expect(asSyx(BANK)).toBe(BANK);
  });

  it('lists the same 128 patch names from the .mid as from the .syx', () => {
    const fromMid = PatchStore.listSyx(asSyx(toMidiFile(BANK)));
    expect(fromMid).toHaveLength(128);
    expect(fromMid).toEqual(PatchStore.listSyx(BANK));
  });

  it('reports a clear error for a .mid with no SysEx', () => {
    const notes = new Uint8Array([0x4d, 0x54, 0x68, 0x64, 0, 0, 0, 6, 0, 0, 0, 1, 1, 0xe0, 0x4d, 0x54, 0x72, 0x6b, 0, 0, 0, 4, 0, 0xff, 0x2f, 0]);
    expect(() => asSyx(notes)).toThrow(/no SysEx/);
  });
});
