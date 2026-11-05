// The bank: all 128 User patch slots as the editor sees them. A bank is read from the synth, loaded from a file or built
// from library patches, changed slot by slot, and written back only through the guarded ceremony (a full backup first).

import { PatchDoc } from '../wasm/pkg/xppatch.js';
import type { ArmedBank, Backend, BankSaveReport, Progress } from './backend';
import { normaliseTag } from './folders';

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
  return typeof e === 'string' ? e : e instanceof Error ? e.message : String(e);
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
  error = $state('');
  selected = $state(0);
  /** Where the loaded bank came from, for the title. */
  source = $state('Empty');

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

  get hasOriginal(): boolean {
    return this.original.some((b) => b !== null);
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
    this.slots[slot] = { bytes: bytes.slice(), name, origin };
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
      this.phase = 'idle';
      return true;
    } catch (e) {
      this.fail(message(e));
      return false;
    }
  }

  /** What the last "add to library" did, for the notice. */
  libraryNotice = $state('');

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

  /** Step 1 of writing: read and back up the whole bank. Nothing is written. */
  async arm(): Promise<boolean> {
    if (this.working || this.phase === 'armed') return false;
    if (this.changed.length === 0) {
      this.fail('Nothing to write: no slot differs from the instrument.');
      return false;
    }
    // What is confirmed is fixed now: these slots, with these patches.
    this.frozen = this.changed.flatMap((i) => {
      const b = this.slots[i]?.bytes;
      return b ? [[i, b.slice()] as [number, Uint8Array]] : [];
    });
    this.phase = 'arming';
    this.error = '';
    this.report = null;
    this.cancelWhenArmed = false;
    try {
      this.armed = await this.withProgress(() => this.backend.armBank(this.confirmedSlots));
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

  private fail(text: string): void {
    this.error = text;
    this.phase = 'failed';
  }
}
