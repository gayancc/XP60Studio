// Undo history: a browsable list of whole-state snapshots. A patch is about 600 bytes, so a snapshot per step is exact and
// far simpler than inverse operations. A whole drag is one entry.

export interface HistoryEntry<S> {
  label: string;
  before: S;
  after: S;
  /** Milliseconds timestamp, for display. */
  at: number;
}

export const DEFAULT_HISTORY_LIMIT = 200;

export class History<S> {
  private items: HistoryEntry<S>[] = [];
  /** Number of entries currently applied; entries past it can be redone. */
  private applied = 0;

  constructor(private readonly limit = DEFAULT_HISTORY_LIMIT) {}

  get entries(): readonly HistoryEntry<S>[] {
    return this.items;
  }

  get position(): number {
    return this.applied;
  }

  get canUndo(): boolean {
    return this.applied > 0;
  }

  get canRedo(): boolean {
    return this.applied < this.items.length;
  }

  /** Record a new step. Anything that could have been redone is discarded. */
  record(label: string, before: S, after: S, at: number): void {
    this.items = this.items.slice(0, this.applied);
    this.items.push({ label, before, after, at });
    if (this.items.length > this.limit) this.items = this.items.slice(this.items.length - this.limit);
    this.applied = this.items.length;
  }

  /** The state to restore, or null when there is nothing to undo. */
  undo(): S | null {
    if (!this.canUndo) return null;
    this.applied -= 1;
    return this.items[this.applied]?.before ?? null;
  }

  redo(): S | null {
    if (!this.canRedo) return null;
    const entry = this.items[this.applied];
    this.applied += 1;
    return entry?.after ?? null;
  }

  /** Move to the state after `n` entries (0 is the state before the first). Returns it, or null if `n` is invalid. */
  jumpTo(n: number): S | null {
    if (!Number.isInteger(n) || n < 0 || n > this.items.length) return null;
    this.applied = n;
    if (n === 0) return this.items[0]?.before ?? null;
    return this.items[n - 1]?.after ?? null;
  }

  clear(): void {
    this.items = [];
    this.applied = 0;
  }
}
