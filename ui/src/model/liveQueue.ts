// The live-edit send queue: debounced, coalesced, paced, and honest about failures. Moving a control never produces one
// message per pixel; a parameter with a message still waiting just has that message replaced by the newest value, and
// messages leave at most one per `minGapMs` (the XP-60 drops writes that arrive closer than about 20 ms).
//
// A send that fails is retried a few times. A frame that still fails is counted as lost and the queue stays in the
// `failed` state until it is cleared or replaced by a whole-patch send, so the UI can never claim that something was
// sent when it was not.

export type QueueState = 'ok' | 'behind' | 'failed';

export interface QueueStats {
  pending: number;
  /** Sends started but not yet settled. */
  inFlight: number;
  state: QueueState;
  sent: number;
  failed: number;
  /** Frames given up on after the retries. */
  lost: number;
}

export interface LiveQueueOptions {
  send: (frame: Uint8Array) => void | Promise<void>;
  now: () => number;
  setTimer: (fn: () => void, ms: number) => unknown;
  clearTimer: (handle: unknown) => void;
  minGapMs?: number;
  /** Pending count at which the queue reports itself as behind. */
  behindAt?: number;
  /** Attempts per frame before it is counted as lost. */
  maxAttempts?: number;
  onstats?: (stats: QueueStats) => void;
}

export class LiveQueue {
  private readonly pendingFrames = new Map<number, Uint8Array>();
  private readonly attempts = new Map<number, number>();
  private timer: unknown = null;
  private lastSent = Number.NEGATIVE_INFINITY;
  private sentCount = 0;
  private failedCount = 0;
  private lostCount = 0;
  private inFlight = 0;
  private lastFailed = false;
  private idleWaiters: Array<() => void> = [];
  private readonly minGap: number;
  private readonly behindAt: number;
  private readonly maxAttempts: number;

  constructor(private readonly o: LiveQueueOptions) {
    this.minGap = o.minGapMs ?? 20;
    this.behindAt = o.behindAt ?? 8;
    this.maxAttempts = o.maxAttempts ?? 3;
  }

  get stats(): QueueStats {
    const pending = this.pendingFrames.size;
    const state: QueueState =
      this.lostCount > 0 || this.lastFailed ? 'failed' : pending >= this.behindAt ? 'behind' : 'ok';
    return {
      pending,
      inFlight: this.inFlight,
      state,
      sent: this.sentCount,
      failed: this.failedCount,
      lost: this.lostCount,
    };
  }

  /** Resolves when nothing is waiting and nothing is in flight. Optional `timeoutMs` resolves anyway so callers cannot hang forever on a stuck MIDI send. */
  whenIdle(timeoutMs?: number): Promise<'idle' | 'timeout'> {
    if (this.pendingFrames.size === 0 && this.inFlight === 0) return Promise.resolve('idle');
    return new Promise((resolve) => {
      let settled = false;
      const finish = (result: 'idle' | 'timeout'): void => {
        if (settled) return;
        settled = true;
        resolve(result);
      };
      this.idleWaiters.push(() => finish('idle'));
      if (timeoutMs !== undefined && timeoutMs >= 0) {
        this.o.setTimer(() => finish('timeout'), timeoutMs);
      }
    });
  }

  /** Queue `frame` for `key` (its location in the instrument). A frame already waiting for that key is replaced. */
  push(key: number, frame: Uint8Array): void {
    this.pendingFrames.set(key, frame);
    this.attempts.delete(key);
    this.emit();
    this.schedule();
  }

  /** Replace everything waiting with an ordered list of frames (a whole-patch send supersedes single edits). */
  replaceAll(frames: readonly Uint8Array[]): void {
    this.pendingFrames.clear();
    this.attempts.clear();
    this.lostCount = 0;
    this.lastFailed = false;
    frames.forEach((f, i) => this.pendingFrames.set(-1 - i, f));
    this.emit();
    this.schedule();
  }

  /** Drop everything waiting and forget past failures. In-flight sends still finish. */
  clear(): void {
    this.pendingFrames.clear();
    this.attempts.clear();
    this.lostCount = 0;
    this.lastFailed = false;
    if (this.timer !== null) {
      this.o.clearTimer(this.timer);
      this.timer = null;
    }
    this.emit();
  }

  /** Drop frames not yet started (e.g. cancel a bank audition before Program Change). */
  cancelPending(): void {
    this.pendingFrames.clear();
    this.attempts.clear();
    if (this.timer !== null) {
      this.o.clearTimer(this.timer);
      this.timer = null;
    }
    this.emit();
  }

  private emit(): void {
    this.o.onstats?.(this.stats);
    if (this.pendingFrames.size === 0 && this.inFlight === 0 && this.idleWaiters.length > 0) {
      const waiters = this.idleWaiters;
      this.idleWaiters = [];
      waiters.forEach((w) => w());
    }
  }

  private schedule(): void {
    if (this.timer !== null || this.pendingFrames.size === 0) return;
    const wait = Math.max(0, this.lastSent + this.minGap - this.o.now());
    this.timer = this.o.setTimer(() => this.run(), wait);
  }

  private run(): void {
    this.timer = null;
    const first = this.pendingFrames.entries().next();
    if (first.done) return;
    const [key, frame] = first.value;
    this.pendingFrames.delete(key);
    this.lastSent = this.o.now();
    this.inFlight += 1;
    try {
      const result = this.o.send(frame);
      if (result instanceof Promise) {
        result.then(
          () => this.settle(true, key, frame),
          () => this.settle(false, key, frame),
        );
      } else {
        this.settle(true, key, frame);
      }
    } catch {
      this.settle(false, key, frame);
    }
    this.schedule();
  }

  private settle(ok: boolean, key: number, frame: Uint8Array): void {
    this.inFlight -= 1;
    if (ok) {
      this.sentCount += 1;
      this.lastFailed = false;
      this.attempts.delete(key);
    } else {
      this.failedCount += 1;
      this.lastFailed = true;
      const tried = (this.attempts.get(key) ?? 0) + 1;
      if (tried < this.maxAttempts) {
        this.attempts.set(key, tried);
        // A newer value for the same parameter wins; otherwise try this one again.
        if (!this.pendingFrames.has(key)) this.pendingFrames.set(key, frame);
        this.schedule();
      } else {
        this.attempts.delete(key);
        this.lostCount += 1;
      }
    }
    this.emit();
  }
}
