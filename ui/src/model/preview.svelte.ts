// Hover audition (parity row D6): while the pointer rests on a stored patch, send it to the instrument's *temporary*
// patch so it can be played, and when the pointer leaves put back what the instrument had. Only temporary memory is
// touched (never User memory), and only when the user has switched the feature on.

/** What the service needs from the outside, so it can be tested without an instrument. */
export interface PreviewDeps {
  connected: () => boolean;
  /** The instrument's temporary patch as concatenated block bytes, or null when it could not be read. */
  readTemp: () => Promise<Uint8Array | null>;
  /** The stored patch as block bytes, or null when it could not be read. */
  readStored: (id: number) => Promise<Uint8Array | null>;
  /** Send these block bytes to the temporary patch. */
  sendTemp: (blocks: Uint8Array) => Promise<void>;
  /** The temporary patch was replaced and could not be put back: cached copies of it are no longer right. */
  markStale: () => void;
}

/** Wait this long on a row before it sounds, so sweeping the pointer across the list sends nothing. */
export const DWELL_MS = 350;
/** Wait this long after leaving before restoring, so moving to the next row does not bounce through the old sound. */
export const LEAVE_MS = 200;

export class PreviewService {
  /** The user switched hover audition on. Off by default. */
  enabled = $state(false);
  /** The stored patch currently sounding in the temporary area, or null. */
  playing = $state<number | null>(null);
  error = $state('');

  private deps: PreviewDeps | undefined;
  /** The instrument's own temporary patch from before the first preview of this hover session. */
  private saved: Uint8Array | null = null;
  private hasSaved = false;
  private dwell: ReturnType<typeof setTimeout> | undefined;
  private leave: ReturnType<typeof setTimeout> | undefined;
  /** Serialises the instrument work so a restore never overtakes the send it follows. */
  private chain: Promise<void> = Promise.resolve();

  get available(): boolean {
    return this.deps !== undefined;
  }

  attach(deps: PreviewDeps): void {
    this.deps = deps;
  }

  setEnabled(on: boolean): void {
    this.enabled = on;
    if (!on) void this.stop();
  }

  /** The pointer came onto a row. */
  hover(id: number): void {
    clearTimeout(this.leave);
    clearTimeout(this.dwell);
    if (!this.enabled || !this.deps?.connected() || this.playing === id) return;
    this.dwell = setTimeout(() => void this.run(() => this.start(id)), DWELL_MS);
  }

  /** The pointer left the list rows. */
  unhover(): void {
    clearTimeout(this.dwell);
    clearTimeout(this.leave);
    if (this.hasSaved || this.playing !== null) this.leave = setTimeout(() => void this.stop(), LEAVE_MS);
  }

  /** Put back what the instrument had, now. Resolves when the instrument has been written to. */
  stop(): Promise<void> {
    clearTimeout(this.dwell);
    clearTimeout(this.leave);
    return this.run(() => this.restore());
  }

  private run(step: () => Promise<void>): Promise<void> {
    this.chain = this.chain.then(step, step);
    return this.chain;
  }

  private async start(id: number): Promise<void> {
    const deps = this.deps;
    if (!deps || !this.enabled || !deps.connected()) return;
    try {
      if (!this.hasSaved) {
        // Remember what is there before the first send; without it nothing could be put back.
        const current = await deps.readTemp();
        if (!current) {
          this.error = 'Preview needs the instrument\'s current sound first, and it could not be read.';
          return;
        }
        this.saved = current;
        this.hasSaved = true;
      }
      const stored = await deps.readStored(id);
      if (!stored) return;
      await deps.sendTemp(stored);
      this.playing = id;
      this.error = '';
    } catch (e) {
      this.error = e instanceof Error ? e.message : String(e);
    }
  }

  private async restore(): Promise<void> {
    const deps = this.deps;
    const saved = this.saved;
    const touched = this.hasSaved;
    this.saved = null;
    this.hasSaved = false;
    this.playing = null;
    if (!deps || !touched) return;
    try {
      if (!saved || !deps.connected()) throw new Error('not connected');
      await deps.sendTemp(saved);
    } catch {
      // The sound that was there cannot be put back: say so rather than show a copy that is no longer true.
      deps.markStale();
      this.error = 'The instrument\'s previous sound could not be restored; select it again on the panel.';
    }
  }
}

export const preview = new PreviewService();
