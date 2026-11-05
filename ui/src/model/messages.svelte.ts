// The one place the app tells the person something: errors, warnings, information, confirmations and validation problems all appear in the bottom-left corner
// of the window and nowhere else. Anything in the app that has something to say calls `report` (directly, or through a service field that reports when it is
// set); no view draws its own message line.

import { untrack } from 'svelte';

export type MessageKind = 'error' | 'warning' | 'info' | 'success';

export interface AppMessage {
  readonly id: number;
  readonly kind: MessageKind;
  readonly text: string;
  /** Bumped when the same message is reported again, so the person sees it renewed. */
  readonly count: number;
}

/** How long each kind stays, in milliseconds; an error stays until it is dismissed or this long has passed. */
export const MESSAGE_LIFETIME: Record<MessageKind, number> = { info: 5000, success: 4500, warning: 9000, error: 20000 };

/** At most this many messages show at once; the oldest give way. */
export const MAX_VISIBLE = 4;

class MessageCentre {
  list = $state<AppMessage[]>([]);
  private nextId = 1;
  private timers = new Map<number, ReturnType<typeof setTimeout>>();

  /** Show a message. The same text again renews the one on screen instead of stacking a copy. */
  report(kind: MessageKind, text: string, lifetime: number = MESSAGE_LIFETIME[kind]): number {
    const clean = text.trim();
    if (!clean) return 0;
    // Reading the list here must not make a caller's $effect depend on it: reporting from an effect would otherwise re-run that effect forever.
    return untrack(() => {
      const same = this.list.find((m) => m.kind === kind && m.text === clean);
      const id = same?.id ?? this.nextId++;
      const message: AppMessage = { id, kind, text: clean, count: (same?.count ?? 0) + 1 };
      this.list = same ? this.list.map((m) => (m.id === id ? message : m)) : [...this.list, message].slice(-MAX_VISIBLE);
      this.arm(id, lifetime);
      return id;
    });
  }

  /** A message that stays until `dismiss` is called (a state that lasts, such as waiting for a controller to move). */
  hold(kind: MessageKind, text: string): number {
    const id = this.report(kind, text, 0);
    return id;
  }

  dismiss(id: number): void {
    this.disarm(id);
    untrack(() => {
      this.list = this.list.filter((m) => m.id !== id);
    });
  }

  clear(): void {
    for (const id of this.timers.keys()) this.disarm(id);
    this.list = [];
  }

  private arm(id: number, lifetime: number): void {
    this.disarm(id);
    if (lifetime > 0) this.timers.set(id, setTimeout(() => this.dismiss(id), lifetime));
  }

  private disarm(id: number): void {
    const t = this.timers.get(id);
    if (t !== undefined) clearTimeout(t);
    this.timers.delete(id);
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
