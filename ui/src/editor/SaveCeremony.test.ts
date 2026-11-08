import { messages } from '../model/messages.svelte';
import { afterEach, beforeAll, beforeEach, describe, expect, it, vi } from 'vitest';
import { cleanup, fireEvent, render, waitFor } from '@testing-library/svelte';
import { readFileSync } from 'node:fs';
import { catalogJson, initSync } from '../wasm/pkg/xppatch.js';
import { parseCatalog, type Catalog } from '../model/catalog';
import { SaveFlow } from '../model/save.svelte';
import { PatchStore } from '../model/store.svelte';
import { SystemSaveFlow } from '../model/systemSave.svelte';
import { MemoryTransport } from '../model/transport';
import SaveHarness from './SaveHarness.svelte';
import type { Services } from './services';

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

const report = { messages: 3, bytes: 120, verified: true };
function systemBackend() {
  return {
    armSystemSave: vi.fn(async () => ({ backup_path: 'C:/backups/1-system.syx' })),
    disarmSystemSave: vi.fn(async () => {}),
    commitSystemSave: vi.fn(async () => report),
    restoreSystemSave: vi.fn(async () => report),
  };
}
const services = (b: ReturnType<typeof systemBackend>): Services => ({ systemSave: new SystemSaveFlow(b), save: {} as SaveFlow } as unknown as Services);
const systemStore = (): PatchStore => new PatchStore(catalog, new MemoryTransport(), 0x10, undefined, 'system');
const button = (text: string | RegExp) => (c: HTMLElement) =>
  [...c.querySelectorAll('button')].find((b) => (typeof text === 'string' ? b.textContent?.trim() === text : text.test(b.textContent ?? ''))) as HTMLButtonElement;

describe('writing the System settings from the top bar', () => {
  it('opens a warning panel that says it replaces the settings for good and writes nothing yet', async () => {
    const b = systemBackend();
    const { container } = render(SaveHarness, { props: { store: systemStore(), services: services(b), which: 'system' } });
    await fireEvent.click(button('Write to synth…')(container));
    const panel = container.querySelector('#system-save-panel') as HTMLElement;
    expect(panel.textContent).toMatch(/replaces/);
    expect(panel.textContent).toMatch(/backup file/);
    expect(b.armSystemSave).not.toHaveBeenCalled();
    expect(b.commitSystemSave).not.toHaveBeenCalled();
  });

  it('walks read-and-back-up, a second explicit confirmation, then the write, and offers an undo', async () => {
    const b = systemBackend();
    const store = systemStore();
    const { container, findByText } = render(SaveHarness, { props: { store, services: services(b), which: 'system' } });
    await fireEvent.click(button('Write to synth…')(container));
    await fireEvent.click(button(/^1\./)(container));
    await findByText(/Backed up to C:\/backups\/1-system.syx/);
    expect(b.commitSystemSave).not.toHaveBeenCalled(); // armed is not written
    expect(document.activeElement?.textContent?.trim()).toBe('Cancel'); // the safe choice has focus
    await fireEvent.click(button(/^2\./)(container));
    await waitFor(() => expect(messages.list.map((m) => m.text).join(' | ')).toMatch(/System settings written: read back and they match/));
    expect(b.commitSystemSave).toHaveBeenCalledTimes(1);
    expect((b.commitSystemSave.mock.calls[0] as unknown[])[0]).toEqual(store.snapshot());
    await fireEvent.click(button('Undo the last write…')(container));
    await fireEvent.click(button('Put the old settings back')(container));
    await waitFor(() => expect(b.restoreSystemSave).toHaveBeenCalledTimes(1));
  });

  it('cancelling from the confirmation writes nothing and disarms', async () => {
    const b = systemBackend();
    const { container, findByText } = render(SaveHarness, { props: { store: systemStore(), services: services(b), which: 'system' } });
    await fireEvent.click(button('Write to synth…')(container));
    await fireEvent.click(button(/^1\./)(container));
    await findByText(/Backed up to/);
    await fireEvent.click(button('Cancel')(container));
    await waitFor(() => expect(b.disarmSystemSave).toHaveBeenCalledTimes(1));
    expect(b.commitSystemSave).not.toHaveBeenCalled();
  });

  it('shows why when the instrument is not connected', async () => {
    const b = systemBackend();
    b.armSystemSave.mockRejectedValueOnce('not connected to an instrument');
    const { container, findByText } = render(SaveHarness, { props: { store: systemStore(), services: services(b), which: 'system' } });
    await fireEvent.click(button('Write to synth…')(container));
    await fireEvent.click(button(/^1\./)(container));
    await waitFor(() => expect(messages.list.map((m) => m.text).join(' | ')).toMatch(/not connected to an instrument/i));
    expect(messages.list.at(-1)?.kind).toBe('error');
  });
});
