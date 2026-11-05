import { beforeAll, beforeEach, describe, expect, it } from 'vitest';
import { readFileSync } from 'node:fs';
import { catalogJson, initSync } from '../wasm/pkg/xppatch.js';
import { canStepBank, openBankSlot, stepBankSlot } from './bankNav';
import { parseCatalog, type Catalog } from './catalog';
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
const names = (n: number): Array<[number, string]> => Array.from({ length: n }, (_, i) => [i, `p${i}`]);

beforeEach(() => {
  ui.bank = { file: 'bank.syx', bytes: BANK, names: names(128) };
  ui.bankSlot = 0;
});

describe('moving through the open bank', () => {
  it('opens a patch of the bank and remembers which one', () => {
    const store = make();
    expect(openBankSlot(store, 17)).toBe(true);
    expect(ui.bankSlot).toBe(17);
    expect(store.name.trim()).toBe('Clav 1 x4');
  });

  it('steps forwards and backwards and stops at both ends', () => {
    const store = make();
    expect(canStepBank(-1)).toBe(false);
    expect(stepBankSlot(store, -1)).toBe(false);
    expect(stepBankSlot(store, 1)).toBe(true);
    expect(ui.bankSlot).toBe(1);
    ui.bankSlot = 127;
    expect(canStepBank(1)).toBe(false);
    expect(stepBankSlot(store, 1)).toBe(false);
    expect(ui.bankSlot).toBe(127);
  });

  it('does nothing when no bank is open or the slot is not in it', () => {
    const store = make();
    expect(openBankSlot(store, 500)).toBe(false);
    ui.bank = null;
    expect(openBankSlot(store, 0)).toBe(false);
    expect(canStepBank(1)).toBe(false);
    expect(stepBankSlot(store, 1)).toBe(false);
  });
});
