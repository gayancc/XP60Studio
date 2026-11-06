// Bulks in the library: a bank file is kept as a named, ordered set of patches (a "bulk"), shown under its name and expanded to its
// patches. This holds what the screen shows (the bulks, which are open, search results, what is selected) and every change made to
// them: saving the bank as a bulk or over one, moving patches between bulks and the bank by drag and drop, versions. The work itself
// is done in Rust through the `Backend`; nothing here talks to the instrument.

import type { Backend, BulkHit, BulkRevision, BulkRow, BulkSlot } from './backend';
import type { BankService } from './bank.svelte';
import { reportError, reportInfo, reportWarning } from './messages.svelte';

function message(e: unknown): string {
  return typeof e === 'string' ? e : e instanceof Error ? e.message : String(e);
}

/** A patch being dragged, wherever it comes from. */
export type DragItem =
  | { kind: 'patch'; patchId: number }
  | { kind: 'bulk-slot'; bulk: number; position: number; patchId: number }
  | { kind: 'bank-slot'; slot: number };

/** What a drag carries: one or more patches, or a whole bulk. */
export type DragPayload = { type: 'patches'; items: DragItem[] } | { type: 'bulk'; bulk: number };

/** Where a drop lands in the library. */
export type DropTarget =
  | { kind: 'bulk-slot'; bulk: number; position: number }
  | { kind: 'bulk'; bulk: number }
  | { kind: 'new' };

/** The MIME type a library drag uses; the payload is JSON. (Single patches also set `text/xp-library` for older drop targets.) */
export const DRAG_TYPE = 'application/x-xp-drag';

export function parsePayload(text: string | undefined): DragPayload | null {
  if (!text) return null;
  try {
    const v = JSON.parse(text) as DragPayload;
    if (v?.type === 'bulk' && Number.isInteger(v.bulk)) return v;
    if (v?.type === 'patches' && Array.isArray(v.items) && v.items.length > 0) return v;
  } catch {
    /* not ours */
  }
  return null;
}

/** A file or bank name without its `.syx`/`.mid` ending, as the default name of a bulk. */
export function bulkNameFrom(source: string): string {
  const stem = source.replace(/\.(syx|mid|midi)$/i, '').trim();
  return stem === '' || stem === 'Empty' ? 'My bank' : stem.slice(0, 60);
}

export class BulkService {
  bulks = $state<BulkRow[]>([]);
  /** Bulks that are open (showing their patches). */
  open = $state<Record<number, boolean>>({});
  contents = $state<Record<number, BulkSlot[]>>({});
  revisions = $state<Record<number, BulkRevision[]>>({});
  /** Text typed in the search box, and what it found. */
  query = $state('');
  hits = $state<BulkHit[]>([]);
  /** Patches picked with ctrl or shift, to be dragged together. */
  selection = $state<Array<{ bulk: number; position: number; patchId: number }>>([]);
  /** The bulk whose name is being edited. */
  renaming = $state<number | null>(null);
  /** A bulk the user asked to load into the bank (button or drop on the grid), waiting for the "replace the bank?" answer. */
  pendingLoad = $state<number | null>(null);
  private inFlight = $state(0);
  private ticket = 0;

  constructor(
    private readonly backend: Backend,
    /** Called after anything changed, so the flat patch list refreshes too. */
    private readonly changed: () => void = () => undefined,
  ) {}

  get busy(): boolean {
    return this.inFlight > 0;
  }

  bulk(id: number): BulkRow | undefined {
    return this.bulks.find((b) => b.id === id);
  }

  private fail(e: unknown): void {
    reportError(message(e));
  }

  async refresh(): Promise<void> {
    const mine = ++this.ticket;
    try {
      const bulks = await this.backend.bulkList();
      const openIds = bulks.filter((b) => this.open[b.id]).map((b) => b.id);
      const loaded = await Promise.all(openIds.map(async (id) => [id, await this.backend.bulkContents(id)] as const));
      if (mine !== this.ticket) return;
      this.bulks = bulks;
      this.contents = Object.fromEntries(loaded);
      this.open = Object.fromEntries(Object.entries(this.open).filter(([id]) => bulks.some((b) => b.id === Number(id))));
      this.selection = this.selection.filter((s) => this.contents[s.bulk]?.some((c) => c.position === s.position && c.patch_id === s.patchId));
      if (this.query.trim() !== '') await this.search(this.query);
    } catch (e) {
      if (mine === this.ticket) this.fail(e);
    }
  }

  /** Run a change, then refresh what is shown. Resolves to the result, or null when it failed (the reason is shown). */
  private async run<T>(action: () => Promise<T>): Promise<T | null> {
    this.inFlight += 1;
    try {
      const result = await action();
      await this.refresh();
      this.changed();
      return result;
    } catch (e) {
      this.fail(e);
      return null;
    } finally {
      this.inFlight -= 1;
    }
  }

  async toggle(id: number): Promise<void> {
    if (this.open[id]) {
      this.open = { ...this.open, [id]: false };
      return;
    }
    try {
      this.contents = { ...this.contents, [id]: await this.backend.bulkContents(id) };
      this.open = { ...this.open, [id]: true };
    } catch (e) {
      this.fail(e);
    }
  }

  async search(text: string): Promise<void> {
    this.query = text;
    const q = text.trim();
    if (q === '') {
      this.hits = [];
      return;
    }
    const mine = ++this.ticket;
    try {
      const hits = await this.backend.bulkSearch(q, 500);
      if (mine === this.ticket) this.hits = hits;
    } catch (e) {
      this.fail(e);
    }
  }

  // ---- selection ----

  isSelected(bulk: number, position: number): boolean {
    return this.selection.some((s) => s.bulk === bulk && s.position === position);
  }

  toggleSelected(bulk: number, position: number, patchId: number): void {
    this.selection = this.isSelected(bulk, position)
      ? this.selection.filter((s) => !(s.bulk === bulk && s.position === position))
      : [...this.selection, { bulk, position, patchId }];
  }

  clearSelection(): void {
    this.selection = [];
  }

  // ---- bulks ----

  async rename(id: number, name: string): Promise<boolean> {
    const ok = (await this.run(() => this.backend.bulkRename(id, name).then(() => true))) === true;
    if (ok) this.renaming = null;
    return ok;
  }

  async duplicate(id: number): Promise<number | null> {
    const src = this.bulk(id);
    if (!src) return null;
    return this.run(() => this.backend.bulkDuplicate(id, this.freeName(`${src.name} copy`)));
  }

  /** `base`, or `base 2`, `base 3` ... the first name no bulk has (the library compares names ignoring case). */
  freeName(base: string): string {
    const taken = new Set(this.bulks.map((b) => b.name.toLowerCase()));
    if (!taken.has(base.toLowerCase())) return base;
    for (let n = 2; ; n++) {
      const candidate = `${base} ${n}`;
      if (!taken.has(candidate.toLowerCase())) return candidate;
    }
  }

  async remove(id: number): Promise<boolean> {
    const name = this.bulk(id)?.name ?? 'The bulk';
    const gone = await this.run(() => this.backend.bulkDelete(id));
    if (gone === null) return false;
    reportInfo(`${name} deleted${gone > 0 ? `, and ${gone} patch${gone === 1 ? '' : 'es'} only it held` : ''}.`);
    return true;
  }

  async exportBulk(id: number, device: number): Promise<Uint8Array | null> {
    try {
      return await this.backend.bulkExport(id, device);
    } catch (e) {
      this.fail(e);
      return null;
    }
  }

  // ---- the bank <-> the library ----

  /** The bank's patches as the library takes them: `[slot, 590 bytes]`. */
  private bankPatches(bank: BankService): Array<[number, Uint8Array]> {
    return bank.slots.flatMap((s, i): Array<[number, Uint8Array]> => (s.bytes ? [[i, s.bytes]] : []));
  }

  /** Save the bank as a new bulk (`overwrite` null) or over an existing one. The bank then belongs to that bulk and counts as saved. */
  async saveBank(bank: BankService, name: string, overwrite: number | null): Promise<boolean> {
    const patches = this.bankPatches(bank);
    if (patches.length === 0) {
      reportWarning('The bank is empty: there is nothing to save.');
      return false;
    }
    const r = await this.run(() => this.backend.bulkSave(overwrite, name, patches));
    if (!r) return false;
    bank.markSaved(r.bulk_id, r.name);
    this.open = { ...this.open, [r.bulk_id]: true };
    await this.refresh();
    reportInfo(
      `${overwrite === null ? 'Saved as' : 'Saved over'} "${r.name}": ${patches.length} patches${r.added > 0 ? `, ${r.added} new to the library` : ''}${
        r.kept_version ? '. The previous contents are kept as a version.' : '.'
      }`,
    );
    return true;
  }

  /** Replace the bank with a bulk (the bank's unsaved changes are lost, so callers confirm first). */
  async loadIntoBank(bank: BankService, id: number): Promise<boolean> {
    const name = this.bulk(id)?.name ?? 'bulk';
    try {
      const blocks = await this.backend.bulkBlocks(id);
      bank.loadFromBulk(id, name, blocks);
      return true;
    } catch (e) {
      this.fail(e);
      return false;
    }
  }

  // ---- versions ----

  async loadRevisions(id: number): Promise<void> {
    try {
      this.revisions = { ...this.revisions, [id]: await this.backend.bulkRevisions(id) };
    } catch (e) {
      this.fail(e);
    }
  }

  async restore(bulk: number, revision: number): Promise<boolean> {
    const ok = (await this.run(() => this.backend.bulkRestore(bulk, revision).then(() => true))) === true;
    if (ok) await this.loadRevisions(bulk);
    return ok;
  }

  // ---- dropping ----

  /** The bytes of a dragged patch, wherever it lives. */
  private async bytesOf(item: DragItem, bank: BankService): Promise<Uint8Array | null> {
    if (item.kind === 'bank-slot') return bank.slots[item.slot]?.bytes ?? null;
    return this.backend.libraryGet(item.patchId);
  }

  /** The library id of a dragged patch (a patch from the bank is stored first, an identical one is reused). */
  private async idOf(item: DragItem, bank: BankService): Promise<number | null> {
    if (item.kind !== 'bank-slot') return item.patchId;
    const bytes = bank.slots[item.slot]?.bytes;
    if (!bytes) return null;
    return (await this.backend.libraryAdd(bytes, 'bank'))[0];
  }

  /**
   * Drop what is dragged on a place in the library. A single patch is moved (Alt copies); several patches, or patches from the bank, are
   * copied. A whole bulk dropped on "new" is duplicated. Resolves to a sentence for the screen, or null when nothing happened.
   */
  async drop(bank: BankService, target: DropTarget, payload: DragPayload, copy: boolean): Promise<string | null> {
    if (payload.type === 'bulk') {
      if (target.kind !== 'new') return null;
      return (await this.duplicate(payload.bulk)) === null ? null : 'Copied the bulk.';
    }
    const items = payload.items;
    const result = await this.run(async () => {
      if (target.kind === 'new') {
        const patches: Array<[number, Uint8Array]> = [];
        for (const it of items.slice(0, 128)) {
          const bytes = await this.bytesOf(it, bank);
          if (bytes) patches.push([patches.length, bytes]);
        }
        if (patches.length === 0) return null;
        const r = await this.backend.bulkSave(null, this.freeName('New bulk'), patches);
        this.open = { ...this.open, [r.bulk_id]: true };
        this.renaming = r.bulk_id;
        return `Made the bulk "${r.name}" from ${patches.length} patch${patches.length === 1 ? '' : 'es'}. Type its name.`;
      }
      const single = items.length === 1 ? items[0] : undefined;
      const [first] = items;
      if (target.kind === 'bulk-slot') {
        if (single?.kind === 'bulk-slot' && !copy) {
          await this.backend.bulkMove(single.bulk, single.position, target.bulk, target.position);
          return single.bulk === target.bulk ? 'Swapped the two patches.' : 'Moved the patch.';
        }
        let placed = 0;
        for (const [i, it] of items.entries()) {
          const pos = target.position + i;
          if (pos > 127) break;
          const id = await this.idOf(it, bank);
          if (id !== null) {
            await this.backend.bulkPlace(target.bulk, pos, id);
            placed += 1;
          }
        }
        return placed > 0 ? `Put ${placed} patch${placed === 1 ? '' : 'es'} in.` : null;
      }
      // dropped on the bulk itself: add at the first free places
      let added = 0;
      for (const it of items) {
        const id = await this.idOf(it, bank);
        if (id === null) continue;
        await this.backend.bulkAppend(target.bulk, id);
        added += 1;
        if (single?.kind === 'bulk-slot' && !copy && first && single.bulk !== target.bulk) await this.backend.bulkClear(single.bulk, single.position);
      }
      return added > 0 ? `Added ${added} patch${added === 1 ? '' : 'es'} at the end.` : null;
    });
    return result;
  }
}
