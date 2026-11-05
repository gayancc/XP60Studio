// Where live-edit frames go. The editor does not know about MIDI ports: the Tauri shell supplies a transport that
// forwards frames to the instrument; the browser build and the tests use the in-memory one.

export interface LiveTransport {
  readonly label: string;
  /** True when frames actually reach an instrument. */
  readonly connected: boolean;
  send(frame: Uint8Array): void | Promise<void>;
}

/**
 * One gate in front of the real transport. Every document store has its own queue, but there is one MIDI port and the
 * instrument drops writes closer than about 20 ms, so all sends pass through here one at a time with the minimum gap.
 */
export class PacedTransport implements LiveTransport {
  private tail: Promise<void> = Promise.resolve();
  private lastSent = Number.NEGATIVE_INFINITY;

  constructor(
    readonly inner: LiveTransport,
    private gapMs = 20,
    private readonly now: () => number = () => performance.now(),
    private readonly sleep: (ms: number) => Promise<void> = (ms) => new Promise((r) => setTimeout(r, ms)),
  ) {}

  get label(): string {
    return this.inner.label;
  }

  /** The gap between sends; never below the instrument's documented 20 ms. */
  setGap(ms: number): number {
    this.gapMs = Math.min(250, Math.max(20, Math.round(Number.isFinite(ms) ? ms : 20)));
    return this.gapMs;
  }

  get gap(): number {
    return this.gapMs;
  }

  get connected(): boolean {
    return this.inner.connected;
  }

  send(frame: Uint8Array): Promise<void> {
    const run = this.tail.then(async () => {
      const wait = this.lastSent + this.gapMs - this.now();
      if (wait > 0) await this.sleep(wait);
      try {
        await this.inner.send(frame);
      } finally {
        this.lastSent = this.now();
      }
    });
    // A failed send must not block the ones behind it.
    this.tail = run.catch(() => undefined);
    return run;
  }
}

/** Records frames instead of sending them (browser preview, storybook, tests). */
export class MemoryTransport implements LiveTransport {
  readonly label = 'Preview (no instrument)';
  readonly connected = true;
  readonly frames: Uint8Array[] = [];

  send(frame: Uint8Array): void {
    this.frames.push(frame);
    // Keep the preview's memory bounded.
    if (this.frames.length > 2000) this.frames.splice(0, this.frames.length - 2000);
  }
}
