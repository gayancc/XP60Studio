import { afterEach, beforeAll, describe, expect, it, vi } from 'vitest';
import { cleanup, fireEvent, render, waitFor } from '@testing-library/svelte';
import { readFileSync } from 'node:fs';
import { PatchDoc, catalogJson, initSync } from '../wasm/pkg/xppatch.js';
import type { Backend } from '../model/backend';
import { BankService } from '../model/bank.svelte';
import { DRAG_TYPE } from '../model/bulks.svelte';
import { LibraryService } from '../model/library.svelte';
import { fakeBulks } from '../test/bulkBackend';
import { parseCatalog } from '../model/catalog';
import { PatchStore } from '../model/store.svelte';
import { MemoryTransport } from '../model/transport';
import type { Services } from './services';
import BankViewHarness from './BankViewHarness.svelte';

beforeAll(() => {
  initSync({ module: readFileSync('src/wasm/pkg/xppatch_bg.wasm') });
});
afterEach(cleanup);

function setup() {
  const fake = fakeBulks();
  const backend = new Proxy(fake.api, { get: (t, k) => (k in t ? t[k as keyof typeof t] : vi.fn(async () => [])) }) as unknown as Backend;
  const store = new PatchStore(parseCatalog(catalogJson()), new MemoryTransport());
  const bank = new BankService(backend);
  const services = { bank, library: new LibraryService(backend), save: {}, systemSave: {} } as unknown as Services;
  return { ...render(BankViewHarness, { props: { store, services } }), bank, fake, services };
}

const slotButton = (c: HTMLElement, n: number): HTMLElement => c.querySelector<HTMLElement>(`.slot:nth-child(${n})`)!;
const lit = (c: HTMLElement): string[] => [...c.querySelectorAll('.pad.guide')].map((b) => b.textContent!.trim());
const libraryDrag = { dataTransfer: { types: ['text/xp-library'], getData: () => '' } };

describe('dragging a patch over the slot grid', () => {
  it('lights the sub-group, BANK and NUMBER buttons that pick the slot under the pointer, and follows it', async () => {
    const { container } = setup();
    expect(lit(container)).toEqual([]);
    await fireEvent.dragOver(slotButton(container, 28), libraryDrag); // a44 = slot 044
    expect(slotButton(container, 28).classList.contains('target')).toBe(true);
    expect(lit(container)).toEqual(['a', '4', '4']);
    await fireEvent.dragOver(slotButton(container, 62), libraryDrag); // a86 = slot 062
    expect(slotButton(container, 28).classList.contains('target')).toBe(false);
    expect(lit(container)).toEqual(['a', '8', '6']);
  });

  it('clears the guide when the pointer leaves the grid or the drag ends', async () => {
    const { container } = setup();
    await fireEvent.dragOver(slotButton(container, 10), libraryDrag);
    expect(lit(container).length).toBe(3);
    await fireEvent.dragLeave(slotButton(container, 10), { relatedTarget: document.body });
    expect(lit(container)).toEqual([]);
    await fireEvent.dragOver(slotButton(container, 10), libraryDrag);
    await fireEvent.dragEnd(slotButton(container, 10));
    expect(lit(container)).toEqual([]);
  });

  it('does not light anything for a drag it cannot accept', async () => {
    const { container } = setup();
    await fireEvent.dragOver(slotButton(container, 10), { dataTransfer: { types: ['Files'] } });
    expect(lit(container)).toEqual([]);
  });

  it('selects the slot on drop so the lit buttons stay on', async () => {
    const { container, bank } = setup();
    await fireEvent.dragOver(slotButton(container, 28), libraryDrag);
    await fireEvent.drop(slotButton(container, 28), libraryDrag);
    expect(bank.selected).toBe(27);
    expect(lit(container)).toEqual([]);
    expect(slotButton(container, 28).classList.contains('sel')).toBe(true);
  });

  it('sends Bank Select and Program Change on drop when Select on the instrument is on', async () => {
    const selectSound = vi.fn(async () => true);
    const synth = { connected: true, selectSound } as unknown as import('../model/synth.svelte').SynthService;
    const fake = fakeBulks();
    const backend = new Proxy(fake.api, { get: (t, k) => (k in t ? t[k as keyof typeof t] : vi.fn(async () => [])) }) as unknown as Backend;
    const store = new PatchStore(parseCatalog(catalogJson()), new MemoryTransport());
    const bank = new BankService(backend);
    const services = { bank, library: new LibraryService(backend), save: {}, systemSave: {} } as unknown as Services;
    const { container } = render(BankViewHarness, { props: { store, services, synth } });
    await fireEvent.drop(slotButton(container, 28), libraryDrag);
    expect(bank.selected).toBe(27);
    expect(selectSound).toHaveBeenCalledWith(0, 80, 0, 27);
  });

  it('puts several dragged patches into the slots that follow the one dropped on', async () => {
    const { container, bank, fake } = setup();
    const ids = [1, 2].map((i) => {
      const id = fake.addPatch(`P${i}`);
      const d = new PatchDoc();
      d.setName(`Drag ${i}`);
      fake.patches.get(id)!.bytes = d.toBlocks();
      d.free();
      return id;
    });
    const payload = JSON.stringify({ type: 'patches', items: ids.map((patchId) => ({ kind: 'patch', patchId })) });
    await fireEvent.drop(slotButton(container, 10), { dataTransfer: { types: [DRAG_TYPE], getData: (t: string) => (t === DRAG_TYPE ? payload : '') } });
    await waitFor(() => expect(bank.slots[9]?.name.trim()).toBe('Drag 1'));
    await waitFor(() => expect(bank.slots[10]?.name.trim()).toBe('Drag 2'));
    expect(bank.selected).toBe(9);
  });

  it('a whole bulk dropped on the grid asks before it replaces the bank, and changes nothing yet', async () => {
    const { container, bank, services } = setup();
    const payload = JSON.stringify({ type: 'bulk', bulk: 7 });
    await fireEvent.drop(slotButton(container, 3), { dataTransfer: { types: [DRAG_TYPE], getData: (t: string) => (t === DRAG_TYPE ? payload : '') } });
    expect(services.library.bulks.pendingLoad).toBe(7);
    expect(bank.slots.every((s) => s.bytes === null)).toBe(true);
    expect(bank.selected).toBe(0);
  });
});
