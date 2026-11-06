import { describe, expect, it, vi } from 'vitest';
import type { ArmedSave, Backend, LibraryRow, SaveReport } from './backend';
import { LibraryService } from './library.svelte';
import { SaveFlow } from './save.svelte';

function row(id: number, name: string, extra: Partial<LibraryRow> = {}): LibraryRow {
  return { id, name, tags: [], favourite: false, source: 't', added_at: 1, ...extra };
}

function fakeBackend(overrides: Partial<Backend> = {}): Backend {
  const never = (): never => {
    throw new Error('not expected');
  };
  return {
    libraryImport: vi.fn(async () => ({ added: 2, duplicates: 1 })),
    libraryList: vi.fn(async () => [row(1, 'Alpha'), row(2, 'Beta')]),
    libraryCount: vi.fn(async () => 2),
    libraryTags: vi.fn(async () => [['pad', 2]] as Array<[string, number]>),
    libraryGet: vi.fn(async () => new Uint8Array(590)),
    libraryAdd: vi.fn(async () => [3, true] as [number, boolean]),
    libraryFavourite: vi.fn(async () => undefined),
    libraryTagAdd: vi.fn(async () => undefined),
    libraryTagRemove: vi.fn(async () => undefined),
    libraryRename: vi.fn(async () => undefined),
    libraryDelete: vi.fn(async () => undefined),
    libraryExport: vi.fn(async () => new Uint8Array([1, 2, 3])),
    backupsList: vi.fn(),
    backupRead: vi.fn(),
    backupAll: vi.fn(),
    storageInfo: vi.fn(),
    storageSet: vi.fn(),
    armSave: vi.fn(never),
    disarmSave: vi.fn(never),
    commitSave: vi.fn(never),
    restoreSave: vi.fn(never),
    armSystemSave: vi.fn(never),
    disarmSystemSave: vi.fn(never),
    commitSystemSave: vi.fn(never),
    restoreSystemSave: vi.fn(never),
    saveFile: vi.fn(never),
    readBank: vi.fn(never),
    armBank: vi.fn(never),
    disarmBank: vi.fn(never),
    commitBank: vi.fn(never),
    bankToSyx: vi.fn(never),
    cancelOperation: vi.fn(never),
    onProgress: vi.fn(async () => () => undefined),
    ...overrides,
  };
}

describe('LibraryService', () => {
  it('loads rows, the total and the tags', async () => {
    const lib = new LibraryService(fakeBackend());
    await lib.refresh();
    expect(lib.rows.map((r) => r.name)).toEqual(['Alpha', 'Beta']);
    expect(lib.total).toBe(2);
    expect(lib.tags).toEqual([['pad', 2]]);
    expect(lib.error).toBe('');
  });

  it('passes the search, tag and favourites filters to the backend', async () => {
    const b = fakeBackend();
    const lib = new LibraryService(b);
    lib.text = '  warm ';
    lib.tag = 'pad';
    lib.favouritesOnly = true;
    await lib.refresh();
    expect(b.libraryList).toHaveBeenCalledWith({ text: 'warm', tag: 'pad', favouritesOnly: true, limit: 200, offset: 0 });
  });

  it('only the newest refresh may update the lists', async () => {
    let release: (rows: LibraryRow[]) => void = () => undefined;
    const slow = new Promise<LibraryRow[]>((r) => (release = r));
    const calls: number[] = [];
    const b = fakeBackend({
      libraryList: vi.fn(async () => {
        calls.push(calls.length);
        return calls.length === 1 ? slow : [row(9, 'Fresh')];
      }),
    });
    const lib = new LibraryService(b);
    const first = lib.refresh();
    await lib.refresh();
    release([row(1, 'Stale')]);
    await first;
    expect(lib.rows.map((r) => r.name)).toEqual(['Fresh']);
  });

  it('reports a backend failure and keeps the old rows', async () => {
    let fail = false;
    const b = fakeBackend({
      libraryList: vi.fn(async () => {
        if (fail) throw 'the library is unavailable: disk full';
        return [row(1, 'Alpha')];
      }),
    });
    const lib = new LibraryService(b);
    await lib.refresh();
    fail = true;
    await lib.refresh();
    expect(lib.error).toContain('disk full');
    expect(lib.rows).toHaveLength(1);
  });

  it('import reports added and duplicate counts, then refreshes', async () => {
    const b = fakeBackend();
    const lib = new LibraryService(b);
    const r = await lib.importFile('bank.syx', new Uint8Array([1]));
    expect(r).toEqual({ added: 2, duplicates: 1 });
    expect(lib.notice).toBe('Imported 2 new patches; 1 already in the library.');
    expect(b.libraryList).toHaveBeenCalled();
    expect(lib.busy).toBe(false);
  });

  it('a failed import says why and leaves nothing busy', async () => {
    const b = fakeBackend({ libraryImport: vi.fn(async () => Promise.reject('the file holds no patches')) });
    const lib = new LibraryService(b);
    expect(await lib.importFile('x.syx', new Uint8Array())).toBeNull();
    expect(lib.error).toBe('the file holds no patches');
    expect(lib.busy).toBe(false);
  });

  it('edits go through the backend and the list is refreshed', async () => {
    const b = fakeBackend();
    const lib = new LibraryService(b);
    await lib.favourite(1, true);
    await lib.addTag(1, 'pad');
    await lib.removeTag(1, 'pad');
    await lib.rename(1, 'New');
    await lib.remove(2);
    expect(b.libraryFavourite).toHaveBeenCalledWith(1, true);
    expect(b.libraryTagAdd).toHaveBeenCalledWith(1, 'pad');
    expect(b.libraryTagRemove).toHaveBeenCalledWith(1, 'pad');
    expect(b.libraryRename).toHaveBeenCalledWith(1, 'New');
    expect(b.libraryDelete).toHaveBeenCalledWith(2);
    expect(b.libraryList).toHaveBeenCalledTimes(5);
  });

  it('a selection that disappears is cleared', async () => {
    const lib = new LibraryService(fakeBackend());
    await lib.refresh();
    lib.selectedId = 2;
    expect(lib.selected?.name).toBe('Beta');
    lib.selectedId = 99;
    await lib.refresh();
    expect(lib.selectedId).toBeNull();
  });

  it('export sends at most 128 visible ids and refuses an empty list', async () => {
    const b = fakeBackend();
    const lib = new LibraryService(b);
    expect(await lib.exportVisible(0x10)).toBeNull();
    expect(lib.error).toContain('Nothing to export');
    lib.rows = Array.from({ length: 150 }, (_, i) => row(i + 1, `P${i}`));
    const out = await lib.exportVisible(0x10);
    expect(out).toEqual(new Uint8Array([1, 2, 3]));
    const ids = (b.libraryExport as ReturnType<typeof vi.fn>).mock.calls[0]?.[0] as number[];
    expect(ids).toHaveLength(128);
  });
});

const patch = new Uint8Array(590).fill(7);
const armed = (slot: number): ArmedSave => ({ slot, occupant_name: 'Old Sound', backup_path: 'C:/b/1-user.syx' });
const report = (slot: number, verified = true): SaveReport => ({ slot, messages: 9, bytes: 590, verified });

describe('SaveFlow', () => {
  it('walks idle, armed, writing, done and keeps one undo', async () => {
    const b = fakeBackend({
      armSave: vi.fn(async (slot: number) => armed(slot)),
      commitSave: vi.fn(async () => report(5)),
      restoreSave: vi.fn(async () => report(5)),
    });
    const f = new SaveFlow(b);
    expect(f.phase).toBe('idle');
    expect(await f.arm(5, patch, 'New')).toBe(true);
    expect(f.phase).toBe('armed');
    expect(f.slot).toBe(5);
    expect(f.armed?.occupant_name).toBe('Old Sound');
    expect(await f.commit()).toBe(true);
    expect(f.phase).toBe('done');
    expect(f.armed).toBeNull();
    expect(f.canUndo).toBe(true);
    expect(f.last).toEqual({ slot: 5, backupPath: 'C:/b/1-user.syx', oldName: 'Old Sound' });
    expect(await f.undo()).toBe(true);
    expect(f.canUndo).toBe(false);
    expect(b.restoreSave).toHaveBeenCalledTimes(1);
  });

  it('cannot write without arming, or twice for one arming', async () => {
    const b = fakeBackend({
      armSave: vi.fn(async (slot: number) => armed(slot)),
      commitSave: vi.fn(async () => report(0)),
    });
    const f = new SaveFlow(b);
    expect(await f.commit()).toBe(false);
    expect(b.commitSave).not.toHaveBeenCalled();
    await f.arm(0, patch, 'New');
    await f.commit();
    expect(await f.commit()).toBe(false);
    expect(b.commitSave).toHaveBeenCalledTimes(1);
  });

  it('refuses bad slots without touching the backend', async () => {
    const b = fakeBackend();
    const f = new SaveFlow(b);
    for (const bad of [-1, 128, 1.5, Number.NaN]) {
      expect(await f.arm(bad, patch, 'New')).toBe(false);
      expect(f.phase).toBe('failed');
    }
    expect(b.armSave).not.toHaveBeenCalled();
  });

  it('a failed backup stops the save before anything is armed', async () => {
    const b = fakeBackend({ armSave: vi.fn(async () => Promise.reject('could not write the backup file (access denied)')) });
    const f = new SaveFlow(b);
    expect(await f.arm(3, patch, 'New')).toBe(false);
    expect(f.phase).toBe('failed');
    expect(f.error).toContain('backup');
    expect(f.armed).toBeNull();
    expect(await f.commit()).toBe(false);
  });

  it('cancelling an armed save disarms it', async () => {
    const b = fakeBackend({
      armSave: vi.fn(async (slot: number) => armed(slot)),
      disarmSave: vi.fn(async () => undefined),
    });
    const f = new SaveFlow(b);
    await f.arm(2, patch, 'New');
    await f.cancel();
    expect(f.phase).toBe('idle');
    expect(b.disarmSave).toHaveBeenCalledTimes(1);
    expect(await f.commit()).toBe(false);
  });

  it('a mismatch after the write is reported and can be undone', async () => {
    const b = fakeBackend({
      armSave: vi.fn(async (slot: number) => armed(slot)),
      commitSave: vi.fn(async () => Promise.reject('read-back differs from what was sent in: common')),
      restoreSave: vi.fn(async () => report(1)),
    });
    const f = new SaveFlow(b);
    await f.arm(1, patch, 'New');
    expect(await f.commit()).toBe(false);
    expect(f.phase).toBe('failed');
    expect(f.error).toContain('read-back differs');
    expect(f.canUndo).toBe(true);
    expect(await f.undo()).toBe(true);
  });

  it('cannot start a second arming while one is in progress', async () => {
    let release: (a: ArmedSave) => void = () => undefined;
    const b = fakeBackend({ armSave: vi.fn(() => new Promise<ArmedSave>((r) => (release = r))) });
    const f = new SaveFlow(b);
    const first = f.arm(1, patch, 'New');
    expect(await f.arm(2, patch, 'New')).toBe(false);
    release(armed(1));
    await first;
    expect(b.armSave).toHaveBeenCalledTimes(1);
    expect(f.slot).toBe(1);
  });

  it('writes the patch frozen at arming, not whatever the editor holds later', async () => {
    const b = fakeBackend({
      armSave: vi.fn(async (slot: number) => armed(slot)),
      commitSave: vi.fn(async () => report(2)),
    });
    const f = new SaveFlow(b);
    const mine = new Uint8Array(590).fill(1);
    await f.arm(2, mine, 'Mine');
    mine.fill(9); // the editor changes afterwards
    await f.commit();
    const sent = (b.commitSave as ReturnType<typeof vi.fn>).mock.calls[0]?.[0] as Uint8Array;
    expect(sent[0]).toBe(1);
    expect(f.frozenName).toBe('Mine');
  });

  it('a stray second commit during a write changes nothing', async () => {
    let release: (r: SaveReport) => void = () => undefined;
    const b = fakeBackend({
      armSave: vi.fn(async (slot: number) => armed(slot)),
      commitSave: vi.fn(() => new Promise<SaveReport>((r) => (release = r))),
    });
    const f = new SaveFlow(b);
    await f.arm(1, patch, 'N');
    const first = f.commit();
    expect(f.phase).toBe('writing');
    expect(await f.commit()).toBe(false);
    expect(f.phase).toBe('writing');
    expect(await f.arm(3, patch, 'N')).toBe(false);
    release(report(1));
    await first;
    expect(f.phase).toBe('done');
    expect(b.commitSave).toHaveBeenCalledTimes(1);
  });

  it('an arming older than a minute is refused and disarmed', async () => {
    let t = 0;
    const b = fakeBackend({
      armSave: vi.fn(async (slot: number) => armed(slot)),
      disarmSave: vi.fn(async () => undefined),
      commitSave: vi.fn(async () => report(1)),
    });
    const f = new SaveFlow(b, () => t);
    await f.arm(1, patch, 'N');
    t = 61_000;
    expect(await f.commit()).toBe(false);
    expect(f.error).toContain('expired');
    expect(b.commitSave).not.toHaveBeenCalled();
    expect(b.disarmSave).toHaveBeenCalled();
  });

  it('closing while the slot is still being read disarms as soon as it finishes', async () => {
    let release: (a: ArmedSave) => void = () => undefined;
    const b = fakeBackend({
      armSave: vi.fn(() => new Promise<ArmedSave>((r) => (release = r))),
      disarmSave: vi.fn(async () => undefined),
    });
    const f = new SaveFlow(b);
    const arming = f.arm(4, patch, 'N');
    await f.cancel();
    release(armed(4));
    await arming;
    expect(f.phase).toBe('idle');
    expect(b.disarmSave).toHaveBeenCalledTimes(1);
  });

  it('a write that did not verify is a failure, never a success', async () => {
    const b = fakeBackend({
      armSave: vi.fn(async (slot: number) => armed(slot)),
      commitSave: vi.fn(async () => report(1, false)),
    });
    const f = new SaveFlow(b);
    await f.arm(1, patch, 'N');
    expect(await f.commit()).toBe(false);
    expect(f.phase).toBe('failed');
    expect(f.error).toContain('MISMATCH');
    expect(f.canUndo).toBe(true);
  });

  it('arming another slot keeps the undo of the earlier save', async () => {
    const b = fakeBackend({
      armSave: vi.fn(async (slot: number) => armed(slot)),
      disarmSave: vi.fn(async () => undefined),
      commitSave: vi.fn(async () => report(7)),
      restoreSave: vi.fn(async () => report(7)),
    });
    const f = new SaveFlow(b);
    await f.arm(7, patch, 'A');
    await f.commit();
    await f.arm(9, patch, 'B');
    await f.cancel();
    expect(f.last?.slot).toBe(7);
    expect(await f.undo()).toBe(true);
  });

  it('undo is refused while something is in progress and asks nothing of a stale state', async () => {
    const b = fakeBackend({ armSave: vi.fn(async (slot: number) => armed(slot)) });
    const f = new SaveFlow(b);
    expect(await f.undo()).toBe(false);
    await f.arm(1, patch, 'N');
    expect(await f.undo()).toBe(false);
  });
});

describe('LibraryService safety', () => {
  it('busy counts overlapping operations', async () => {
    let release: () => void = () => undefined;
    const slow = new Promise<void>((r) => (release = r));
    const b = fakeBackend({ libraryFavourite: vi.fn(() => slow) });
    const lib = new LibraryService(b);
    const first = lib.favourite(1, true);
    const second = lib.rename(1, 'X');
    expect(lib.busy).toBe(true);
    await second;
    expect(lib.busy).toBe(true); // the slow one is still running
    release();
    await first;
    expect(lib.busy).toBe(false);
  });

  it('a file over 16 MB is refused before it is sent', async () => {
    const b = fakeBackend();
    const lib = new LibraryService(b);
    expect(await lib.importFile('huge.syx', new Uint8Array(17 * 1_048_576))).toBeNull();
    expect(lib.error).toContain('larger than 16 MB');
    expect(b.libraryImport).not.toHaveBeenCalled();
  });

  it('export says when it had to stop at 128', async () => {
    const b = fakeBackend();
    const lib = new LibraryService(b);
    lib.rows = Array.from({ length: 150 }, (_, i) => row(i + 1, `P${i}`));
    await lib.exportVisible(0x10);
    expect(lib.notice).toContain('first 128 of 150');
  });
});
