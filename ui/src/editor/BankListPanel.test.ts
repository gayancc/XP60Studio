import { afterEach, beforeAll, beforeEach, describe, expect, it } from 'vitest';
import { cleanup, fireEvent, render } from '@testing-library/svelte';
import { readFileSync } from 'node:fs';
import { catalogJson, initSync } from '../wasm/pkg/xppatch.js';
import { parseCatalog, type Catalog } from '../model/catalog';
import { PatchStore } from '../model/store.svelte';
import { MemoryTransport } from '../model/transport';
import { ui } from '../model/ui.svelte';
import BankHarness from './BankHarness.svelte';

let catalog: Catalog;
const BANK = new Uint8Array(readFileSync('../tests/golden/xp60-user-bank.syx'));
beforeAll(() => {
  initSync({ module: readFileSync('src/wasm/pkg/xppatch_bg.wasm') });
  catalog = parseCatalog(catalogJson());
});
beforeEach(() => {
  ui.bank = { file: 'bank.syx', bytes: BANK, names: Array.from({ length: 128 }, (_, i) => [i, `Patch ${i}`] as [number, string]) };
  ui.bankSlot = 0;
});
afterEach(() => {
  cleanup();
  ui.bank = null;
});

const make = (): PatchStore => new PatchStore(catalog, new MemoryTransport());

describe('the bank list', () => {
  it('lists every patch and marks the open one', () => {
    const { container } = render(BankHarness, { props: { store: make() } });
    expect(container.querySelectorAll('ol button')).toHaveLength(128);
    expect(container.querySelector('ol button[aria-current="true"]')?.textContent).toContain('Patch 0');
  });

  it('opens a patch when it is clicked, and the next/previous buttons step', async () => {
    const store = make();
    const { container, getByLabelText } = render(BankHarness, { props: { store } });
    await fireEvent.click(container.querySelectorAll('ol button')[5] as HTMLElement);
    expect(ui.bankSlot).toBe(5);
    await fireEvent.click(getByLabelText('Next patch'));
    expect(ui.bankSlot).toBe(6);
    await fireEvent.click(getByLabelText('Previous patch'));
    expect(ui.bankSlot).toBe(5);
  });

  it('disables previous on the first patch', () => {
    const { getByLabelText } = render(BankHarness, { props: { store: make() } });
    expect((getByLabelText('Previous patch') as HTMLButtonElement).disabled).toBe(true);
  });

  it('explains itself when no bank is open', () => {
    ui.bank = null;
    const { container } = render(BankHarness, { props: { store: make() } });
    expect(container.textContent).toContain('No bank is open');
  });
});
