// Test support: an in-memory stand-in for the bulk part of the backend, so the library service and the screens built on it can be tested without Rust.
// It keeps the same rules the Rust side has: patches are stored once, a bulk is positions of patch ids, a save over a bulk keeps a version.

import { vi } from 'vitest';
import type { Backend, BulkHit, BulkRevision, BulkRow, BulkSaveResult, BulkSlot } from '../model/backend';

export interface FakeBulks {
  /** The stored patches: id to bytes and name. */
  patches: Map<number, { bytes: Uint8Array; name: string }>;
  bulks: Map<number, { name: string; slots: Map<number, number>; revisions: Array<{ id: number; slots: Array<[number, number]> }> }>;
  calls: string[];
  api: Pick<
    Backend,
    | 'bulkList'
    | 'bulkContents'
    | 'bulkBlocks'
    | 'bulkSearch'
    | 'bulkRevisions'
    | 'bulkSave'
    | 'bulkRename'
    | 'bulkDuplicate'
    | 'bulkDelete'
    | 'bulkPlace'
    | 'bulkAppend'
    | 'bulkClear'
    | 'bulkSwap'
    | 'bulkMove'
    | 'bulkRestore'
    | 'bulkExport'
    | 'libraryGet'
    | 'libraryAdd'
  >;
  /** Put a patch in the store and return its id. */
  addPatch(name: string, fill?: number): number;
  addBulk(name: string, ids: number[]): number;
}

export function fakeBulks(): FakeBulks {
  const patches = new Map<number, { bytes: Uint8Array; name: string }>();
  const bulks: FakeBulks['bulks'] = new Map();
  const calls: string[] = [];
  let nextPatch = 1;
  let nextBulk = 1;
  let nextRev = 1;

  const find = (bytes: Uint8Array): number | undefined =>
    [...patches].find(([, p]) => p.bytes.length === bytes.length && p.bytes.every((v, i) => v === bytes[i]))?.[0];
  const need = (id: number) => {
    const b = bulks.get(id);
    if (!b) throw new Error(`no bulk with id ${id}`);
    return b;
  };
  const nameTaken = (name: string, except?: number): boolean => [...bulks].some(([id, b]) => id !== except && b.name.toLowerCase() === name.toLowerCase());
  const keep = (id: number): boolean => {
    const b = need(id);
    if (b.slots.size === 0) return false;
    b.revisions.unshift({ id: nextRev++, slots: [...b.slots].sort((x, y) => x[0] - y[0]) });
    return true;
  };

  const addPatch = (name: string, fill = nextPatch): number => {
    const id = nextPatch++;
    patches.set(id, { bytes: new Uint8Array(590).fill(fill % 128), name });
    return id;
  };

  const api: FakeBulks['api'] = {
    bulkList: vi.fn(async (): Promise<BulkRow[]> =>
      [...bulks].map(([id, b]) => ({ id, name: b.name, source: '', created_at: 1, updated_at: 1, count: b.slots.size })).sort((a, b) => a.name.localeCompare(b.name)),
    ),
    bulkContents: vi.fn(async (id: number): Promise<BulkSlot[]> =>
      [...need(id).slots].sort((x, y) => x[0] - y[0]).map(([position, patch_id]) => ({ position, patch_id, name: patches.get(patch_id)?.name ?? '?' })),
    ),
    bulkBlocks: vi.fn(async (id: number) =>
      [...need(id).slots].sort((x, y) => x[0] - y[0]).map(([p, pid]): [number, Uint8Array] => [p, patches.get(pid)!.bytes]),
    ),
    bulkSearch: vi.fn(async (text: string): Promise<BulkHit[]> =>
      [...bulks].flatMap(([id, b]) =>
        [...b.slots]
          .filter(([, pid]) => patches.get(pid)?.name.toLowerCase().includes(text.toLowerCase()))
          .map(([position, patch_id]) => ({ bulk_id: id, bulk_name: b.name, position, patch_id, name: patches.get(patch_id)!.name })),
      ),
    ),
    bulkRevisions: vi.fn(async (id: number): Promise<BulkRevision[]> => need(id).revisions.map((r) => ({ id: r.id, saved_at: 1, count: r.slots.length }))),
    bulkSave: vi.fn(async (overwrite: number | null, name: string, list: Array<[number, Uint8Array]>): Promise<BulkSaveResult> => {
      calls.push(`save ${overwrite ?? 'new'} ${name}`);
      let added = 0;
      let reused = 0;
      const slots = new Map<number, number>();
      for (const [slot, bytes] of list) {
        let id = find(bytes);
        if (id === undefined) {
          id = addPatch(`P${nextPatch}`);
          patches.get(id)!.bytes = bytes.slice();
          added += 1;
        } else reused += 1;
        slots.set(slot, id);
      }
      if (overwrite === null) {
        if (nameTaken(name)) throw new Error(`a bulk called "${name}" already exists`);
        const id = nextBulk++;
        bulks.set(id, { name, slots, revisions: [] });
        return { bulk_id: id, name, added, reused, kept_version: false };
      }
      const b = need(overwrite);
      if (nameTaken(name, overwrite)) throw new Error(`a bulk called "${name}" already exists`);
      const kept = keep(overwrite);
      b.slots = slots;
      b.name = name;
      return { bulk_id: overwrite, name, added, reused, kept_version: kept };
    }),
    bulkRename: vi.fn(async (id: number, name: string) => {
      if (nameTaken(name, id)) throw new Error(`a bulk called "${name}" already exists`);
      need(id).name = name;
    }),
    bulkDuplicate: vi.fn(async (id: number, name: string) => {
      if (nameTaken(name)) throw new Error(`a bulk called "${name}" already exists`);
      const n = nextBulk++;
      bulks.set(n, { name, slots: new Map(need(id).slots), revisions: [] });
      return n;
    }),
    bulkDelete: vi.fn(async (id: number) => {
      calls.push(`delete ${id}`);
      need(id);
      bulks.delete(id);
      return 0;
    }),
    bulkPlace: vi.fn(async (bulk: number, position: number, patchId: number) => {
      calls.push(`place ${bulk} ${position} ${patchId}`);
      need(bulk).slots.set(position, patchId);
    }),
    bulkAppend: vi.fn(async (bulk: number, patchId: number) => {
      calls.push(`append ${bulk} ${patchId}`);
      const b = need(bulk);
      let p = 0;
      while (b.slots.has(p)) p += 1;
      b.slots.set(p, patchId);
      return p;
    }),
    bulkClear: vi.fn(async (bulk: number, position: number) => {
      calls.push(`clear ${bulk} ${position}`);
      need(bulk).slots.delete(position);
    }),
    bulkSwap: vi.fn(async (bulk: number, a: number, b: number) => {
      const s = need(bulk).slots;
      const [pa, pb] = [s.get(a), s.get(b)];
      s.delete(a);
      s.delete(b);
      if (pb !== undefined) s.set(a, pb);
      if (pa !== undefined) s.set(b, pa);
    }),
    bulkMove: vi.fn(async (fromBulk: number, fromPosition: number, toBulk: number, toPosition: number) => {
      calls.push(`move ${fromBulk}.${fromPosition} ${toBulk}.${toPosition}`);
      const from = need(fromBulk).slots;
      const to = need(toBulk).slots;
      const moving = from.get(fromPosition);
      if (moving === undefined) throw new Error('nothing there');
      const displaced = to.get(toPosition);
      to.set(toPosition, moving);
      if (displaced === undefined) from.delete(fromPosition);
      else from.set(fromPosition, displaced);
    }),
    bulkRestore: vi.fn(async (bulk: number, revision: number) => {
      const b = need(bulk);
      const r = b.revisions.find((x) => x.id === revision);
      if (!r) throw new Error(`no patch with id ${revision}`);
      keep(bulk);
      b.slots = new Map(r.slots);
    }),
    bulkExport: vi.fn(async () => new Uint8Array([0xf0, 0xf7])),
    libraryGet: vi.fn(async (id: number) => {
      const p = patches.get(id);
      if (!p) throw new Error(`no patch with id ${id}`);
      return p.bytes;
    }),
    libraryAdd: vi.fn(async (bytes: Uint8Array): Promise<[number, boolean]> => {
      const existing = find(bytes);
      if (existing !== undefined) return [existing, false];
      const id = addPatch(`P${nextPatch}`);
      patches.get(id)!.bytes = bytes.slice();
      return [id, true];
    }),
  };

  return {
    patches,
    bulks,
    calls,
    api,
    addPatch,
    addBulk: (name, ids) => {
      const id = nextBulk++;
      bulks.set(id, { name, slots: new Map(ids.map((pid, i) => [i, pid])), revisions: [] });
      return id;
    },
  };
}
