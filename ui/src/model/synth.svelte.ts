// The connection to the instrument, from the UI's side. In the desktop app these calls go to Rust (the tested MIDI
// client); in the browser preview there is no instrument and the service says so.

import { invoke, isTauri } from '@tauri-apps/api/core';
import type { DocKindName } from './catalog';
import type { LiveTransport } from './transport';
import { reportError, reportInfo, reportSuccess, reportWarning } from './messages.svelte';

export interface Ports {
  inputs: string[];
  outputs: string[];
}

export interface Connected {
  /** As shown on the instrument (17 to 32). */
  device_number: number;
  /** The raw ID used in messages (16 to 31). */
  device_id: number;
  patch_name: string;
}

/** Sends live-edit frames through Rust. Frames are only accepted while connected. */
export class TauriTransport implements LiveTransport {
  readonly label = 'XP-60';
  private on = false;

  get connected(): boolean {
    return this.on;
  }

  setConnected(on: boolean): void {
    this.on = on;
  }

  async send(frame: Uint8Array): Promise<void> {
    await invoke('midi_send_frame', { frame: Array.from(frame) });
  }
}

export const runningInDesktopApp = (): boolean => isTauri();

/** How long a completion notice stays up. */
export const NOTICE_MS = 6000;

function message(e: unknown): string {
  return typeof e === 'string' ? e : e instanceof Error ? e.message : String(e);
}

export class SynthService {
  ports = $state<Ports>({ inputs: [], outputs: [] });
  info = $state<Connected | null>(null);
  busy = $state(false);
  #error = $state('');
  get error(): string {
    return this.#error;
  }
  /** Setting text here also shows it in the bottom-left corner with every other message. */
  set error(text: string) {
    this.#error = text;
    if (text) reportError(text);
  }
  /** What the last completed read or check found ("Fetched ..."), shown for a few seconds. */
  #notice = $state('');
  get notice(): string {
    return this.#notice;
  }
  /** Setting text here also shows it in the bottom-left corner with every other message. */
  set notice(text: string) {
    this.#notice = text;
    if (text) reportInfo(text);
  }
  private noticeTimer: ReturnType<typeof setTimeout> | undefined;
  /** Milliseconds between writes. */
  pacing = $state(20);

  private readonly listeners: Array<(info: Connected | null) => void> = [];

  /** Say that something finished, for NOTICE_MS. A newer notice replaces an older one. */
  say(text: string, ms = NOTICE_MS): void {
    clearTimeout(this.noticeTimer);
    this.notice = text;
    this.noticeTimer = setTimeout(() => (this.notice = ''), ms);
  }

  constructor(
    private readonly transport: TauriTransport,
    /** The shared pacing gate in front of the transport, if any, so the longer gap applies on this side too. */
    private readonly gate?: { setGap(ms: number): number },
  ) {}

  /** Called whenever the connection opens (with what answered) or closes (null). */
  onconnection(listener: (info: Connected | null) => void): void {
    this.listeners.push(listener);
  }

  private notify(): void {
    this.listeners.forEach((l) => l(this.info));
  }

  get connected(): boolean {
    return this.info !== null;
  }

  async refreshPorts(): Promise<void> {
    try {
      this.ports = await invoke<Ports>('midi_ports');
      this.error = '';
    } catch (e) {
      this.error = message(e);
    }
  }

  async connect(input: string, output: string, device: number): Promise<void> {
    this.busy = true;
    try {
      this.info = await invoke<Connected>('midi_connect', { input, output, device });
      this.transport.setConnected(true);
      this.error = '';
    } catch (e) {
      this.info = null;
      this.transport.setConnected(false);
      this.error = message(e);
    } finally {
      this.busy = false;
    }
    this.notify();
  }

  async disconnect(): Promise<void> {
    try {
      await invoke('midi_disconnect');
    } catch (e) {
      this.error = message(e);
    } finally {
      this.info = null;
      this.transport.setConnected(false);
      this.notify();
    }
  }

  /** The instrument's temporary patch, performance or rhythm set as concatenated block bytes, or null (with `error` set). */
  async fetchTemp(kind: DocKindName = 'patch'): Promise<Uint8Array | null> {
    this.busy = true;
    try {
      const bytes = await invoke<number[]>('midi_fetch_temp_doc', { kind });
      this.error = '';
      return new Uint8Array(bytes);
    } catch (e) {
      this.error = message(e);
      return null;
    } finally {
      this.busy = false;
    }
  }

  /** Play or release a note on the instrument (`channel` 0 to 15). Notes write nothing. */
  async note(channel: number, note: number, velocity: number, on: boolean): Promise<void> {
    if (!this.connected) return;
    try {
      await invoke('midi_note', { channel, note, velocity, on });
    } catch (e) {
      this.error = message(e);
    }
  }

  /**
   * Select a sound on the instrument with Bank Select and Program Change (`channel` 0 to 15, `program` 0 to 127). Writes
   * nothing to memory; the instrument's temporary sound is replaced. Returns whether it was sent.
   */
  async selectSound(channel: number, msb: number, lsb: number, program: number): Promise<boolean> {
    if (!this.connected) return false;
    try {
      await invoke('midi_select_sound', { channel, msb, lsb, program });
      return true;
    } catch (e) {
      this.error = message(e);
      return false;
    }
  }

  /** Gap between writes in ms (20 to 250). Returns the value in use. */
  async setPacing(ms: number): Promise<number> {
    try {
      const used = await invoke<number>('midi_set_pacing', { gapMs: ms });
      this.pacing = used;
      this.gate?.setGap(used);
      return used;
    } catch (e) {
      this.error = message(e);
      return this.pacing;
    }
  }

  async allNotesOff(channel: number): Promise<void> {
    if (!this.connected) return;
    try {
      await invoke('midi_all_notes_off', { channel });
    } catch (e) {
      this.error = message(e);
    }
  }

  /**
   * Poll the instrument every `everyMs` for a panel change and call `onStale` once per change. One poll serves every
   * document kind, so the event is never consumed by a view that is not on screen. Returns a function that stops it.
   */
  startStalePolling(onStale: () => void, everyMs = 600): () => void {
    let stopped = false;
    let timer: ReturnType<typeof setTimeout> | undefined;
    const tick = async (): Promise<void> => {
      if (await this.pollStale()) onStale();
      if (!stopped) timer = setTimeout(tick, everyMs);
    };
    timer = setTimeout(tick, everyMs);
    return () => {
      stopped = true;
      clearTimeout(timer);
    };
  }

  /** True once if the sound was changed from the instrument's panel. */
  async pollStale(): Promise<boolean> {
    if (!this.connected || this.busy) return false;
    try {
      return await invoke<boolean>('midi_poll_stale');
    } catch {
      return false;
    }
  }
}
