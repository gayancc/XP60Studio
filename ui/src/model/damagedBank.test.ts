import { beforeAll, describe, expect, it } from 'vitest';
import { readFileSync } from 'node:fs';
import { initSync } from '../wasm/pkg/xppatch.js';
import { damageNotice, previewFile } from './openFile';
import { PatchStore } from './store.svelte';

beforeAll(() => {
  initSync({ module: readFileSync('src/wasm/pkg/xppatch_bg.wasm') });
});

const BANK = new Uint8Array(readFileSync('../tests/golden/xp60-user-bank.syx'));

/** The golden bank with one patch-common message's manufacturer byte and one Tone message's command byte damaged, as a transfer error can leave them. */
function damaged(): Uint8Array {
  const b = BANK.slice();
  const find = (slot: number, block: number): number => {
    for (let i = 0; i < b.length - 8; i++) if (b[i] === 0xf0 && b[i + 5] === 0x11 && b[i + 6] === slot && b[i + 7] === block) return i;
    throw new Error('block not found');
  };
  b[find(3, 0x00) + 1] = 0x01;
  b[find(9, 0x10) + 4] = 0x02;
  return b;
}

describe('a bank file with damaged messages', () => {
  it('loads all 128 patches, as the keyboard does, instead of failing the whole file', () => {
    const { names } = previewFile(damaged(), 'allright.syx');
    expect(names).toHaveLength(128);
    expect(PatchStore.listSyx(damaged())).toHaveLength(128);
  });

  it('says what was skipped and which patch parts were reset', () => {
    const note = damageNotice(damaged(), 'allright.syx');
    expect(note).toContain('2 messages in allright.syx');
    expect(note).toContain('patch 004 common');
    expect(note).toContain('patch 010 tone 1');
  });

  it('says nothing about a clean file', () => {
    expect(damageNotice(BANK, 'clean.syx')).toBeNull();
  });

  it('names a file for another instrument instead of blaming a device ID', () => {
    const other = new Uint8Array([0xf0, 0x41, 0x09, 0x00, 0x0d, 0x01, 0x02, 0x03, 0x04, 0x76, 0xf7]);
    expect(() => previewFile(other, 'kasun.syx')).toThrow(/not the XP-60/);
  });
});
