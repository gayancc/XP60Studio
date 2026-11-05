// Driving parameters from a hardware controller (parity rows A6 and A7). The instrument or a controller keyboard sends
// Control Change messages; the Rust side reports each controller that moved; this service maps them onto parameters
// with the bindings the user made by "learning" (move a controller while a parameter is armed).

import { invoke } from '@tauri-apps/api/core';
import { paramInfo, type BlockName, type Catalog, type DocKindName } from './catalog';
import { bind, describe, find, hears, parseBindings, scaleValue, serializeBindings, unbind, type Binding, type SourceKind } from './controlMap';
import type { PatchStore } from './store.svelte';

export interface ControlMove {
  channel: number;
  kind: SourceKind;
  /** The controller number (0..127) or the NRPN/RPN number (0..16383). */
  number: number;
  value: number;
  /** The largest value this source carries: 127 for a CC, 16383 for an NRPN or RPN. */
  max: number;
}

/** What the service needs from the outside, so it can be tested without an instrument. */
export interface ControlDeps {
  connected: () => boolean;
  busy: () => boolean;
  listen: (on: boolean) => Promise<void>;
  poll: () => Promise<ControlMove[]>;
}

export interface ControlTarget {
  kind: DocKindName;
  block: BlockName;
  id: string;
  /** Name shown to the user ("Cutoff Frequency"). */
  name: string;
}

const STORAGE_KEY = 'xppatch.controlmap.v1';
/** A burst of moves is one undo step; it ends after this long without a move. */
const GROUP_IDLE_MS = 400;

export const tauriControlDeps = (connected: () => boolean, busy: () => boolean): ControlDeps => ({
  connected,
  busy,
  listen: (on) => invoke('midi_listen_controls', { on }),
  poll: () => invoke<ControlMove[]>('midi_poll_controls'),
});

function readStorage(): string | null {
  try {
    return globalThis.localStorage?.getItem(STORAGE_KEY) ?? null;
  } catch {
    return null;
  }
}

function writeStorage(text: string): void {
  try {
    globalThis.localStorage?.setItem(STORAGE_KEY, text);
  } catch {
    // The map still works for this run; it is only not remembered.
  }
}

export class ControlLink {
  bindings = $state<Binding[]>([]);
  /** The parameter waiting for a controller to be moved, or null. */
  learning = $state<ControlTarget | null>(null);
  notice = $state('');

  private deps: ControlDeps | undefined;
  private stores: Partial<Record<DocKindName, PatchStore>> = {};
  private catalog: Catalog | undefined;
  private listening = false;
  private readonly groups = new Map<DocKindName, ReturnType<typeof setTimeout>>();

  /** True when an instrument connection exists to listen on (the desktop app). */
  get available(): boolean {
    return this.deps !== undefined;
  }

  attach(deps: ControlDeps, stores: Partial<Record<DocKindName, PatchStore>>, catalog: Catalog): void {
    this.deps = deps;
    this.stores = stores;
    this.catalog = catalog;
    this.bindings = parseBindings(readStorage());
  }

  /** Text for a parameter's menu: the controller it is linked to, or null. */
  linked(target: Pick<Binding, 'kind' | 'block' | 'id'>): string | null {
    const b = find(this.bindings, target);
    return b ? describe(b) : null;
  }

  learn(target: ControlTarget): void {
    this.learning = target;
    this.notice = `Move a controller to link it to ${target.name}.`;
  }

  cancelLearn(): void {
    if (this.learning) this.notice = '';
    this.learning = null;
  }

  unlink(target: Pick<Binding, 'kind' | 'block' | 'id'>): void {
    this.bindings = unbind(this.bindings, target);
    this.persist();
    this.notice = '';
  }

  clearAll(): void {
    this.bindings = [];
    this.persist();
    this.notice = '';
  }

  private persist(): void {
    writeStorage(serializeBindings(this.bindings));
  }

  /** Poll the instrument for controller moves until the returned function is called. */
  start(everyMs = 40): () => void {
    let stopped = false;
    let timer: ReturnType<typeof setTimeout> | undefined;
    const loop = async (): Promise<void> => {
      await this.tick();
      if (!stopped) timer = setTimeout(loop, everyMs);
    };
    timer = setTimeout(loop, everyMs);
    return () => {
      stopped = true;
      clearTimeout(timer);
      for (const t of this.groups.values()) clearTimeout(t);
    };
  }

  /** One round: switch listening on or off as needed, then apply what moved. Exposed for tests. */
  async tick(): Promise<void> {
    const deps = this.deps;
    if (!deps) return;
    const want = deps.connected() && (this.bindings.length > 0 || this.learning !== null);
    if (want !== this.listening) {
      try {
        await deps.listen(want);
        this.listening = want;
      } catch {
        return;
      }
    }
    if (!this.listening || deps.busy()) return;
    let moves: ControlMove[];
    try {
      moves = await deps.poll();
    } catch {
      return;
    }
    for (const m of moves) this.handle(m);
  }

  private handle(m: ControlMove): void {
    const learning = this.learning;
    if (learning) {
      if (m.kind === 'cc' && (m.number === 0 || m.number === 32)) return;
      const b: Binding = { kind: learning.kind, block: learning.block, id: learning.id, channel: null, source: m.kind, controller: m.number };
      this.bindings = bind(this.bindings, b);
      this.persist();
      this.learning = null;
      this.notice = `${learning.name} is linked to ${describe(b)}.`;
      return;
    }
    for (const b of this.bindings) if (hears(b, m.channel, m.kind, m.number)) this.apply(b, m.value, m.max);
  }

  private apply(b: Binding, controllerValue: number, sourceMax: number): void {
    const store = this.stores[b.kind];
    const info = this.catalog ? paramInfo(this.catalog, b.kind, b.block, b.id) : undefined;
    if (!store || !info) return;
    const value = scaleValue(controllerValue, info.min, info.max, sourceMax);
    if (store.get(b.block, b.id) === value) return;
    // The first move of a burst opens an undo group and every further move extends it.
    const open = this.groups.get(b.kind);
    if (open === undefined) store.begin(store.label(b.block, b.id));
    else clearTimeout(open);
    store.set(b.block, b.id, value);
    this.groups.set(
      b.kind,
      setTimeout(() => {
        this.groups.delete(b.kind);
        store.end();
      }, GROUP_IDLE_MS),
    );
  }
}

export const controls = new ControlLink();
