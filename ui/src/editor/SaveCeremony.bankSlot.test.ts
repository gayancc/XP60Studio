import { messages } from '../model/messages.svelte';
import { afterEach, beforeAll, beforeEach, describe, expect, it, vi } from 'vitest';
import { cleanup, fireEvent, render, waitFor } from '@testing-library/svelte';
import { readFileSync } from 'node:fs';
import { PatchDoc, catalogJson, initSync } from '../wasm/pkg/xppatch.js';
import type { Backend } from '../model/backend';
import { BankService } from '../model/bank.svelte';
import { parseCatalog, type Catalog } from '../model/catalog';
import { SaveFlow } from '../model/save.svelte';
import { PatchStore } from '../model/store.svelte';
import { MemoryTransport } from '../model/transport';
import { fakeBulks } from '../test/bulkBackend';
import type { Services } from './services';
import SaveHarness from './SaveHarness.svelte';

let catalog: Catalog;
beforeAll(() => {
  initSync({ module: readFileSync('src/wasm/pkg/xppatch_bg.wasm') });
  catalog = parseCatalog(catalogJson());
});
beforeEach(() => messages.clear());
afterEach(() => {
  cleanup();
  messages.clear();
});

const button = (text: string | RegExp) => (c: HTMLElement) =>
  [...c.querySelectorAll('button')].find((b) => (typeof text === 'string' ? b.textContent?.trim() === text : text.test(b.textContent ?? ''))) as HTMLButtonElement;

function blocks(name: string): Uint8Array {
  const d = new PatchDoc();
  d.setName(name);
  const b = d.toBlocks();
  d.free();
  return b;
}

describe('SaveCeremony with a Bank-opened patch', () => {
  it('locks the User slot to the Bank pad the patch was opened from (no slot stepper)', async () => {
    const armSave = vi.fn(async (slot: number) => ({ slot, occupant_name: 'Old', backup_path: 'C:/b/u.syx' }));
    const commitSave = vi.fn(async () => ({ slot: 17, messages: 9, bytes: 590, verified: true }));
    const fake = fakeBulks();
    const backend = {
      ...fake.api,
      armSave,
      disarmSave: vi.fn(async () => undefined),
      commitSave,
      restoreSave: vi.fn(async () => ({ slot: 17, messages: 9, bytes: 590, verified: true })),
    } as unknown as Backend;
    const bank = new BankService(backend);
    bank.assign(17, blocks('From Bank'), 'synth');
    bank.linkEditor(17);
    const store = new PatchStore(catalog, new MemoryTransport());
    store.openBlocks(blocks('Edited Bank'), 'Bank slot 018');
    const save = new SaveFlow(backend);
    const services = { bank, save, library: {}, systemSave: {} } as unknown as Services;
    const { container, findByText } = render(SaveHarness, { props: { store, services, which: 'patch' } });

    await fireEvent.click(button('Save to synth…')(container));
    await findByText(/Write to/);
    expect(container.textContent).toMatch(/USER:018/);
    expect(container.textContent).toMatch(/Bank pad this patch was opened from/);
    expect(container.querySelector('[aria-label="User slot"]')).toBeNull();

    await fireEvent.click(button(/^1\./)(container));
    await waitFor(() => expect(armSave).toHaveBeenCalledWith(17, true));
    await fireEvent.click(button(/^2\./)(container));
    await waitFor(() => expect(commitSave).toHaveBeenCalled());
    expect(bank.slots[17]?.name.trim()).toBe('Edited Bank');
    expect(bank.changed).toEqual([]);
  });

  it('still offers a slot stepper when the patch was not opened from the Bank', async () => {
    const backend = {
      ...fakeBulks().api,
      armSave: vi.fn(async (slot: number) => ({ slot, occupant_name: 'Old', backup_path: '' })),
      disarmSave: vi.fn(async () => undefined),
      commitSave: vi.fn(async () => ({ slot: 0, messages: 9, bytes: 590, verified: true })),
    } as unknown as Backend;
    const bank = new BankService(backend);
    const store = new PatchStore(catalog, new MemoryTransport());
    const save = new SaveFlow(backend);
    const services = { bank, save, library: {}, systemSave: {} } as unknown as Services;
    const { container } = render(SaveHarness, { props: { store, services, which: 'patch' } });
    await fireEvent.click(button('Save to synth…')(container));
    expect(container.querySelector('[aria-label="User slot"], [name="User slot"]')).toBeTruthy();
  });
});
