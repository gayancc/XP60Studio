// The patch being edited: its values, undo history, live-edit pipeline and sync state. The codec is the Rust one (wasm);
// this class only coordinates.

import { PatchDoc, docSyxNames, syxNames, syxReport } from '../wasm/pkg/xppatch.js';
import { paramInfo, type BlockName, type Catalog, type DocKindName } from './catalog';
import { KIND_TITLES, docFromBlocks, docFromSyxSlot, newDoc, paramAddressOf, type DocApi } from './kinds';
import { blockTitle } from './labels';
import { diff, initChanges, morph, type Change } from './tools';
import { changedParams, sameBytes, splitFrames } from './frames';
import { History, type HistoryEntry } from './history';
import { LiveQueue, type QueueStats } from './liveQueue';
import { cutFrames } from './sendOptions.svelte';
import type { LiveTransport } from './transport';
import type { SyncState } from '../kit/syncState';
import { humanizeErrorText, reportError, reportInfo, reportSuccess, reportWarning } from './messages.svelte';
import { ToneMutationLog, isToneBlock, otherTonesUnchanged } from './toneIsolation';

/** More changed parameters than this in one step go out as a whole send instead of single frames. */
export const BULK_LIVE_LIMIT = 24;

/** Wave group type/id must not be live-sent alone: an intermediate pair can select a missing expansion board and silence the Tone. */
const WAVE_IDENTITY = new Set(['wave_group_type', 'wave_group_id']);

/**
 * EFX type must not be live-sent alone: the twelve parameter slots change meaning with the type, and a type-only DT1
 * leaves the instrument on the previous type's numbers (ledger H-073). App defaults go with the type as one whole send.
 */
const EFX_TYPE = 'efx_type';

export interface HistoryItem {
  label: string;
  at: number;
}

export class PatchStore {
  /** Bumped on every change so reads in components re-run. */
  rev = $state(0);
  sync = $state<SyncState>('local');
  live = $state(true);
  queue = $state<QueueStats>({ pending: 0, inFlight: 0, state: 'ok', sent: 0, failed: 0, lost: 0 });
  /** Entries of the history, oldest first, and how many are applied. */
  items = $state<HistoryItem[]>([]);
  position = $state(0);
  /** Description of the open sound's origin ("Preview bank, slot 12"). */
  origin = $state('New patch');
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

  /** The two A/B slots (snapshots), and which one the editor is showing. */
  ab = $state<{ a: Uint8Array | null; b: Uint8Array | null; active: 'a' | 'b' }>({ a: null, b: null, active: 'a' });
  /** The state the sound was opened in, for the Compare list. */
  reference = $state<Uint8Array>(new Uint8Array());

  private doc: DocApi;
  private readonly history = new History<Uint8Array>();
  private group: { label: string; before: Uint8Array } | null = null;
  private readonly liveQueue: LiveQueue;
  private liveDirty = false;
  /** True when the instrument's temporary patch is known to hold this patch (sent whole, or read back). */
  private baseSent = false;
  /** A whole-patch send is in the queue; when it drains the base is on the instrument. */
  private fullPending = false;
  /**
   * A whole-patch live send was requested while an undo group is open. Flush it from `end()` so type/id (and the first
   * sync after opening a file) never leave the instrument on a half-updated, often silent, intermediate state.
   */
  private deferredFullSend = false;
  /**
   * Diagnostic checkpoints for Tone 1–4 writes (before/after value + SysEx coalesce address). Used by the verification
   * loop; also guards against cross-tone byte bleed on every Tone parameter set.
   */
  readonly toneLog = new ToneMutationLog();

  constructor(
    readonly catalog: Catalog,
    private readonly transport: LiveTransport,
    private device = 0x10,
    clock: { now: () => number } = { now: () => performance.now() },
    readonly kind: DocKindName = 'patch',
  ) {
    this.doc = newDoc(kind);
    this.reference = this.doc.toBlocks();
    // System settings live in permanent memory: they are edited here and written only through the write ceremony.
    if (kind === 'system') this.live = false;
    this.liveQueue = new LiveQueue({
      send: (f) => transport.send(f),
      now: () => clock.now(),
      setTimer: (fn, ms) => setTimeout(fn, ms),
      clearTimer: (h) => clearTimeout(h as ReturnType<typeof setTimeout>),
      onstats: (s) => {
        this.queue = s;
        if (s.state === 'failed') {
          this.baseSent = false;
          this.error = 'Some edits could not be sent to the instrument.';
        }
        if (this.liveDirty && s.pending === 0 && s.inFlight === 0 && s.state === 'ok') {
          this.liveDirty = false;
          if (this.fullPending) {
            this.fullPending = false;
            this.baseSent = true;
          }
          // Only claim SENT when the instrument has the whole patch and the user has not changed sound behind us.
          // `local` is included so opening a file/New with Live on (immediate whole-send) can reach SENT without a fake edit.
          if (this.baseSent && (this.sync === 'edited' || this.sync === 'local')) this.sync = 'sent';
        }
      },
    });
  }

  /** The raw device ID (16 to 31) frames are addressed to. */
  setDevice(device: number): void {
    this.device = device;
  }

  /** The raw device ID (16 to 31) frames are addressed to. */
  get deviceId(): number {
    return this.device;
  }

  get transportLabel(): string {
    return this.transport.label;
  }

  // ---- reading ----

  get name(): string {
    void this.rev;
    return this.doc.name();
  }

  get(block: BlockName, id: string): number {
    void this.rev;
    return this.doc.get(block, id);
  }

  format(block: BlockName, id: string): string {
    void this.rev;
    return this.doc.format(block, id);
  }

  formatValue(block: BlockName, id: string, value: number): string {
    return this.doc.formatValue(block, id, value);
  }

  get canUndo(): boolean {
    void this.position;
    return this.history.canUndo;
  }

  get canRedo(): boolean {
    void this.position;
    return this.history.canRedo;
  }

  snapshot(): Uint8Array {
    return this.doc.toBlocks();
  }

  // ---- editing ----

  /** Open a group: everything until `end()` is one undo step (a whole drag). */
  begin(label: string): void {
    if (this.group) return;
    this.group = { label, before: this.doc.toBlocks() };
  }

  end(): void {
    const g = this.group;
    this.group = null;
    if (!g) return;
    const after = this.doc.toBlocks();
    if (sameBytes(g.before, after)) {
      this.deferredFullSend = false;
      return;
    }
    this.history.record(g.label, g.before, after, Date.now());
    this.syncHistory();
    if (this.deferredFullSend) {
      this.deferredFullSend = false;
      if (this.live && this.transport.connected && this.kind !== 'system') this.sendPatch();
    }
  }

  label(block: BlockName, id: string): string {
    const info = paramInfo(this.catalog, this.kind, block, id);
    return `${blockTitle(block, this.kind)} · ${info?.name ?? id}`;
  }

  /** Set one value. Outside a group it is its own undo step. */
  set(block: BlockName, id: string, value: number): void {
    const info = paramInfo(this.catalog, this.kind, block, id);
    if (!info || value === this.doc.get(block, id)) return;
    const beforeValue = this.doc.get(block, id);
    const beforeBlocks = isToneBlock(block, this.kind) ? this.doc.toBlocks() : null;
    const ownGroup = this.group === null;
    if (ownGroup) this.begin(this.label(block, id));
    try {
      this.doc.set(block, id, value);
      this.error = '';
    } catch (e) {
      this.error = e instanceof Error ? e.message : String(e);
      if (ownGroup) this.end();
      return;
    }
    if (beforeBlocks) {
      const afterBlocks = this.doc.toBlocks();
      if (!otherTonesUnchanged(this.catalog, beforeBlocks, afterBlocks, block)) {
        try {
          this.doc.set(block, id, beforeValue);
        } catch {
          /* keep reporting the bleed */
        }
        this.error = `Tone isolation failed: writing ${block}.${id} changed another Tone's bytes.`;
        if (ownGroup) this.end();
        return;
      }
      try {
        this.toneLog.record({
          at: Date.now(),
          block,
          id,
          before: beforeValue,
          after: this.doc.get(block, id),
          address: paramAddressOf(this.kind, block, id),
        });
      } catch (e) {
        this.error = e instanceof Error ? e.message : String(e);
        if (ownGroup) this.end();
        return;
      }
    }
    this.afterEdit([{ block, id }]);
    if (ownGroup) this.end();
  }

  setName(name: string): void {
    if (name.trimEnd() === this.doc.name().trimEnd()) return;
    const ownGroup = this.group === null;
    if (ownGroup) this.begin(`${blockTitle('common', this.kind)} · Name`);
    const before = this.doc.toBlocks();
    try {
      this.doc.setName(name);
      this.error = '';
    } catch (e) {
      this.error = e instanceof Error ? e.message : String(e);
      if (ownGroup) this.end();
      return;
    }
    this.afterEdit(changedParams(this.catalog, this.kind, before, this.doc.toBlocks()));
    if (ownGroup) this.end();
  }

  private afterEdit(changed: Array<{ block: BlockName; id: string }>): void {
    this.rev += 1;
    this.sync = 'edited';
    if (!this.live || !this.transport.connected || this.kind === 'system') {
      this.baseSent = false;
      return;
    }
    if (!this.baseSent) {
      // The instrument does not yet hold this patch, so a lone parameter would leave it half-updated: send it all.
      this.requestFullSend();
      return;
    }
    // The Rhythm Setup's name is not sent live until its temporary address is confirmed (ledger H-045).
    const sendable = changed.filter((c) => !(this.kind === 'rhythm' && c.block === 'common'));
    if (sendable.length > BULK_LIVE_LIMIT) {
      // A tool touched a lot of parameters: one whole send is faster and safer than hundreds of single frames.
      this.requestFullSend();
      return;
    }
    // Wave group type/id change the board the Tone points at. Sending them one-by-one (Wave picker sets type, then id,
    // then number) can briefly select an uninstalled board and the Tone goes silent until a full consistent set arrives.
    // EFX type likewise: the twelve slots are meaningless until the matching defaults arrive (H-073).
    if (sendable.some((c) => WAVE_IDENTITY.has(c.id) || c.id === EFX_TYPE)) {
      this.requestFullSend();
      return;
    }
    for (const { block, id } of sendable) {
      try {
        this.liveQueue.push(paramAddressOf(this.kind, block, id), this.doc.paramFrame(this.device, block, id));
        this.liveDirty = true;
      } catch (e) {
        this.error = e instanceof Error ? e.message : String(e);
      }
    }
  }

  /** Whole-patch live send, or defer it to `end()` when an undo group is open so intermediate values never go out. */
  private requestFullSend(): void {
    if (this.group) {
      this.deferredFullSend = true;
      return;
    }
    this.sendPatch();
  }

  /** Apply many changes as one undo step. A large batch goes out as one whole-document send. */
  applyMany(changes: readonly Change[], label: string): void {
    // Entries that would not change anything are dropped, so a tool never marks the sound edited for nothing.
    const real = changes.filter((c) => {
      try {
        return this.doc.get(c.block, c.id) !== c.value;
      } catch {
        return true; // let doc.set report why
      }
    });
    if (real.length === 0) return;
    const ownGroup = this.group === null;
    if (ownGroup) this.begin(label);
    const done: Change[] = [];
    let failure = '';
    for (const c of real) {
      try {
        this.doc.set(c.block, c.id, c.value);
        done.push(c);
      } catch (e) {
        failure = e instanceof Error ? e.message : String(e);
      }
    }
    if (done.length > 0) this.afterEdit(done);
    if (ownGroup) this.end();
    // After end(): a deferred whole-patch send clears error on success; keep a skipped invalid value visible.
    if (failure) this.error = failure;
  }

  // ---- A/B and morph ----

  /** Switch between the A and B versions. The first switch starts B as a copy of the current sound. */
  abSwitch(): void {
    this.end();
    this.morphBase = null;
    const current = this.doc.toBlocks();
    const here = this.ab.active;
    const there = here === 'a' ? 'b' : 'a';
    const slots = { ...this.ab, [here]: current };
    const target = slots[there] ?? current;
    slots[there] = target;
    this.ab = { a: slots.a, b: slots.b, active: there };
    this.applyMany(
      diff(this.catalog, this.kind, current, target).map((d) => ({ block: d.block, id: d.id, value: d.b })),
      `A/B: ${there.toUpperCase()}`,
    );
  }

  /** True when both A and B hold different sounds, so a morph means something. */
  get canMorph(): boolean {
    void this.rev;
    const pair = this.abPair();
    return pair !== null && !sameBytes(pair.a, pair.b);
  }

  /** The A and B versions with the visible one brought up to date. */
  private abPair(): { a: Uint8Array; b: Uint8Array } | null {
    const current = this.doc.toBlocks();
    const a = this.ab.active === 'a' ? current : this.ab.a;
    const b = this.ab.active === 'b' ? current : this.ab.b;
    return a && b ? { a, b } : null;
  }

  private morphBase: { a: Uint8Array; b: Uint8Array } | null = null;

  /**
   * Blend A (t = 0) into B (t = 1). The two versions are fixed when a morph starts and kept until `endMorph`, so
   * dragging back and forth is exact and returns to either end. Wrap a drag in begin/end for one undo step.
   */
  morphTo(t: number): void {
    this.morphBase ??= this.abPair();
    const base = this.morphBase;
    if (!base) return;
    this.applyMany(
      morph(this.catalog, this.kind, base.a, base.b, t, (block, id) => this.doc.get(block, id)),
      'Morph',
    );
  }

  /** Finish a morph: the visible version now holds the blend. */
  endMorph(): void {
    this.morphBase = null;
    this.end();
  }

  /** What differs between the sound as it was opened and now. */
  get changesSinceOpened(): ReturnType<typeof diff> {
    void this.rev;
    return diff(this.catalog, this.kind, this.reference, this.doc.toBlocks());
  }

  // ---- history ----

  private syncHistory(): void {
    this.items = this.history.entries.map((e: HistoryEntry<Uint8Array>) => ({ label: e.label, at: e.at }));
    this.position = this.history.position;
  }

  private restore(snapshot: Uint8Array | null): void {
    if (!snapshot) return;
    const current = this.doc.toBlocks();
    this.group = null;
    // Going back in history leaves the A/B pair and any morph in progress behind: they described the old state.
    this.ab = { a: null, b: null, active: 'a' };
    this.morphBase = null;
    this.replaceDoc(docFromBlocks(this.kind, snapshot));
    this.rev += 1;
    this.syncHistory();
    this.afterEdit(changedParams(this.catalog, this.kind, current, snapshot));
  }

  undo(): void {
    this.end();
    this.restore(this.history.undo());
  }

  redo(): void {
    this.end();
    this.restore(this.history.redo());
  }

  jumpTo(n: number): void {
    this.end();
    this.restore(this.history.jumpTo(n));
  }

  // ---- the instrument ----

  /** The instrument's sound was changed from its panel: our copy of its temporary patch is out of date. */
  markStale(): void {
    if (!this.transport.connected || this.kind === 'system') return;
    this.baseSent = false;
    this.fullPending = false;
    if (this.sync === 'sent' || this.sync === 'tempVerified' || this.sync === 'edited') this.sync = 'stale';
  }

  /** The connection to the instrument opened or closed. Nothing is sent on connect; the first edit sends the patch. */
  connectionChanged(connected: boolean): void {
    this.liveQueue.clear();
    this.baseSent = false;
    this.fullPending = false;
    this.liveDirty = false;
    if (!connected && this.sync !== 'local') this.sync = 'edited';
  }

  /** Resolves when no live edit is waiting or in flight (call before reading the instrument back). */
  whenQueueIdle(): Promise<void> {
    return this.liveQueue.whenIdle();
  }

  /** Counts every change to the open patch; a read that started earlier is out of date if this moved. */
  get revision(): number {
    return this.rev;
  }

  /** Stop timers and free the codec object. */
  dispose(): void {
    this.liveQueue.clear();
    this.doc.free();
  }

  private replaceDoc(next: DocApi): void {
    const old = this.doc;
    this.doc = next;
    old.free();
  }

  /** Open a patch read from the instrument's temporary patch (590 bytes). It matches the instrument, so it is verified. */
  openFromInstrument(bytes: Uint8Array): void {
    try {
      const system = this.kind === 'system';
      // Already on the instrument — do not whole-send it back (that would race the just-fetched bytes).
      this.open(docFromBlocks(this.kind, bytes), system ? 'Instrument, system settings (read)' : 'Instrument, temporary copy', false);
      // System settings are permanent memory: reading them is not an XP TEMP state.
      this.sync = system ? 'local' : 'tempVerified';
      this.baseSent = !system;
      this.error = '';
    } catch (e) {
      this.error = e instanceof Error ? e.message : String(e);
    }
  }

  /** Compare the open patch with what was read back from the instrument's temporary patch. */
  verifyAgainst(readBack: Uint8Array): boolean {
    const same = sameBytes(this.doc.toBlocks(), readBack);
    this.sync = same ? 'tempVerified' : 'mismatch';
    this.baseSent = same;
    return same;
  }

  // ---- whole patches ----

  /**
   * Replace the open patch (new, imported). History starts fresh.
   * When Live edit is on and an instrument is connected, send immediately so the first Tone switch or wave pick cannot
   * surprise-replace a sounding patch (e.g. Nice Piano) with a silent or different temp only after that click.
   */
  private open(doc: DocApi, origin: string, syncInstrument = true): void {
    this.liveQueue.clear();
    this.replaceDoc(doc);
    this.baseSent = false;
    this.fullPending = false;
    this.deferredFullSend = false;
    this.group = null;
    this.history.clear();
    this.syncHistory();
    this.origin = origin;
    this.reference = doc.toBlocks();
    this.ab = { a: null, b: null, active: 'a' };
    this.morphBase = null;
    this.sync = 'local';
    this.liveDirty = false;
    this.rev += 1;
    if (syncInstrument) this.syncOpenToInstrument();
  }

  /** Push the just-opened document to temporary memory when Live edit would otherwise wait for the first parameter click. */
  private syncOpenToInstrument(): void {
    if (!this.live || !this.transport.connected || this.kind === 'system') return;
    this.sendPatch();
    reportInfo('Sent the open sound to the instrument (Live edit is on).');
  }

  /** Open a patch from stored blocks (the library). It exists only in the app until saved, so it starts LOCAL. */
  openBlocks(bytes: Uint8Array, origin: string): void {
    try {
      this.open(docFromBlocks(this.kind, bytes), origin);
      this.error = '';
    } catch (e) {
      this.error = e instanceof Error ? e.message : String(e);
    }
  }

  /**
   * A System document that has not been read from an instrument. Everything is at its minimum except the keyboard scale, which starts at equal temperament
   * (0 cents on every note) instead of -64, so a page that has not been read never shows, or offers to write, a scale nobody chose.
   */
  newSystemSettings(): void {
    const doc = newDoc(this.kind);
    for (const c of initChanges(this.catalog, this.kind)) {
      if (!c.block.startsWith('tune')) continue;
      try {
        doc.set(c.block, c.id, c.value);
      } catch {
        // A parameter the document refuses keeps its minimum.
      }
    }
    this.open(doc, 'New System settings');
  }

  newPatch(): void {
    // A new sound starts from the neutral sound, not from all-minimum values (which play nothing). It is the opened
    // baseline, so it neither counts as an edit nor lands in the undo history.
    const doc = newDoc(this.kind);
    for (const c of initChanges(this.catalog, this.kind)) {
      try {
        doc.set(c.block, c.id, c.value);
      } catch {
        // A parameter the document refuses keeps its minimum.
      }
    }
    this.open(doc, `New ${KIND_TITLES[this.kind].toLowerCase()}`);
  }

  /** Apply the XP-60 panel's Patch Initialize / DEFAULT bytes as one undoable edit. */
  initXp60Patch(): void {
    if (this.kind !== 'patch') return;
    const next = PatchDoc.xp60Default();
    const before = this.doc.toBlocks();
    const after = next.toBlocks();
    if (sameBytes(before, after)) {
      next.free();
      return;
    }
    const ownGroup = this.group === null;
    if (ownGroup) this.begin('XP-60 Init Patch');
    this.liveQueue.clear();
    this.replaceDoc(next);
    // The panel defaults contain bytes outside named controls too; send the whole patch when live.
    this.baseSent = false;
    this.afterEdit(changedParams(this.catalog, this.kind, before, after));
    if (ownGroup) this.end();
  }

  loadSyx(bytes: Uint8Array, slot: number, fileName: string): void {
    if (this.kind === 'system') {
      this.error = 'System settings have no slots in a .syx file.';
      return;
    }
    try {
      const doc = this.kind === 'patch' ? PatchDoc.fromSyxSlot(bytes, slot) : docFromSyxSlot(this.kind, bytes, slot);
      this.open(doc, `${fileName}, slot ${slot + 1}`);
      this.error = '';
    } catch (e) {
      this.error = e instanceof Error ? e.message : String(e);
    }
  }

  /** `[slot, name]` for what a `.syx` file holds of this kind: patches, or Performances, or Rhythm Sets. */
  static listSyx(bytes: Uint8Array, kind: DocKindName = 'patch'): Array<[number, string]> {
    const json = kind === 'patch' ? syxNames(bytes) : docSyxNames(kind, bytes);
    return JSON.parse(json) as Array<[number, string]>;
  }

  /** What reading a patch bank file left out: messages skipped as damaged or not XP-60 data, and patch parts reset to initial values. */
  static syxReport(bytes: Uint8Array): { skipped: number; filled: Array<[number, string]> } {
    return JSON.parse(syxReport(bytes)) as { skipped: number; filled: Array<[number, string]> };
  }

  /** Send the whole patch to the instrument's temporary patch, replacing any single edits still waiting. */
  sendPatch(): void {
    if (this.kind === 'system') {
      this.error = 'System settings are written to the instrument with the save ceremony, not live.';
      return;
    }
    if (!this.transport.connected) return;
    try {
      this.liveQueue.replaceAll(splitFrames(cutFrames(this.doc.patchFrames(this.device))));
      this.liveDirty = true;
      this.fullPending = true;
      this.error = '';
    } catch (e) {
      this.error = e instanceof Error ? e.message : String(e);
    }
  }

  /**
   * Send these patch blocks to the instrument's temporary area without opening them in the editor.
   * Used when the Bank screen needs the keys to play a slot that lives only in the app (a loaded bulk or file)
   * and has not been written to User memory yet. Program Change would still play the old User slot.
   */
  auditionBlocks(bytes: Uint8Array): void {
    if (this.kind !== 'patch' || !this.transport.connected) return;
    try {
      const doc = PatchDoc.fromBlocks(bytes);
      try {
        this.liveQueue.replaceAll(splitFrames(cutFrames(doc.patchFrames(this.device))));
        // Not this.doc: leave fullPending/baseSent false (as markStale) so the next edit of the
        // open patch is a whole send, not a single-parameter write on top of the auditioned slot.
        this.liveDirty = true;
        this.fullPending = false;
        this.baseSent = false;
        if (this.sync === 'sent' || this.sync === 'tempVerified' || this.sync === 'edited') this.sync = 'stale';
        this.error = '';
      } finally {
        doc.free();
      }
    } catch (e) {
      this.error = e instanceof Error ? e.message : String(e);
    }
  }

  setLive(on: boolean): void {
    if (this.kind === 'system') return;
    this.live = on;
    if (on) {
      this.sendPatch();
      return;
    }
    // Turning live edit off also stops whatever was still waiting to go out.
    this.liveQueue.clear();
    this.baseSent = false;
    this.fullPending = false;
    this.liveDirty = false;
    if (this.sync === 'sent') this.sync = 'edited';
  }
}
