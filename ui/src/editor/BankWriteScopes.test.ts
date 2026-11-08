import { afterEach, beforeAll, describe, expect, it, vi } from 'vitest';
import { cleanup, fireEvent, render, waitFor } from '@testing-library/svelte';
import { readFileSync } from 'node:fs';
import { PatchDoc, catalogJson, initSync } from '../wasm/pkg/xppatch.js';
import type { Backend } from '../model/backend';
import { BankService } from '../model/bank.svelte';
import { LibraryService } from '../model/library.svelte';
import { fakeBulks } from '../test/bulkBackend';
import { parseCatalog } from '../model/catalog';
import { arrangement } from '../model/layouts.svelte';
import { PatchStore } from '../model/store.svelte';
import { MemoryTransport } from '../model/transport';
import type { Services } from './services';
import BankViewHarness from './BankViewHarness.svelte';

beforeAll(() => {
  initSync({ module: readFileSync('src/wasm/pkg/xppatch_bg.wasm') });
});
afterEach(cleanup);

function blocks(name: string, seed = 1): Uint8Array {
  const d = new PatchDoc();
  d.setName(name);
  // Touch a tone level so two seeds differ when names match length.
  d.set('tone1', 'tone_level', Math.min(127, seed));
  const bytes = d.toBlocks();
  d.free();
  return bytes;
}

describe('Bank Write scopes', () => {
  it('offers changed / this slot / all filled after a synth sync, and this-slot works with zero changed', async () => {
    const fake = fakeBulks();
    const rows = Array.from({ length: 128 }, (_, i) => [i, blocks(`Slot ${i + 1}`, i)] as [number, Uint8Array]);
    const backend = new Proxy(
      {
        ...fake.api,
        readBank: vi.fn(async () => rows),
        armBank: vi.fn(async () => ({ backup_path: 'C:/b/bank.syx', names: [] })),
        disarmBank: vi.fn(async () => undefined),
        commitBank: vi.fn(async () => ({ written: [0], stopped_at: null, cancelled: false })),
        onProgress: vi.fn(async () => () => undefined),
      },
      { get: (t, k) => (k in t ? t[k as keyof typeof t] : vi.fn(async () => [])) },
    ) as unknown as Backend;
    const store = new PatchStore(parseCatalog(catalogJson()), new MemoryTransport());
    const bank = new BankService(backend);
    await bank.readFromSynth();
    expect(bank.changed).toEqual([]);
    expect(bank.filled).toHaveLength(128);
    const services = { bank, library: new LibraryService(backend), save: {}, systemSave: {} } as unknown as Services;
    arrangement.bankTab = 3;
    const { container, getByRole } = render(BankViewHarness, { props: { store, services } });

    const changedBtn = getByRole('button', { name: /1a\. Changed only \(0\)/ });
    const thisBtn = getByRole('button', { name: /1b\. This slot/ });
    const allBtn = getByRole('button', { name: /1c\. All filled \(128\)/ });
    expect((changedBtn as HTMLButtonElement).disabled).toBe(true);
    expect((thisBtn as HTMLButtonElement).disabled).toBe(false);
    expect((allBtn as HTMLButtonElement).disabled).toBe(false);
    expect(container.textContent).toMatch(/No slot differs from the last Read from synth/);
    const backup = container.querySelector('.backup-choice input') as HTMLInputElement | null;
    expect(backup).toBeTruthy();
    expect(backup!.checked).toBe(true);
    backup!.checked = false;
    await fireEvent.change(backup!);
    expect(bank.backupBeforeWrite).toBe(false);

    await fireEvent.click(thisBtn);
    await waitFor(() => expect(bank.phase).toBe('armed'));
    expect(bank.confirmedSlots).toEqual([0]);
    expect(backend.armBank).toHaveBeenCalledWith([0], false);
  });

  it('includes an editor edit of one synced pad in Changed only', async () => {
    const fake = fakeBulks();
    const rows = Array.from({ length: 8 }, (_, i) => [i, blocks(`U${i + 1}`, i)] as [number, Uint8Array]);
    const backend = new Proxy(
      {
        ...fake.api,
        readBank: vi.fn(async () => rows),
        armBank: vi.fn(async () => ({ backup_path: 'C:/b/bank.syx', names: [] })),
        disarmBank: vi.fn(async () => undefined),
        onProgress: vi.fn(async () => () => undefined),
      },
      { get: (t, k) => (k in t ? t[k as keyof typeof t] : vi.fn(async () => [])) },
    ) as unknown as Backend;
    const store = new PatchStore(parseCatalog(catalogJson()), new MemoryTransport());
    const bank = new BankService(backend);
    await bank.readFromSynth();
    bank.linkEditor(2);
    store.openBlocks(blocks('Edited', 99), 'Bank slot a13 · 003');
    const services = { bank, library: new LibraryService(backend), save: {}, systemSave: {} } as unknown as Services;
    arrangement.bankTab = 3;
    const { getByRole } = render(BankViewHarness, { props: { store, services } });
    await fireEvent.click(getByRole('button', { name: /1a\. Changed only/ }));
    await waitFor(() => expect(bank.phase).toBe('armed'));
    expect(bank.confirmedSlots).toEqual([2]);
    expect(bank.slots[2]?.name.trim()).toBe('Edited');
  });
});
