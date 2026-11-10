import { afterEach, beforeAll, describe, expect, it, vi } from 'vitest';
import { cleanup, createEvent, fireEvent, render, waitFor } from '@testing-library/svelte';
import { readFileSync } from 'node:fs';
import { PatchDoc, initSync } from '../wasm/pkg/xppatch.js';
import type { Backend } from '../model/backend';
import { BankService } from '../model/bank.svelte';
import { DRAG_TYPE, type DragPayload } from '../model/bulks.svelte';
import { LibraryService } from '../model/library.svelte';
import { messages } from '../model/messages.svelte';
import { fakeBulks } from '../test/bulkBackend';
import BulkPanelHarness from './BulkPanelHarness.svelte';

beforeAll(() => {
  initSync({ module: readFileSync('src/wasm/pkg/xppatch_bg.wasm') });
});
afterEach(cleanup);

function blocks(name: string, level = 0): Uint8Array {
  const d = new PatchDoc();
  d.setName(name);
  d.set('tone1', 'tone_level', level);
  const b = d.toBlocks();
  d.free();
  return b;
}

function setup() {
  const fake = fakeBulks();
  const ids = ['Alpha', 'Beta', 'Gamma', 'Delta'].map((n, i) => {
    const id = fake.addPatch(n);
    fake.patches.get(id)!.bytes = blocks(n, i + 1);
    return id;
  });
  const a = fake.addBulk('allright', [ids[0]!, ids[1]!, ids[2]!]);
  const b = fake.addBulk('other', [ids[3]!]);
  const libraryImport = vi.fn(async (_bytes: Uint8Array, name: string) => {
    const bulkName = name.replace(/\.(syx|mid|midi)$/i, '') || 'imported';
    const bulkId = fake.addBulk(bulkName, [fake.addPatch('Imported A'), fake.addPatch('Imported B')]);
    return { added: 2, duplicates: 0, bulk_id: bulkId, bulk_name: bulkName };
  });
  const backend = {
    ...fake.api,
    libraryImport,
    libraryList: vi.fn(async () => []),
    libraryCount: vi.fn(async () => 0),
    libraryTags: vi.fn(async () => []),
  } as unknown as Backend;
  const lib = new LibraryService(backend);
  const bank = new BankService(backend);
  const view = render(BulkPanelHarness, { props: { bank, lib } });
  return { fake, ids, a, b, bank, lib, libraryImport, ...view };
}

const text = (e: Element | null): string => e?.textContent?.replace(/\s+/g, ' ').trim() ?? '';
const button = (c: HTMLElement, label: RegExp): HTMLButtonElement => {
  const b = [...c.querySelectorAll('button')].find((x) => label.test(x.textContent ?? '') || label.test(x.getAttribute('aria-label') ?? ''));
  if (!b) throw new Error(`no button ${label}`);
  return b as HTMLButtonElement;
};
const row = (c: HTMLElement, name: string): HTMLElement => {
  const b = [...c.querySelectorAll<HTMLElement>('.prow')].find((x) => text(x).includes(name));
  if (!b) throw new Error(`no row ${name}`);
  return b;
};
const payload = (p: DragPayload) => ({ dataTransfer: { types: [DRAG_TYPE], getData: (t: string) => (t === DRAG_TYPE ? JSON.stringify(p) : '') } });
const item = (bulk: number, position: number, patchId: number) => ({ kind: 'bulk-slot' as const, bulk, position, patchId });

describe('the library as bulks', () => {
  it('lists each bulk under its name with its patch count, and opens one to its patches in order', async () => {
    const { container } = setup();
    await waitFor(() => expect(container.querySelectorAll('.bulk').length).toBe(2));
    expect([...container.querySelectorAll('.head')].map((h) => text(h))).toEqual(['▸ allright 3', '▸ other 1']);
    await fireEvent.click(button(container, /Open allright/));
    await waitFor(() => expect(container.querySelectorAll('.slots .prow').length).toBe(3));
    expect([...container.querySelectorAll('.slots .prow')].map((r) => text(r))).toEqual(['Alpha a11 · 001', 'Beta a12 · 002', 'Gamma a13 · 003']);
  });

  it('tints the place a drop would land, with a sentence saying what it does, and clears it when the pointer leaves', async () => {
    const { container, a, ids } = setup();
    await waitFor(() => expect(container.querySelectorAll('.bulk').length).toBe(2));
    await fireEvent.click(button(container, /Open other/));
    await waitFor(() => expect(container.querySelectorAll('.slots .prow').length).toBe(1));
    const target = row(container, 'Delta');
    await fireEvent.dragOver(target, payload({ type: 'patches', items: [item(a, 0, ids[0]!)] }));
    expect(target.classList.contains('target')).toBe(true);
    expect(text(container.querySelector('.hint'))).toContain('Drop to put the patch at a11 · 001 of other');
    await fireEvent.dragLeave(target, { relatedTarget: document.body });
    expect(target.classList.contains('target')).toBe(false);
    expect(text(container.querySelector('.hint'))).toContain('Drag a patch onto');
  });

  it('moves a patch to another bulk on drop, and copies with Alt', async () => {
    const { container, fake, a, b, ids } = setup();
    await waitFor(() => expect(container.querySelectorAll('.bulk').length).toBe(2));
    await fireEvent.click(button(container, /Open other/));
    await waitFor(() => expect(container.querySelectorAll('.slots .prow').length).toBe(1));
    await fireEvent.drop(row(container, 'Delta'), payload({ type: 'patches', items: [item(a, 1, ids[1]!)] }));
    await waitFor(() => expect(fake.calls).toContain(`move ${a}.1 ${b}.0`));
    await waitFor(() => expect(row(container, 'Beta')).toBeTruthy());
    const withAlt = createEvent.drop(row(container, 'Beta'), payload({ type: 'patches', items: [item(a, 0, ids[0]!)] }));
    Object.defineProperty(withAlt, 'altKey', { value: true }); // jsdom has no DragEvent, so the key is set on the event itself
    await fireEvent(row(container, 'Beta'), withAlt);
    await waitFor(() => expect(fake.calls).toContain(`place ${b} 0 ${ids[0]}`));
    expect(fake.bulks.get(a)!.slots.get(0)).toBe(ids[0]);
  });

  it('a drop on the new-bulk area makes a bulk and opens its name box', async () => {
    const { container, fake, a, ids } = setup();
    await waitFor(() => expect(container.querySelectorAll('.bulk').length).toBe(2));
    await fireEvent.drop(container.querySelector('.newzone')!, payload({ type: 'patches', items: [item(a, 0, ids[0]!)] }));
    await waitFor(() => expect(container.querySelector('input.rename')).toBeTruthy());
    expect(fake.calls).toContain('save new New bulk');
    const box = container.querySelector<HTMLInputElement>('input.rename')!;
    await fireEvent.input(box, { target: { value: 'Stage set' } });
    await fireEvent.keyDown(box, { key: 'Enter' });
    await waitFor(() => expect([...fake.bulks.values()].some((x) => x.name === 'Stage set')).toBe(true));
  });

  it('asks before replacing the bank with a bulk, then loads it and marks the bulk as the one in the bank', async () => {
    const { container, bank, a } = setup();
    await waitFor(() => expect(container.querySelectorAll('.bulk').length).toBe(2));
    await fireEvent.click(button(container, /Open allright/));
    await waitFor(() => expect(container.querySelector('.tools')).toBeTruthy());
    await fireEvent.click(button(container, /Load into bank/));
    expect(text(container.querySelector('.ask'))).toContain('Replace the bank with allright?');
    await fireEvent.click(button(container, /^Replace$/));
    await waitFor(() => expect(bank.origin?.id).toBe(a));
    expect(bank.slots.filter((s) => s.bytes).length).toBe(3);
    await waitFor(() => expect(container.querySelector('.head.loaded')).toBeTruthy());
  });

  it('after a bulk fills the bank, a click on a patch from another bulk still replaces the selected slot', async () => {
    const onput = vi.fn();
    const fake = fakeBulks();
    const ids = ['Alpha', 'Beta', 'Gamma', 'Delta'].map((n, i) => {
      const id = fake.addPatch(n);
      fake.patches.get(id)!.bytes = blocks(n, i + 1);
      return id;
    });
    const a = fake.addBulk('allright', [ids[0]!, ids[1]!, ids[2]!]);
    fake.addBulk('other', [ids[3]!]);
    const backend = {
      ...fake.api,
      libraryImport: vi.fn(),
      libraryList: vi.fn(async () => []),
      libraryCount: vi.fn(async () => 0),
      libraryTags: vi.fn(async () => []),
    } as unknown as Backend;
    const lib = new LibraryService(backend);
    const bank = new BankService(backend);
    const { container } = render(BulkPanelHarness, { props: { bank, lib, onput } });
    await waitFor(() => expect(container.querySelectorAll('.bulk').length).toBe(2));
    await fireEvent.click(button(container, /Open allright/));
    await waitFor(() => expect(container.querySelector('.tools')).toBeTruthy());
    await fireEvent.click(button(container, /Load into bank/));
    await fireEvent.click(button(container, /^Replace$/));
    await waitFor(() => expect(bank.origin?.id).toBe(a));
    expect(bank.slots[0]?.name.trim()).toBe('Alpha');
    await fireEvent.click(button(container, /Open other/));
    await waitFor(() => expect(row(container, 'Delta')).toBeTruthy());
    await fireEvent.click(row(container, 'Delta'));
    await waitFor(() => expect(bank.slots[0]?.name.trim()).toBe('Delta'));
    expect(onput).toHaveBeenCalledWith(0);
  });

  it('deleting asks first and keeps the bulk if you say keep', async () => {
    const { container, fake, b } = setup();
    await waitFor(() => expect(container.querySelectorAll('.bulk').length).toBe(2));
    await fireEvent.click(button(container, /Open other/));
    await waitFor(() => expect(container.querySelector('.tools')).toBeTruthy());
    await fireEvent.click(button(container, /^Delete$/));
    await fireEvent.click(button(container, /^Keep$/));
    expect(fake.bulks.has(b)).toBe(true);
    await fireEvent.click(button(container, /^Delete$/));
    await fireEvent.click(container.querySelector('.ask button.danger')!);
    await waitFor(() => expect(fake.bulks.has(b)).toBe(false));
  });

  it('searches patches in every bulk and shows the bulk of each hit', async () => {
    const { container } = setup();
    await waitFor(() => expect(container.querySelectorAll('.bulk').length).toBe(2));
    const search = container.querySelector<HTMLInputElement>('input[type=search]')!;
    await fireEvent.input(search, { target: { value: 'delt' } });
    await waitFor(() => expect(container.querySelectorAll('.hits .prow').length).toBe(1));
    expect(text(container.querySelector('.hits .prow'))).toContain('other');
  });

  it('ctrl-click picks several patches and a drag then carries all of them', async () => {
    const { container, ids, a, bulks } = (() => {
      const s = setup();
      return { ...s, bulks: s.lib.bulks };
    })();
    await waitFor(() => expect(container.querySelectorAll('.bulk').length).toBe(2));
    await fireEvent.click(button(container, /Open allright/));
    await waitFor(() => expect(container.querySelectorAll('.slots .prow').length).toBe(3));
    await fireEvent.click(row(container, 'Alpha'), { ctrlKey: true });
    await fireEvent.click(row(container, 'Gamma'), { ctrlKey: true });
    expect(bulks.selection.map((s) => s.position)).toEqual([0, 2]);
    const data: Record<string, string> = {};
    await fireEvent.dragStart(row(container, 'Gamma'), { dataTransfer: { setData: (t: string, v: string) => (data[t] = v), effectAllowed: '' } });
    const sent = JSON.parse(data[DRAG_TYPE]!) as DragPayload;
    expect(sent.type === 'patches' && sent.items.map((i) => i.kind === 'bulk-slot' && i.patchId)).toEqual([ids[0], ids[2]]);
    expect(a).toBeGreaterThan(0);
  });
});

async function pickSyx(container: HTMLElement, name: string, bytes: Uint8Array): Promise<void> {
  const input = container.querySelector<HTMLInputElement>('input[type=file][accept]')!;
  const file = new File([bytes as BlobPart], name, { type: 'application/octet-stream' });
  Object.defineProperty(input, 'files', { configurable: true, value: [file] });
  await fireEvent.change(input);
}

describe('importing a bank file into the library', () => {
  it('stages a picked .syx, then imports it as a bulk after confirm', async () => {
    const BANK = new Uint8Array(readFileSync('../tests/golden/xp60-user-bank.syx'));
    const { container, lib, libraryImport } = setup();
    await waitFor(() => expect(container.querySelectorAll('.bulk').length).toBe(2));
    expect(button(container, /^Import/).disabled).toBe(false);

    await pickSyx(container, 'session-bank.syx', BANK);
    await waitFor(() => expect(lib.staged?.names.length).toBe(128));
    expect(text(container.querySelector('.staged'))).toContain('session-bank.syx');
    expect(text(container.querySelector('.staged'))).toContain('128 patches');
    expect(libraryImport).not.toHaveBeenCalled();

    await fireEvent.click(button(container, /Import 128 as bulk/));
    await waitFor(() => expect(libraryImport).toHaveBeenCalledTimes(1));
    await waitFor(() => expect(lib.staged).toBeNull());
    await waitFor(() => expect(container.querySelectorAll('.bulk').length).toBe(3));
    expect(container.querySelector('.ok')).toBeNull();
    expect(messages.list.map((m) => m.text).join(' | ')).toContain('Imported "session-bank"');
    expect([...container.querySelectorAll('.head')].map((h) => text(h)).some((t) => t.includes('session-bank'))).toBe(true);
  });

  it('cancels a staged import without calling the backend', async () => {
    const BANK = new Uint8Array(readFileSync('../tests/golden/xp60-user-bank.syx'));
    const { container, lib, libraryImport } = setup();
    await pickSyx(container, 'tmp.syx', BANK);
    await waitFor(() => expect(lib.staged).not.toBeNull());
    await fireEvent.click(container.querySelector('.staged button:not(.primary)')!);
    expect(lib.staged).toBeNull();
    expect(libraryImport).not.toHaveBeenCalled();
  });
});

describe('saving the bank into the library', () => {
  it('saves the bank as a new bulk, which is then the bulk in the bank', async () => {
    const { container, bank, fake } = setup();
    await waitFor(() => expect(container.querySelectorAll('.bulk').length).toBe(2));
    expect(button(container, /Save as new bulk/).disabled).toBe(true); // an empty bank has nothing to save
    bank.assign(0, blocks('Mine', 9), 'editor');
    await waitFor(() => expect(button(container, /Save as new bulk/).disabled).toBe(false));
    await fireEvent.click(button(container, /Save as new bulk/));
    const box = container.querySelector<HTMLInputElement>('.ask input')!;
    await fireEvent.input(box, { target: { value: 'Live set' } });
    await fireEvent.click(container.querySelector('.ask .go')!);
    await waitFor(() => expect(bank.origin?.name).toBe('Live set'));
    expect(fake.calls).toContain('save new Live set');
    expect(text(container.querySelector('.bar .line'))).toContain('Live set');
  });

  it('saves over the bulk it came from only after asking, and keeps a version', async () => {
    const { container, bank, lib, a, fake } = setup();
    await waitFor(() => expect(container.querySelectorAll('.bulk').length).toBe(2));
    await lib.bulks.loadIntoBank(bank, a);
    await waitFor(() => expect(text(container.querySelector('.bar .line'))).toContain('allright'));
    expect(button(container, /^Save$/).disabled).toBe(true); // nothing changed since loading
    bank.assign(0, blocks('Edited', 33), 'editor');
    await waitFor(() => expect(button(container, /^Save$/).disabled).toBe(false));
    expect(text(container.querySelector('.bar .line'))).toContain('modified · 1');
    await fireEvent.click(button(container, /^Save$/));
    expect(text(container.querySelector('.bar .ask'))).toContain('Save over allright?');
    expect(fake.calls.filter((c) => c.startsWith('save'))).toEqual([]);
    await fireEvent.click(button(container, /^Save over$/));
    await waitFor(() => expect(fake.calls).toContain(`save ${a} allright`));
    await waitFor(() => expect(bank.unsaved).toEqual([]));
    expect(fake.bulks.get(a)!.revisions.length).toBe(1);
  });

  it('shows the kept versions of a bulk and restores one', async () => {
    const { container, bank, lib, a, fake } = setup();
    await waitFor(() => expect(container.querySelectorAll('.bulk').length).toBe(2));
    await lib.bulks.loadIntoBank(bank, a);
    bank.assign(0, blocks('Edited', 33), 'editor');
    await lib.bulks.saveBank(bank, 'allright', a);
    await waitFor(() => expect(container.querySelector('.tools')).toBeTruthy()); // saving opened the bulk
    await fireEvent.click(button(container, /^Versions$/));
    await waitFor(() => expect(container.querySelectorAll('.versions li button').length).toBe(1));
    await fireEvent.click(button(container, /^Restore$/));
    await waitFor(() => expect(fake.bulks.get(a)!.slots.size).toBe(3));
    await waitFor(() => expect(fake.bulks.get(a)!.revisions.length).toBe(2)); // the restore kept the current contents too
  });
});
