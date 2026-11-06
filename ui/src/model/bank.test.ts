import { beforeAll, describe, expect, it, vi } from 'vitest';
import { readFileSync } from 'node:fs';
import { PatchDoc, initSync, syxNames } from '../wasm/pkg/xppatch.js';
import type { ArmedBank, Backend, BankSaveReport, Progress } from './backend';
import { BankService, SLOTS } from './bank.svelte';
import { fakeBulks } from '../test/bulkBackend';

beforeAll(() => {
  initSync({ module: readFileSync('src/wasm/pkg/xppatch_bg.wasm') });
});

const BANK = new Uint8Array(readFileSync('../tests/golden/xp60-user-bank.syx'));

function blocks(name: string, level = 0): Uint8Array {
  const d = new PatchDoc();
  d.setName(name);
  d.set('tone1', 'tone_level', level);
  const b = d.toBlocks();
  d.free();
  return b;
}

function fake(overrides: Partial<Backend> = {}): Backend {
  const never = (): never => {
    throw new Error('not expected');
  };
  return {
    ...fakeBulks().api,
    libraryImport: vi.fn(never),
    libraryList: vi.fn(never),
    libraryCount: vi.fn(never),
    libraryTags: vi.fn(never),
    libraryGet: vi.fn(never),
    libraryAdd: vi.fn(never),
    libraryFavourite: vi.fn(never),
    libraryTagAdd: vi.fn(never),
    libraryTagRemove: vi.fn(never),
    libraryRename: vi.fn(never),
    libraryDelete: vi.fn(never),
    libraryExport: vi.fn(never),
    backupsList: vi.fn(never),
    backupRead: vi.fn(never),
    backupAll: vi.fn(never),
    storageInfo: vi.fn(never),
    storageSet: vi.fn(never),
    armSave: vi.fn(never),
    disarmSave: vi.fn(never),
    commitSave: vi.fn(never),
    restoreSave: vi.fn(never),
    armSystemSave: vi.fn(never),
    disarmSystemSave: vi.fn(never),
    commitSystemSave: vi.fn(never),
    restoreSystemSave: vi.fn(never),
    saveFile: vi.fn(never),
    readBank: vi.fn(async () =>
      Array.from({ length: SLOTS }, (_, i): [number, Uint8Array] => [i, blocks(`Slot ${i + 1}`, i)]),
    ),
    armBank: vi.fn(async (): Promise<ArmedBank> => ({ backup_path: 'C:/b/1-user-bank.syx', names: [] })),
    disarmBank: vi.fn(async () => undefined),
    commitBank: vi.fn(
      async (patches: Array<[number, Uint8Array]>): Promise<BankSaveReport> => ({
        written: patches.map(([s]: [number, Uint8Array]) => s),
        stopped_at: null,
        cancelled: false,
      }),
    ),
    bankToSyx: vi.fn(async () => new Uint8Array([0xf0, 0xf7])),
    cancelOperation: vi.fn(async () => undefined),
    onProgress: vi.fn(async (listener: (p: Progress) => void) => {
      listener({ what: 'user patches', done: 1, total: 2 });
      return () => undefined;
    }),
    ...overrides,
  };
}

describe('BankService', () => {
  it('starts empty and reads all 128 slots from the synth', async () => {
    const b = new BankService(fake());
    expect(b.slots.every((s) => s.bytes === null)).toBe(true);
    expect(await b.readFromSynth()).toBe(true);
    expect(b.slots).toHaveLength(SLOTS);
    expect(b.slots[0]?.name).toBe('Slot 1');
    expect(b.slots[127]?.origin).toBe('synth');
    expect(b.source).toBe('Instrument');
    expect(b.hasOriginal).toBe(true);
    expect(b.changed).toEqual([]);
    expect(b.phase).toBe('idle');
    expect(b.progress).toBeNull(); // progress shows only while working
  });

  it('backs up all memory without touching the grid, and shows progress only while it runs', async () => {
    const file = new Uint8Array([1, 2, 3]);
    const backupAll = vi.fn(async () => file);
    const b = new BankService(fake({ backupAll }));
    const got = await b.backupEverything();
    expect(got).toBe(file);
    expect(backupAll).toHaveBeenCalledTimes(1);
    expect(b.slots.every((slot) => slot.bytes === null)).toBe(true);
    expect(b.phase).toBe('idle');
    expect(b.progress).toBeNull();
    expect(b.error).toBe('');
  });

  it('a failed backup says why, hands back nothing and leaves the grid alone', async () => {
    const b = new BankService(fake({ backupAll: vi.fn(async () => Promise.reject('User patch 5 could not be read')) }));
    expect(await b.backupEverything()).toBeNull();
    expect(b.phase).toBe('failed');
    expect(b.error).toContain('User patch 5');
    expect(b.slots.every((slot) => slot.bytes === null)).toBe(true);
  });

  it('cannot start a second backup while one is running', async () => {
    let release: (v: Uint8Array) => void = () => {};
    const backupAll = vi.fn(() => new Promise<Uint8Array>((r) => (release = r)));
    const b = new BankService(fake({ backupAll }));
    const first = b.backupEverything();
    expect(await b.backupEverything()).toBeNull();
    await vi.waitFor(() => expect(backupAll).toHaveBeenCalledTimes(1));
    release(new Uint8Array([9]));
    expect(await first).toEqual(new Uint8Array([9]));
  });

  it('files every patch of the grid into a Library folder, counting new and repeated ones', async () => {
    let next = 0;
    const libraryAdd = vi.fn(async (_bytes: Uint8Array, _source: string): Promise<[number, boolean]> => {
      next += 1;
      return [next, next % 4 !== 0]; // every fourth patch is already in the library
    });
    const libraryTagAdd = vi.fn(async () => undefined);
    const b = new BankService(fake({ readBank: vi.fn(async () => [[0, blocks('A')], [1, blocks('B')], [2, blocks('C')], [3, blocks('D')]] as Array<[number, Uint8Array]>), libraryAdd, libraryTagAdd }));
    await b.readFromSynth();
    expect(await b.addAllToLibrary('  Synth / 2026-11-04 ')).toBe(4);
    expect(libraryAdd).toHaveBeenCalledTimes(4);
    expect(libraryAdd.mock.calls[0]?.[1]).toBe('Instrument');
    expect(libraryTagAdd).toHaveBeenCalledTimes(4);
    expect(libraryTagAdd).toHaveBeenNthCalledWith(1, 1, 'Synth/2026-11-04');
    // The patch that was already there is filed too: it is the folder that matters.
    expect(libraryTagAdd).toHaveBeenNthCalledWith(4, 4, 'Synth/2026-11-04');
    expect(b.libraryNotice).toContain('4 patches filed');
    expect(b.libraryNotice).toContain('3 new, 1 already there');
    expect(b.error).toBe('');
  });

  it('refuses an empty grid and an empty folder name without calling the library', async () => {
    const libraryAdd = vi.fn();
    const b = new BankService(fake({ libraryAdd }));
    expect(await b.addAllToLibrary('Synth')).toBeNull();
    expect(b.error).toContain('empty');
    await b.loadFile(BANK, 'bank.syx', [[0, 'x']]);
    expect(await b.addAllToLibrary(' / ')).toBeNull();
    expect(b.error).toContain('name');
    expect(libraryAdd).not.toHaveBeenCalled();
  });

  it('stops at the first failure and says how far it got', async () => {
    let calls = 0;
    const libraryAdd = vi.fn(async (): Promise<[number, boolean]> => {
      calls += 1;
      if (calls === 3) throw 'the library is locked';
      return [calls, true];
    });
    const b = new BankService(fake({ readBank: vi.fn(async () => [[0, blocks('A')], [1, blocks('B')], [2, blocks('C')], [3, blocks('D')]] as Array<[number, Uint8Array]>), libraryAdd, libraryTagAdd: vi.fn(async () => undefined) }));
    await b.readFromSynth();
    expect(await b.addAllToLibrary('Synth')).toBe(2);
    expect(b.error).toContain('locked');
    expect(b.error).toContain('2 of 4 filed');
    expect(b.libraryNotice).toBe('');
  });

  it('a failed read says why and changes nothing', async () => {
    const b = new BankService(fake({ readBank: vi.fn(async () => Promise.reject('no reply')) }));
    expect(await b.readFromSynth()).toBe(false);
    expect(b.phase).toBe('failed');
    expect(b.error).toBe('no reply');
    expect(b.slots.every((s) => s.bytes === null)).toBe(true);
  });

  it('assigning marks a slot changed, assigning the same patch back clears it, and bad data is refused', async () => {
    const b = new BankService(fake());
    await b.readFromSynth();
    expect(b.assign(5, blocks('Mine', 99), 'library')).toBe(true);
    expect(b.changed).toEqual([5]);
    expect(b.slots[5]?.name).toBe('Mine');
    expect(b.assign(5, blocks('Slot 6', 5), 'editor')).toBe(true);
    expect(b.changed).toEqual([]);
    expect(b.assign(5, new Uint8Array(10), 'file')).toBe(false);
    expect(b.error).not.toBe('');
    expect(b.assign(200, blocks('X'), 'file')).toBe(false);
  });

  it('swaps two slots', async () => {
    const b = new BankService(fake());
    await b.readFromSynth();
    b.swap(0, 1);
    expect(b.slots[0]?.name).toBe('Slot 2');
    expect(b.slots[1]?.name).toBe('Slot 1');
    expect(b.changed).toEqual([0, 1]);
    b.swap(3, 3);
    b.swap(-1, 4);
    b.swap(4, 500);
    expect(b.slots[3]?.name).toBe('Slot 4');
  });

  it('loads a real bank file into the slots', () => {
    const b = new BankService(fake());
    const list = JSON.parse(syxNames(BANK)) as Array<[number, string]>;
    expect(b.loadFile(BANK, 'bank.syx', list)).toBe(128);
    expect(b.slots[0]?.name).toBe(list[0]?.[1]);
    expect(b.source).toBe('bank.syx');
    expect(b.changed).toHaveLength(128); // never read from the synth: every slot is a change
  });

  it('writing needs a change, then arming (backup), then one commit of only the changed slots', async () => {
    const backend = fake();
    const b = new BankService(backend);
    await b.readFromSynth();
    expect(await b.arm()).toBe(false);
    expect(b.error).toContain('Nothing to write');
    b.reset();
    b.assign(7, blocks('New Seven', 70), 'library');
    b.assign(9, blocks('New Nine', 90), 'library');
    expect(await b.arm()).toBe(true);
    expect(b.phase).toBe('armed');
    expect(b.armed?.backup_path).toContain('user-bank');
    expect(await b.commit()).toBe(true);
    expect(b.phase).toBe('done');
    const sent = (backend.commitBank as ReturnType<typeof vi.fn>).mock.calls[0]?.[0] as Array<[number, Uint8Array]>;
    expect(sent.map(([s]) => s)).toEqual([7, 9]);
    expect(b.changed).toEqual([]); // what was written is now what the synth holds
    expect(await b.commit()).toBe(false); // the arming is used up
  });

  it('a write that stops reports the slot and keeps the unwritten changes', async () => {
    const backend = fake({
      commitBank: vi.fn(
        async (): Promise<BankSaveReport> => ({ written: [7], stopped_at: [9, 'read-back differs'], cancelled: false }),
      ),
    });
    const b = new BankService(backend);
    await b.readFromSynth();
    b.assign(7, blocks('A', 1), 'file');
    b.assign(9, blocks('B', 2), 'file');
    b.assign(11, blocks('C', 3), 'file');
    await b.arm();
    expect(await b.commit()).toBe(false);
    expect(b.phase).toBe('failed');
    expect(b.error).toContain('slot 10');
    expect(b.changed).toEqual([9, 11]);
  });

  it('cancelling reports how far it got', async () => {
    const backend = fake({
      commitBank: vi.fn(async (): Promise<BankSaveReport> => ({ written: [1, 2], stopped_at: null, cancelled: true })),
    });
    const b = new BankService(backend);
    await b.readFromSynth();
    for (const s of [1, 2, 3]) b.assign(s, blocks(`N${s}`, s), 'file');
    await b.arm();
    expect(await b.commit()).toBe(false);
    expect(b.error).toContain('Cancelled after 2 of 3');
  });

  it('cannot commit without arming, and walking away disarms', async () => {
    const backend = fake();
    const b = new BankService(backend);
    await b.readFromSynth();
    b.assign(1, blocks('X', 1), 'file');
    expect(await b.commit()).toBe(false);
    await b.arm();
    await b.cancelArming();
    expect(b.phase).toBe('idle');
    expect(backend.disarmBank).toHaveBeenCalledTimes(1);
    expect(await b.commit()).toBe(false);
    expect(backend.commitBank).not.toHaveBeenCalled();
  });

  it('a failed backup stops the write before anything is armed', async () => {
    const backend = fake({ armBank: vi.fn(async () => Promise.reject('could not write the backup file')) });
    const b = new BankService(backend);
    await b.readFromSynth();
    b.assign(1, blocks('X', 1), 'file');
    expect(await b.arm()).toBe(false);
    expect(b.phase).toBe('failed');
    expect(b.armed).toBeNull();
    expect(await b.commit()).toBe(false);
  });

  it('nothing can be edited while an operation is running, and cancel is passed on', async () => {
    let release: (v: Array<[number, Uint8Array]>) => void = () => undefined;
    const backend = fake({ readBank: vi.fn(() => new Promise<Array<[number, Uint8Array]>>((r) => (release = r))) });
    const b = new BankService(backend);
    const reading = b.readFromSynth();
    expect(b.working).toBe(true);
    expect(b.assign(0, blocks('X'), 'file')).toBe(false);
    expect(await b.readFromSynth()).toBe(false);
    await b.cancel();
    expect(backend.cancelOperation).toHaveBeenCalledTimes(1);
    release([]);
    await reading;
    expect(b.working).toBe(false);
  });

  it('exports the filled slots as a bank file, and refuses an empty bank', async () => {
    const backend = fake();
    const b = new BankService(backend);
    expect(await b.toSyx(0x10)).toBeNull();
    expect(b.error).toContain('empty');
    await b.readFromSynth();
    expect(await b.toSyx(0x10)).toEqual(new Uint8Array([0xf0, 0xf7]));
    const arg = (backend.bankToSyx as ReturnType<typeof vi.fn>).mock.calls[0]?.[0] as unknown[];
    expect(arg).toHaveLength(128);
  });

  it('arming passes exactly the changed slots, and the write sends those patches, not later edits', async () => {
    const backend = fake();
    const b = new BankService(backend);
    await b.readFromSynth();
    b.assign(7, blocks('Seven', 70), 'file');
    b.assign(9, blocks('Nine', 90), 'file');
    await b.arm();
    expect(backend.armBank).toHaveBeenCalledWith([7, 9]);
    expect(b.confirmedSlots).toEqual([7, 9]);
    // Everything is locked while armed.
    expect(b.locked).toBe(true);
    expect(b.assign(11, blocks('Eleven', 1), 'file')).toBe(false);
    expect(b.assign(7, blocks('Late edit', 5), 'file')).toBe(false);
    b.swap(7, 9);
    expect(b.slots[7]?.name).toBe('Seven');
    expect(b.loadFile(BANK, 'x.syx', [[0, 'x']])).toBe(0);
    await b.commit();
    const sent = (backend.commitBank as ReturnType<typeof vi.fn>).mock.calls[0]?.[0] as Array<[number, Uint8Array]>;
    expect(sent.map(([s2]) => s2)).toEqual([7, 9]);
    const doc = PatchDoc.fromBlocks(sent[0]![1]);
    expect(doc.name()).toBe('Seven');
    doc.free();
  });

  it('reading the bank again while armed disarms first', async () => {
    const backend = fake();
    const b = new BankService(backend);
    await b.readFromSynth();
    b.assign(1, blocks('X', 1), 'file');
    await b.arm();
    await b.readFromSynth();
    expect(backend.disarmBank).toHaveBeenCalledTimes(1);
    expect(b.phase).toBe('idle');
    expect(b.armed).toBeNull();
  });

  it('closing during the backup disarms as soon as arming finishes', async () => {
    let release: (a: ArmedBank) => void = () => undefined;
    const backend = fake({ armBank: vi.fn(() => new Promise<ArmedBank>((r) => (release = r))) });
    const b = new BankService(backend);
    await b.readFromSynth();
    b.assign(1, blocks('X', 1), 'file');
    const arming = b.arm();
    await b.cancelArming();
    await vi.waitFor(() => expect(backend.armBank).toHaveBeenCalled());
    release({ backup_path: 'p', names: [] });
    await arming;
    expect(b.phase).toBe('idle');
    expect(backend.disarmBank).toHaveBeenCalledTimes(1);
    expect(await b.commit()).toBe(false);
  });

  it('after a write that threw, what the synth holds is unknown and the message says to read it again', async () => {
    const backend = fake({ commitBank: vi.fn(async () => Promise.reject('cable unplugged')) });
    const b = new BankService(backend);
    await b.readFromSynth();
    b.assign(2, blocks('Two', 2), 'file');
    await b.arm();
    expect(await b.commit()).toBe(false);
    expect(b.error).toContain('cable unplugged');
    expect(b.error).toContain('Read the bank');
  });
});
