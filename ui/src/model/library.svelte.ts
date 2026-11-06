// The librarian: browse, search, tag and favourite stored patches, import and export `.syx`. State lives here; the work
import { previewFile } from './openFile';
// is done in Rust through a `Backend`.

import type { Backend, ImportResult, LibraryFilter, LibraryRow } from './backend';
import { reportError, reportInfo, reportSuccess, reportWarning } from './messages.svelte';

export const PAGE_SIZE = 200;
/** Largest .syx file accepted (the backend enforces the same limit). */
export const MAX_IMPORT_BYTES = 16 * 1_048_576;

function message(e: unknown): string {
  return typeof e === 'string' ? e : e instanceof Error ? e.message : String(e);
}

export class LibraryService {
  rows = $state<LibraryRow[]>([]);
  total = $state(0);
  tags = $state<Array<[string, number]>>([]);
  text = $state('');
  tag = $state('');
  favouritesOnly = $state(false);
  selectedId = $state<number | null>(null);
  /** Number of operations in flight (a boolean would flip early when two overlap). */
  private inFlight = $state(0);
  #error = $state('');
  get error(): string {
    return this.#error;
  }
  /** Setting text here also shows it in the bottom-left corner with every other message. */
  set error(text: string) {
    this.#error = text;
    if (text) reportError(text);
  }
  #notice = $state('');
  get notice(): string {
    return this.#notice;
  }
  /** Setting text here also shows it in the bottom-left corner with every other message. */
  set notice(text: string) {
    this.#notice = text;
    if (text) reportInfo(text);
  }

  get busy(): boolean {
    return this.inFlight > 0;
  }

  /** A counter so only the newest refresh may update the lists. */
  private ticket = 0;

  constructor(private readonly backend: Backend) {}

  private filter(offset = 0): LibraryFilter {
    return { text: this.text.trim(), tag: this.tag, favouritesOnly: this.favouritesOnly, limit: PAGE_SIZE, offset };
  }

  get selected(): LibraryRow | null {
    return this.rows.find((r) => r.id === this.selectedId) ?? null;
  }

  async refresh(): Promise<void> {
    const mine = ++this.ticket;
    try {
      const f = this.filter();
      const [rows, total, tags] = await Promise.all([
        this.backend.libraryList(f),
        this.backend.libraryCount(f),
        this.backend.libraryTags(),
      ]);
      if (mine !== this.ticket) return; // a newer search already replaced this one
      this.rows = rows;
      this.total = total;
      this.tags = tags;
      if (this.selectedId !== null && !rows.some((r) => r.id === this.selectedId)) this.selectedId = null;
      this.error = '';
    } catch (e) {
      if (mine === this.ticket) this.error = message(e);
    }
  }

  private async run(action: () => Promise<void>): Promise<boolean> {
    this.inFlight += 1;
    try {
      await action();
      this.error = '';
      await this.refresh();
      return true;
    } catch (e) {
      this.error = message(e);
      return false;
    } finally {
      this.inFlight -= 1;
    }
  }

  /** A file picked or dropped and shown for confirmation before it is imported (null when there is none). */
  staged = $state<{ name: string; bytes: Uint8Array; names: Array<[number, string]> } | null>(null);

  /** Read a file and show what it holds. Nothing is imported until `importStaged`. */
  stage(name: string, raw: Uint8Array): boolean {
    try {
      const { bytes, names } = previewFile(raw);
      this.staged = { name, bytes, names };
      this.error = '';
      return true;
    } catch (e) {
      this.staged = null;
      this.error = message(e);
      return false;
    }
  }

  cancelStaged(): void {
    this.staged = null;
  }

  async importStaged(): Promise<ImportResult | null> {
    const s = this.staged;
    if (!s) return null;
    const result = await this.importFile(s.name, s.bytes);
    // A failed import keeps the preview so the user can try again or cancel.
    if (result) this.staged = null;
    return result;
  }

  async importFile(name: string, bytes: Uint8Array): Promise<ImportResult | null> {
    if (bytes.length > MAX_IMPORT_BYTES) {
      this.error = `That file is larger than ${MAX_IMPORT_BYTES / 1_048_576} MB; a .syx bank is far smaller.`;
      return null;
    }
    let result: ImportResult | null = null;
    const ok = await this.run(async () => {
      result = await this.backend.libraryImport(bytes, name);
      this.notice = `Imported ${result.added} new patch${result.added === 1 ? '' : 'es'}; ${result.duplicates} already in the library.`;
    });
    return ok ? result : null;
  }

  async addPatch(bytes: Uint8Array, source: string): Promise<boolean> {
    return this.run(async () => {
      const [, isNew] = await this.backend.libraryAdd(bytes, source);
      this.notice = isNew ? 'Added to the library.' : 'An identical patch is already in the library.';
    });
  }

  async favourite(id: number, on: boolean): Promise<boolean> {
    return this.run(() => this.backend.libraryFavourite(id, on));
  }

  async addTag(id: number, tag: string): Promise<boolean> {
    return this.run(() => this.backend.libraryTagAdd(id, tag));
  }

  async removeTag(id: number, tag: string): Promise<boolean> {
    return this.run(() => this.backend.libraryTagRemove(id, tag));
  }

  async rename(id: number, name: string): Promise<boolean> {
    return this.run(() => this.backend.libraryRename(id, name));
  }

  async remove(id: number): Promise<boolean> {
    return this.run(() => this.backend.libraryDelete(id));
  }

  /** The stored patch as 590 bytes of blocks, or null (with `error` set). */
  async fetchBlocks(id: number): Promise<Uint8Array | null> {
    try {
      return await this.backend.libraryGet(id);
    } catch (e) {
      this.error = message(e);
      return null;
    }
  }

  /** The visible patches as a `.syx` bank (at most 128), or null (with `error` set). */
  async exportVisible(device: number): Promise<Uint8Array | null> {
    const ids = this.rows.slice(0, 128).map((r) => r.id);
    if (ids.length === 0) {
      this.error = 'Nothing to export: no patches match.';
      return null;
    }
    try {
      const bytes = await this.backend.libraryExport(ids, device);
      this.notice = this.rows.length > 128 ? `A bank holds 128 patches: exported the first 128 of ${this.rows.length} shown.` : `Exported ${ids.length} patch${ids.length === 1 ? '' : 'es'}.`;
      return bytes;
    } catch (e) {
      this.error = message(e);
      return null;
    }
  }
}
