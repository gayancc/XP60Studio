// The bank: all 128 User patch slots as the editor sees them. A bank is read from the synth, loaded from a file or built
// from library patches, changed slot by slot, and written back only through the guarded ceremony (a full backup first).

import { PatchDoc } from '../wasm/pkg/xppatch.js';
import type { ArmedBank, Backend, BankSaveReport, Progress } from './backend';
import type { Catalog } from './catalog';
import { normaliseTag } from './folders';
import { errorText, humanizeErrorText, reportError, reportInfo, reportSuccess, reportWarning } from './messages.svelte';
import {
  applyBankLevelReport,
  levelBank,
  type BankLevelReport,
} from './voiceLeveling';

export const SLOTS = 128;

export type SlotOrigin = 'synth' | 'file' | 'library' | 'editor';

export interface BankSlot {
  /** 590 bytes of blocks, or null when nothing is known about the slot yet. */
  bytes: Uint8Array | null;
  name: string;
  origin: SlotOrigin | null;
}

export type BankPhase = 'idle' | 'reading' | 'arming' | 'armed' | 'writing' | 'done' | 'failed';

function message(e: unknown): string {
  return errorText(e);
}

function sameBytes(a: Uint8Array | null, b: Uint8Array | null): boolean {
  if (a === null || b === null) return a === b;
  return a.length === b.length && a.every((v, i) => v === b[i]);
}

const emptySlot = (): BankSlot => ({ bytes: null, name: '', origin: null });

export class BankService {
  slots = $state<BankSlot[]>(Array.from({ length: SLOTS }, emptySlot));
  /** What the synth held when the bank was last read, to tell which slots have been changed. */
  private original: Array<Uint8Array | null> = Array.from({ length: SLOTS }, () => null);
  /** The slots and patches confirmed at arming. A write sends exactly these, whatever is edited afterwards. */
  private frozen: Array<[number, Uint8Array]> = [];
  /** The panel was closed while the bank was still being backed up: disarm as soon as arming finishes. */
  private cancelWhenArmed = false;
  phase = $state<BankPhase>('idle');
  progress = $state<Progress | null>(null);
  armed = $state<ArmedBank | null>(null);
  report = $state<BankSaveReport | null>(null);
  #error = $state('');
  get error(): string {
    return this.#error;
  }
  /** Setting text here also shows it in the bottom-left corner with every other message. */
  set error(text: string) {
    const clean = text ? humanizeErrorText(text) : '';
    this.#error = clean;
    if (clean) reportError(clean);
  }
  selected = $state(0);
  /**
   * Slot last opened into the Patch editor from this bank. Used so a write can pull the editor's
   * current patch back into that slot (edits do not land in the bank until then).
   */
  editorSlot = $state<number | null>(null);
  /**
   * When preparing a write, also save the instrument’s current User bank to a backup `.syx`.
   * Off by the musician’s choice — writes still read the target slots for the stale-slot guard.
   */
  backupBeforeWrite = $state(true);
  /** Where the loaded bank came from, for the title. */
  source = $state('Empty');
  /** The library bulk this bank was loaded from or last saved as; null when the bank is not in the library. */
  origin = $state<{ id: number; name: string } | null>(null);
  /** The patches the bank held when it was last loaded from or saved to the library, to tell what has changed since. */
  private libraryBaseline = $state<Array<Uint8Array | null>>(Array.from({ length: SLOTS }, () => null));
  /** Last Level bank preview (in-memory only; nothing sent to the instrument). */
  levelPreview = $state<BankLevelReport | null>(null);
  /** One-shot undo for the last Level bank Apply (original slot bytes). */
  private levelUndo: Array<{ slot: number; bytes: Uint8Array }> | null = null;

  constructor(private readonly backend: Backend) {}

  get working(): boolean {
    return this.phase === 'reading' || this.phase === 'arming' || this.phase === 'writing';
  }

  /** Working, or armed: the slots cannot be changed (what was confirmed must be what is written). */
  get locked(): boolean {
    return this.working || this.phase === 'armed';
  }

  /** The slots that will be written, as confirmed at arming. */
  get confirmedSlots(): number[] {
    return this.frozen.map(([s]) => s);
  }

  /** The slots that differ from what the synth held when it was read (all assigned slots if it was never read). */
  get changed(): number[] {
    return this.slots.flatMap((s, i) => (s.bytes !== null && !sameBytes(s.bytes, this.original[i] ?? null) ? [i] : []));
  }

  /** Every slot that currently holds a patch in the app (candidates for "write all"). */
  get filled(): number[] {
    return this.slots.flatMap((s, i) => (s.bytes !== null ? [i] : []));
  }

  get hasOriginal(): boolean {
    return this.original.some((b) => b !== null);
  }

  /** Remember that the Patch editor was opened from this bank slot. */
  linkEditor(slot: number): void {
    if (!Number.isInteger(slot) || slot < 0 || slot >= SLOTS) {
      this.editorSlot = null;
      return;
    }
    this.editorSlot = slot;
  }

  /**
   * If the editor was opened from a bank slot and `bytes` differ from what that slot holds, put them
   * into the slot (origin editor). Returns whether the slot was updated.
   */
  pullEditor(bytes: Uint8Array): boolean {
    const slot = this.editorSlot;
    if (slot === null || this.locked) return false;
    const held = this.slots[slot]?.bytes ?? null;
    if (held !== null && sameBytes(held, bytes)) return false;
    return this.assign(slot, bytes, 'editor');
  }

  /**
   * After a successful single-slot save to the instrument: the pad now matches User memory, so it
   * is no longer “changed” relative to the last instrument read.
   */
  markWritten(slot: number, bytes: Uint8Array): void {
    if (!Number.isInteger(slot) || slot < 0 || slot >= SLOTS || this.locked) return;
    this.setSlot(slot, bytes, 'synth');
    this.original[slot] = bytes.slice();
  }

  private async withProgress<T>(action: () => Promise<T>): Promise<T> {
    let stop: (() => void) | undefined;
    try {
      stop = await this.backend.onProgress((p) => (this.progress = p));
      return await action();
    } finally {
      stop?.();
      this.progress = null;
    }
  }

  private setSlot(slot: number, bytes: Uint8Array, origin: SlotOrigin): void {
    let name = '';
    try {
      const doc = PatchDoc.fromBlocks(bytes);
      name = doc.name();
      doc.free();
    } catch {
      name = '(unreadable)';
    }
    // Replace the array so every view sees the new pad name even when only one slot changed.
    this.slots = this.slots.map((s, i) => (i === slot ? { bytes: bytes.slice(), name, origin } : s));
  }

  /** Put a patch (590 bytes of blocks) into a slot locally. Nothing reaches the synth until the bank is written. */
  assign(slot: number, bytes: Uint8Array, origin: SlotOrigin): boolean {
    if (!Number.isInteger(slot) || slot < 0 || slot >= SLOTS) {
      this.error = 'Choose a slot from 1 to 128.';
      return false;
    }
    if (this.locked) return false;
    try {
      PatchDoc.fromBlocks(bytes).free();
    } catch (e) {
      this.error = message(e);
      return false;
    }
    this.error = '';
    this.setSlot(slot, bytes, origin);
    return true;
  }

  /** Exchange two slots. */
  swap(a: number, b: number): void {
    if (this.locked || a === b || a < 0 || b < 0 || a >= SLOTS || b >= SLOTS) return;
    const sa = this.slots[a];
    const sb = this.slots[b];
    if (!sa || !sb) return;
    this.slots[a] = sb;
    this.slots[b] = sa;
  }

  /** The slots that differ from what the library bulk held when the bank was loaded from or saved to it. */
  get unsaved(): number[] {
    return this.slots.flatMap((s, i) => (!sameBytes(s.bytes, this.libraryBaseline[i] ?? null) ? [i] : []));
  }

  /** The bank now matches the library bulk `id`: remember it as the one to save over, and what it held. */
  markSaved(id: number, name: string): void {
    this.origin = { id, name };
    this.libraryBaseline = this.slots.map((s) => s.bytes?.slice() ?? null);
    this.source = `Bulk: ${name}`;
  }

  /** Replace every slot with the patches of a library bulk (`[position, 590 bytes]`); positions it does not hold become empty. */
  loadFromBulk(id: number, name: string, blocks: Array<[number, Uint8Array]>): number {
    if (this.locked) return 0;
    const next: BankSlot[] = Array.from({ length: SLOTS }, emptySlot);
    this.slots = next;
    let n = 0;
    for (const [slot, bytes] of blocks) {
      if (slot < 0 || slot >= SLOTS) continue;
      this.setSlot(slot, bytes, 'library');
      n += 1;
    }
    this.markSaved(id, name);
    this.editorSlot = null;
    return n;
  }

  /** Replace the whole bank from a `.syx` file (a bank file with up to 128 patches); missing slots stay as they were. */
  loadFile(bytes: Uint8Array, fileName: string, list: Array<[number, string]>): number {
    if (this.locked) return 0;
    let n = 0;
    for (const [slot] of list) {
      try {
        const doc = PatchDoc.fromSyxSlot(bytes, slot);
        this.setSlot(slot, doc.toBlocks(), 'file');
        doc.free();
        n += 1;
      } catch (e) {
        this.error = message(e);
      }
    }
    this.source = fileName;
    this.origin = null; // a file is not a library bulk
    this.editorSlot = null;
    return n;
  }

  /** Read all 128 slots from the instrument. Replaces what is shown. */
  async readFromSynth(): Promise<boolean> {
    if (this.working) return false;
    if (this.phase === 'armed') await this.cancelArming();
    this.phase = 'reading';
    this.error = '';
    this.report = null;
    try {
      const rows = await this.withProgress(() => this.backend.readBank());
      for (const [slot, bytes] of rows) {
        this.setSlot(slot, bytes, 'synth');
        this.original[slot] = bytes.slice();
      }
      this.source = 'Instrument';
      this.editorSlot = null;
      this.phase = 'idle';
      return true;
    } catch (e) {
      this.fail(message(e));
      return false;
    }
  }

  /** What the last "add to library" did, for the notice. */
  #libraryNotice = $state('');
  get libraryNotice(): string {
    return this.#libraryNotice;
  }
  /** Setting text here also shows it in the bottom-left corner with every other message. */
  set libraryNotice(text: string) {
    this.#libraryNotice = text;
    if (text) reportSuccess(text);
  }

  /**
   * File every patch in the grid into the Library under the folder `folder` (a tag, so "Synth/2026-11-04" nests). A patch the
   * library already holds is not stored twice but is still filed in the folder. Stops at the first failure and says how far
   * it got. Returns how many were filed, or null when it could not start.
   */
  async addAllToLibrary(folder: string): Promise<number | null> {
    const tag = normaliseTag(folder);
    const patches = this.slots.flatMap((s, i) => (s.bytes ? [[i, s.bytes] as [number, Uint8Array]] : []));
    this.libraryNotice = '';
    if (tag === '') {
      this.error = 'Give the folder a name.';
      return null;
    }
    if (patches.length === 0) {
      this.error = 'The bank is empty: read it from the synth or load a file first.';
      return null;
    }
    let filed = 0;
    let added = 0;
    try {
      for (const [, bytes] of patches) {
        const [id, isNew] = await this.backend.libraryAdd(bytes, this.source);
        await this.backend.libraryTagAdd(id, tag);
        filed += 1;
        if (isNew) added += 1;
      }
    } catch (e) {
      this.error = `${message(e)} (${filed} of ${patches.length} filed before it stopped.)`;
      return filed;
    }
    this.error = '';
    this.libraryNotice = `${filed} patches filed in the Library folder "${tag}" (${added} new, ${filed - added} already there).`;
    return filed;
  }

  /**
   * Read every User patch, performance and rhythm set from the instrument as one `.syx` for the user to keep. This is a
   * read only: nothing is written to the instrument and the bank grid is not changed. Null (with `error` set) on failure.
   */
  async backupEverything(): Promise<Uint8Array | null> {
    if (this.working) return null;
    this.phase = 'reading';
    this.error = '';
    try {
      const bytes = await this.withProgress(() => this.backend.backupAll());
      this.phase = 'idle';
      return bytes;
    } catch (e) {
      this.fail(message(e));
      return null;
    }
  }

  /**
   * Step 1 of writing: freeze the chosen slots for commit. When `backupBeforeWrite` is on, the
   * whole User bank is also saved to a `.syx` first. `slots` is which app slots to write
   * (changed-only, one slot, or all filled). Nothing is written to User memory yet.
   */
  async arm(slots?: readonly number[]): Promise<boolean> {
    if (this.working || this.phase === 'armed') return false;
    const targets = slots !== undefined ? [...slots] : this.changed;
    const unique = [...new Set(targets)].filter((i) => Number.isInteger(i) && i >= 0 && i < SLOTS).sort((a, b) => a - b);
    this.frozen = unique.flatMap((i) => {
      const b = this.slots[i]?.bytes;
      return b ? [[i, b.slice()] as [number, Uint8Array]] : [];
    });
    if (this.frozen.length === 0) {
      this.fail(
        slots !== undefined
          ? 'Nothing to write: those slots have no patch in the app yet.'
          : 'Nothing to write: no slot differs from the instrument. Put the editor patch into a slot, or choose This slot / All filled.',
      );
      return false;
    }
    this.phase = 'arming';
    this.error = '';
    this.report = null;
    this.cancelWhenArmed = false;
    try {
      this.armed = await this.withProgress(() => this.backend.armBank(this.confirmedSlots, this.backupBeforeWrite));
      this.phase = 'armed';
      if (this.cancelWhenArmed) {
        await this.cancelArming();
        return false;
      }
      return true;
    } catch (e) {
      this.armed = null;
      this.frozen = [];
      this.fail(message(e));
      return false;
    }
  }

  /** Prepare a write of only the slots that differ from the last instrument read. */
  async armChanged(): Promise<boolean> {
    return this.arm(this.changed);
  }

  /** Prepare a write of one slot (the selected pad, or `slot` when given). */
  async armSelected(slot: number = this.selected): Promise<boolean> {
    return this.arm([slot]);
  }

  /** Prepare a write of every slot that currently holds a patch in the app. */
  async armAllFilled(): Promise<boolean> {
    return this.arm(this.filled);
  }

  async cancelArming(): Promise<void> {
    if (this.phase === 'arming') {
      this.cancelWhenArmed = true;
      return;
    }
    if (this.phase !== 'armed') return;
    this.phase = 'idle';
    this.armed = null;
    this.frozen = [];
    try {
      await this.backend.disarmBank();
    } catch {
      // The next arming replaces it.
    }
  }

  /** Step 2: write the changed slots. Stops at the first failure. */
  async commit(): Promise<boolean> {
    if (this.phase !== 'armed') return false;
    const patches = this.frozen;
    this.frozen = [];
    this.phase = 'writing';
    this.armed = null;
    try {
      const report = await this.withProgress(() => this.backend.commitBank(patches));
      this.report = report;
      // What the synth now holds for the slots that were written is what is shown.
      // What the synth now holds for the slots that were written is what was sent (not what is shown now).
      const sent = new Map(patches);
      for (const slot of report.written) this.original[slot] = sent.get(slot)?.slice() ?? null;
      if (report.stopped_at) {
        this.fail(`Stopped at slot ${report.stopped_at[0] + 1}: ${report.stopped_at[1]}`);
        return false;
      }
      if (report.cancelled) {
        this.fail(`Cancelled after ${report.written.length} of ${patches.length} slots.`);
        return false;
      }
      this.phase = 'done';
      reportSuccess(`Wrote ${report.written.length} slot${report.written.length === 1 ? '' : 's'} and read each one back: it matches.`);
      return true;
    } catch (e) {
      // Some slots may have been written before this failed: read the bank again to see what the synth holds.
      this.fail(`${message(e)} Read the bank from the synth to see what it holds now.`);
      return false;
    }
  }

  async cancel(): Promise<void> {
    if (!this.working) return;
    try {
      await this.backend.cancelOperation();
    } catch {
      // Nothing more to do; the operation ends on its own.
    }
  }

  /** The bank as a `.syx` file (every slot that holds a patch). */
  async toSyx(device: number): Promise<Uint8Array | null> {
    const patches = this.slots.flatMap((s, i) => (s.bytes ? [[i, s.bytes] as [number, Uint8Array]] : []));
    if (patches.length === 0) {
      this.error = 'The bank is empty.';
      return null;
    }
    try {
      return await this.backend.bankToSyx(patches, device);
    } catch (e) {
      this.error = message(e);
      return null;
    }
  }

  reset(): void {
    if (this.working || this.phase === 'armed') return;
    this.phase = 'idle';
    this.error = '';
    this.report = null;
  }

  /**
   * Preview Normalize bank (ELS median match) over every filled slot.
   * Does not change slots or write the instrument.
   */
  previewLevelBank(catalog: Catalog): BankLevelReport | null {
    if (this.locked) {
      this.error = 'Finish or cancel the bank write before normalizing.';
      return null;
    }
    if (this.filled.length === 0) {
      this.error = 'Load or read patches into the bank first.';
      return null;
    }
    try {
      this.error = '';
      this.levelPreview = levelBank(catalog, this.slots);
      reportInfo(
        `Normalize bank preview: ${this.levelPreview.changed} of ${this.levelPreview.filled} slots would change` +
          (this.levelPreview.outliers ? ` · ${this.levelPreview.outliers} outlier(s) kept identity-safe` : ''),
      );
      return this.levelPreview;
    } catch (e) {
      this.error = message(e);
      this.levelPreview = null;
      return null;
    }
  }

  /** Apply the current Normalize bank preview into memory only. */
  applyLevelBank(): boolean {
    if (this.locked || !this.levelPreview) return false;
    const report = this.levelPreview;
    this.levelUndo = applyBankLevelReport(
      report,
      (slot) => this.slots[slot]?.bytes ?? null,
      (slot, bytes) => this.setSlot(slot, bytes, 'editor'),
    );
    this.levelPreview = null;
    reportSuccess(
      this.levelUndo.length
        ? `Normalize bank applied to ${this.levelUndo.length} slot(s) in memory. Write the bank when you want them on the XP-60.`
        : 'Normalize bank: no slot needed changes.',
    );
    return true;
  }

  /** Restore slot bytes from the last Normalize bank Apply. */
  undoLevelBank(): boolean {
    if (this.locked || !this.levelUndo?.length) return false;
    for (const u of this.levelUndo) this.setSlot(u.slot, u.bytes, 'editor');
    const n = this.levelUndo.length;
    this.levelUndo = null;
    reportInfo(`Undid Normalize bank on ${n} slot(s).`);
    return true;
  }

  get canUndoLevelBank(): boolean {
    return !this.locked && (this.levelUndo?.length ?? 0) > 0;
  }

  clearLevelPreview(): void {
    this.levelPreview = null;
  }

  private fail(text: string): void {
    this.error = text;
    this.phase = 'failed';
  }
}
