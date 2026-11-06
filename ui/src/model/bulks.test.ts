import { beforeAll, describe, expect, it, vi } from 'vitest';
import { readFileSync } from 'node:fs';
import { PatchDoc, initSync } from '../wasm/pkg/xppatch.js';
import type { Backend } from './backend';
import { BankService } from './bank.svelte';
import { BulkService, bulkNameFrom, parsePayload } from './bulks.svelte';
import { fakeBulks } from '../test/bulkBackend';

beforeAll(() => {
  initSync({ module: readFileSync('src/wasm/pkg/xppatch_bg.wasm') });
});

function blocks(name: string, level = 0): Uint8Array {
  const d = new PatchDoc();
  d.setName(name);
  d.set('tone1', 'tone_level', level);
  const b = d.toBlocks();
  d.free();
  return b;
}

/** A library with real patch bytes: three bulks' worth of material. */
function setup() {
  const fake = fakeBulks();
  const ids = ['Alpha', 'Beta', 'Gamma', 'Delta'].map((n, i) => {
    const id = fake.addPatch(n);
    fake.patches.get(id)!.bytes = blocks(n, i + 1);
    return id;
  });
  const a = fake.addBulk('allright', [ids[0]!, ids[1]!, ids[2]!]);
  const b = fake.addBulk('other', [ids[3]!]);
  const backend = fake.api as unknown as Backend;
  const bulks = new BulkService(backend);
  const bank = new BankService(backend);
  return { fake, ids, a, b, bulks, bank, backend };
}

describe('bulks in the library', () => {
  it('lists the bulks and expands one to its patches in order', async () => {
    const { bulks, a } = setup();
    await bulks.refresh();
    expect(bulks.bulks.map((b) => [b.name, b.count])).toEqual([
      ['allright', 3],
      ['other', 1],
    ]);
    await bulks.toggle(a);
    expect(bulks.open[a]).toBe(true);
    expect(bulks.contents[a]?.map((s) => [s.position, s.name])).toEqual([
      [0, 'Alpha'],
      [1, 'Beta'],
      [2, 'Gamma'],
    ]);
    await bulks.toggle(a);
    expect(bulks.open[a]).toBe(false);
  });

  it('searches patch names across every bulk and says which bulk holds each', async () => {
    const { bulks } = setup();
    await bulks.refresh();
    await bulks.search('a');
    expect(new Set(bulks.hits.map((h) => h.bulk_name))).toEqual(new Set(['allright', 'other']));
    await bulks.search('  ');
    expect(bulks.hits).toEqual([]);
  });

  it('finds a free name ignoring case', async () => {
    const { bulks } = setup();
    await bulks.refresh();
    expect(bulks.freeName('New bulk')).toBe('New bulk');
    expect(bulks.freeName('ALLRIGHT')).toBe('ALLRIGHT 2');
  });

  it('takes bulk names from file names', () => {
    expect(bulkNameFrom('allright.syx')).toBe('allright');
    expect(bulkNameFrom('Empty')).toBe('My bank');
  });
});

describe('the bank and the library', () => {
  it('loading a bulk fills the bank and makes it the bulk to save over', async () => {
    const { bulks, bank, a } = setup();
    await bulks.refresh();
    expect(await bulks.loadIntoBank(bank, a)).toBe(true);
    expect(bank.slots.filter((s) => s.bytes).map((s) => s.name.trim())).toEqual(['Alpha', 'Beta', 'Gamma']);
    expect(bank.origin).toEqual({ id: a, name: 'allright' });
    expect(bank.unsaved).toEqual([]);
    bank.assign(5, blocks('Fresh'), 'editor');
    expect(bank.unsaved).toEqual([5]);
  });

  it('saves the bank as a new bulk, which then counts as saved', async () => {
    const { bulks, bank, fake } = setup();
    bank.assign(0, blocks('Alpha', 1), 'editor');
    bank.assign(1, blocks('Mine', 9), 'editor');
    expect(await bulks.saveBank(bank, 'Live set', null)).toBe(true);
    expect(fake.calls).toContain('save new Live set');
    expect(bank.origin?.name).toBe('Live set');
    expect(bank.unsaved).toEqual([]);
    expect(bulks.bulks.some((b) => b.name === 'Live set' && b.count === 2)).toBe(true);
  });

  it('refuses a name that is taken and leaves the bank unsaved', async () => {
    const { bulks, bank } = setup();
    await bulks.refresh();
    bank.assign(0, blocks('X'), 'editor');
    expect(await bulks.saveBank(bank, 'allright', null)).toBe(false);
    expect(bank.origin).toBeNull();
    expect(bank.unsaved).toEqual([0]);
  });

  it('saves over the loaded bulk and keeps its old contents as a version that can be restored', async () => {
    const { bulks, bank, a } = setup();
    await bulks.refresh();
    await bulks.loadIntoBank(bank, a);
    bank.assign(0, blocks('Changed', 50), 'editor');
    expect(await bulks.saveBank(bank, 'allright', a)).toBe(true);
    expect(bank.unsaved).toEqual([]);
    await bulks.loadRevisions(a);
    expect(bulks.revisions[a]).toHaveLength(1);
    expect(await bulks.restore(a, bulks.revisions[a]![0]!.id)).toBe(true);
    await bulks.toggle(a);
    expect(bulks.contents[a]?.[0]?.name).toBe('Alpha');
  });

  it('will not save an empty bank', async () => {
    const { bulks, bank, fake } = setup();
    expect(await bulks.saveBank(bank, 'Nothing', null)).toBe(false);
    expect(fake.calls).toEqual([]);
  });

  it('a bank loaded from a file is not in the library', async () => {
    const { bulks, bank, a } = setup();
    await bulks.refresh();
    await bulks.loadIntoBank(bank, a);
    bank.loadFile(new Uint8Array(0), 'x.syx', []);
    expect(bank.origin).toBeNull();
  });
});

describe('dropping patches on the library', () => {
  const item = (bulk: number, position: number, patchId: number) => ({ kind: 'bulk-slot' as const, bulk, position, patchId });

  it('moves a single patch between bulks, and copies with Alt', async () => {
    const { bulks, bank, fake, a, b, ids } = setup();
    await bulks.refresh();
    await bulks.drop(bank, { kind: 'bulk-slot', bulk: b, position: 0 }, { type: 'patches', items: [item(a, 1, ids[1]!)] }, false);
    expect(fake.calls).toContain(`move ${a}.1 ${b}.0`);
    await bulks.drop(bank, { kind: 'bulk-slot', bulk: b, position: 7 }, { type: 'patches', items: [item(a, 0, ids[0]!)] }, true);
    expect(fake.calls).toContain(`place ${b} 7 ${ids[0]}`);
    expect(fake.bulks.get(a)!.slots.get(0)).toBe(ids[0]);
  });

  it('puts several patches in consecutive positions as copies', async () => {
    const { bulks, bank, fake, a, b, ids } = setup();
    await bulks.refresh();
    await bulks.drop(bank, { kind: 'bulk-slot', bulk: b, position: 4 }, { type: 'patches', items: [item(a, 0, ids[0]!), item(a, 1, ids[1]!)] }, false);
    expect([fake.bulks.get(b)!.slots.get(4), fake.bulks.get(b)!.slots.get(5)]).toEqual([ids[0], ids[1]]);
    expect(fake.bulks.get(a)!.slots.size).toBe(3);
  });

  it('adds a patch from the bank to a bulk, storing it first', async () => {
    const { bulks, bank, fake, b } = setup();
    await bulks.refresh();
    bank.assign(3, blocks('From bank', 77), 'editor');
    await bulks.drop(bank, { kind: 'bulk', bulk: b }, { type: 'patches', items: [{ kind: 'bank-slot', slot: 3 }] }, false);
    expect(fake.api.libraryAdd).toHaveBeenCalled();
    expect(fake.bulks.get(b)!.slots.size).toBe(2);
    expect(fake.calls.some((c) => c.startsWith(`append ${b} `))).toBe(true);
  });

  it('a patch dropped on a bulk header moves there (appended) and leaves its old place', async () => {
    const { bulks, bank, fake, a, b, ids } = setup();
    await bulks.refresh();
    await bulks.drop(bank, { kind: 'bulk', bulk: b }, { type: 'patches', items: [item(a, 2, ids[2]!)] }, false);
    expect(fake.bulks.get(a)!.slots.has(2)).toBe(false);
    expect([...fake.bulks.get(b)!.slots.values()]).toContain(ids[2]);
  });

  it('a drop on the new-bulk area makes a bulk and opens its name for typing', async () => {
    const { bulks, bank, fake, a, ids } = setup();
    await bulks.refresh();
    await bulks.drop(bank, { kind: 'new' }, { type: 'patches', items: [item(a, 0, ids[0]!), item(a, 1, ids[1]!)] }, false);
    const made = bulks.bulks.find((x) => x.name === 'New bulk');
    expect(made?.count).toBe(2);
    expect(bulks.renaming).toBe(made?.id);
    expect(fake.calls).toContain('save new New bulk');
  });

  it('a whole bulk dropped on the new-bulk area is copied', async () => {
    const { bulks, bank, a } = setup();
    await bulks.refresh();
    await bulks.drop(bank, { kind: 'new' }, { type: 'bulk', bulk: a }, false);
    expect(bulks.bulks.map((x) => x.name)).toContain('allright copy');
  });

  it('a whole bulk dropped on another bulk does nothing', async () => {
    const { bulks, bank, a, b, fake } = setup();
    await bulks.refresh();
    expect(await bulks.drop(bank, { kind: 'bulk', bulk: b }, { type: 'bulk', bulk: a }, false)).toBeNull();
    expect(fake.calls).toEqual([]);
  });

  it('shows why a change failed and changes nothing in the list', async () => {
    const { bulks, bank, a, ids } = setup();
    await bulks.refresh();
    vi.spyOn(console, 'error').mockImplementation(() => undefined);
    expect(await bulks.drop(bank, { kind: 'bulk-slot', bulk: 999, position: 0 }, { type: 'patches', items: [item(a, 0, ids[0]!)] }, true)).toBeNull();
  });
});

describe('what a drag carries', () => {
  it('reads our payloads and ignores anything else', () => {
    expect(parsePayload(JSON.stringify({ type: 'bulk', bulk: 4 }))).toEqual({ type: 'bulk', bulk: 4 });
    expect(parsePayload(JSON.stringify({ type: 'patches', items: [{ kind: 'patch', patchId: 1 }] }))?.type).toBe('patches');
    expect(parsePayload(JSON.stringify({ type: 'patches', items: [] }))).toBeNull();
    expect(parsePayload('not json')).toBeNull();
    expect(parsePayload(undefined)).toBeNull();
  });

  it('keeps a selection of patches and drops entries that no longer exist after a refresh', async () => {
    const { bulks, fake, a } = setup();
    await bulks.refresh();
    await bulks.toggle(a);
    bulks.toggleSelected(a, 0, fake.bulks.get(a)!.slots.get(0)!);
    bulks.toggleSelected(a, 1, fake.bulks.get(a)!.slots.get(1)!);
    expect(bulks.isSelected(a, 1)).toBe(true);
    bulks.toggleSelected(a, 1, 0);
    expect(bulks.selection).toHaveLength(1);
    fake.bulks.get(a)!.slots.delete(0);
    await bulks.refresh();
    expect(bulks.selection).toEqual([]);
  });
});
