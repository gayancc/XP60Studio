// The one place the app tells the person something. Every kind (error, warning, info, success) is queued and
// shown one at a time in the bottom hint bar; nothing floats over the main view. Display time for the active
// message is shortened as the queue grows so a backlog clears without blocking work. The clock pauses while a
// control hint covers the status line so a message cannot expire unseen. Anything in the app that has something
// to say calls `report` (directly, or through a service field that reports when it is set); no view draws its
// own message line.

import { untrack } from 'svelte';

export type MessageKind = 'error' | 'warning' | 'info' | 'success';

export interface AppMessage {
  readonly id: number;
  readonly kind: MessageKind;
  readonly text: string;
  /** Bumped when the same message is reported again, so the person sees it renewed. */
  readonly count: number;
  /**
   * Fixed display time in ms when the caller asked for one (`say(..., ms)`). Held messages omit this and stay
   * until `dismiss`. When unset, the armed time is scaled by queue size.
   */
  readonly durationMs?: number;
}

/** Base display time when the queue holds a single message, in milliseconds. */
export const MESSAGE_LIFETIME: Record<MessageKind, number> = {
  info: 4000,
  success: 3500,
  warning: 5000,
  error: 7000,
};

/** Shortest a queued message may stay on screen, even when the queue is long. */
export const MIN_MESSAGE_MS = 1200;

/** At most this many messages wait in the status-bar queue; the oldest non-held give way. */
export const MAX_QUEUE = 16;

/** Display time for a message given how many are waiting (including itself). */
export function lifetimeFor(kind: MessageKind, queueSize: number): number {
  const n = Math.max(1, queueSize);
  return Math.max(MIN_MESSAGE_MS, Math.round(MESSAGE_LIFETIME[kind] / n));
}

class MessageCentre {
  /** FIFO queue: index 0 is the line currently shown in the hint bar. */
  list = $state<AppMessage[]>([]);
  private nextId = 1;
  private timer: ReturnType<typeof setTimeout> | null = null;
  private held = new Set<number>();
  /** When true, the active line's clock is frozen (a control hint is covering the status line). */
  private paused = false;
  private remainingMs = 0;
  private deadline = 0;

  /** The message currently shown in the hint bar, or null when the queue is empty. */
  get current(): AppMessage | null {
    return this.list[0] ?? null;
  }

  /** Text of the current status line, or null when there is none. */
  get status(): string | null {
    return this.current?.text ?? null;
  }

  /** How many messages are waiting, including the one on screen. */
  get queueSize(): number {
    return this.list.length;
  }

  /** True while a hover hint has paused the active message's clock. */
  get isPaused(): boolean {
    return this.paused;
  }

  /** Show a message. The same text again renews the matching queued entry instead of stacking a copy. */
  report(kind: MessageKind, text: string, lifetime?: number): number {
    const clean = text.trim();
    if (!clean) return 0;
    // Reading the list here must not make a caller's $effect depend on it: reporting from an effect would otherwise re-run that effect forever.
    return untrack(() => {
      const same = this.list.find((m) => m.kind === kind && m.text === clean);
      const id = same?.id ?? this.nextId++;
      const durationMs = lifetime !== undefined && lifetime > 0 ? lifetime : same?.durationMs;
      const message: AppMessage = {
        id,
        kind,
        text: clean,
        count: (same?.count ?? 0) + 1,
        ...(durationMs !== undefined ? { durationMs } : {}),
      };
      if (same) {
        this.list = this.list.map((m) => (m.id === id ? message : m));
      } else {
        this.list = this.trimQueue([...this.list, message]);
      }
      // lifetime 0 is a hold: the caller marks it held and it must not get a timer.
      if (lifetime === 0) return id;
      if (this.list[0]?.id === id) this.armCurrent();
      else this.rearmCurrent();
      return id;
    });
  }

  /** A message that stays until `dismiss` is called (a state that lasts, such as waiting for a controller to move). */
  hold(kind: MessageKind, text: string): number {
    const id = this.report(kind, text, 0);
    if (id) {
      this.held.add(id);
      // If this hold replaced a timed line at the head, stop that clock.
      if (this.list[0]?.id === id) this.disarm();
    }
    return id;
  }

  dismiss(id: number): void {
    this.held.delete(id);
    untrack(() => {
      const wasCurrent = this.list[0]?.id === id;
      const next = this.list.filter((m) => m.id !== id);
      if (next.length === this.list.length) return; // already gone (for example dropped from a full queue)
      this.list = next;
      if (wasCurrent) {
        this.remainingMs = 0;
        this.disarm();
        this.armCurrent();
      }
    });
  }

  /** Skip the current line and show the next queued message, if any. */
  advance(): void {
    const cur = this.list[0];
    if (!cur) return;
    if (this.held.has(cur.id)) return;
    this.dismiss(cur.id);
  }

  /** Freeze the active message's remaining time (used while a control hint covers the status line). */
  pause(): void {
    if (this.paused) return;
    this.paused = true;
    if (this.timer !== null) {
      this.remainingMs = Math.max(0, this.deadline - Date.now());
      this.disarm();
    }
  }

  /** Resume after `pause`, keeping whatever time was left. */
  resume(): void {
    if (!this.paused) return;
    this.paused = false;
    const cur = this.list[0];
    if (!cur || this.held.has(cur.id)) return;
    if (this.remainingMs > 0) this.arm(cur.id, this.remainingMs);
    else this.armCurrent();
  }

  clear(): void {
    this.disarm();
    this.held.clear();
    this.paused = false;
    this.remainingMs = 0;
    this.deadline = 0;
    this.list = [];
  }

  /** Drop oldest non-held entries until the queue fits; never drop a held line. */
  private trimQueue(next: AppMessage[]): AppMessage[] {
    const out = [...next];
    while (out.length > MAX_QUEUE) {
      const i = out.findIndex((m) => !this.held.has(m.id));
      if (i < 0) break;
      const [gone] = out.splice(i, 1);
      if (gone) this.held.delete(gone.id);
    }
    return out;
  }

  private displayMs(m: AppMessage): number {
    if (m.durationMs !== undefined) return m.durationMs;
    return lifetimeFor(m.kind, this.list.length);
  }

  private rearmCurrent(): void {
    const cur = this.list[0];
    if (!cur || this.held.has(cur.id) || this.paused) return;
    // A fixed-duration message keeps its remaining time; only queue-scaled messages shrink when the backlog grows.
    if (cur.durationMs !== undefined) return;
    this.armCurrent();
  }

  private armCurrent(): void {
    const cur = this.list[0];
    if (!cur) return;
    if (this.held.has(cur.id)) return;
    this.arm(cur.id, this.displayMs(cur));
  }

  private arm(id: number, lifetime: number): void {
    this.disarm();
    this.remainingMs = 0;
    if (lifetime <= 0 || this.held.has(id)) return;
    if (this.paused) {
      this.remainingMs = lifetime;
      return;
    }
    this.deadline = Date.now() + lifetime;
    this.timer = setTimeout(() => {
      this.timer = null;
      if (this.list[0]?.id !== id) return;
      this.held.delete(id);
      untrack(() => {
        this.list = this.list.filter((m) => m.id !== id);
        this.armCurrent();
      });
    }, lifetime);
  }

  private disarm(): void {
    if (this.timer !== null) clearTimeout(this.timer);
    this.timer = null;
  }
}

export const messages = new MessageCentre();

/** Shorthands used all over the app. */
export const reportError = (text: string): number => messages.report('error', text);
export const reportWarning = (text: string): number => messages.report('warning', text);
export const reportInfo = (text: string): number => messages.report('info', text);
export const reportSuccess = (text: string): number => messages.report('success', text);

/** The text of a thrown value, for an error message. */
export function errorText(e: unknown): string {
  return typeof e === 'string' ? e : e instanceof Error ? e.message : String(e);
}
