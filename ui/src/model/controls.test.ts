import { afterEach, beforeAll, beforeEach, describe, expect, it, vi } from 'vitest';
import { readFileSync } from 'node:fs';
import { catalogJson, initSync } from '../wasm/pkg/xppatch.js';
import { parseCatalog, type Catalog } from './catalog';
import { ControlLink, type ControlDeps, type ControlMove } from './controls.svelte';
import { PatchStore } from './store.svelte';
import { MemoryTransport } from './transport';

let catalog: Catalog;

beforeAll(() => {
  initSync({ module: readFileSync('src/wasm/pkg/xppatch_bg.wasm') });
  catalog = parseCatalog(catalogJson());
});

beforeEach(() => {
  vi.useFakeTimers();
  globalThis.localStorage?.clear();
});
afterEach(() => vi.useRealTimers());

function setup(): { link: ControlLink; store: PatchStore; send: (...m: ControlMove[]) => Promise<void>; listen: ReturnType<typeof vi.fn>; state: { connected: boolean; busy: boolean } } {
  const store = new PatchStore(catalog, new MemoryTransport(), 0x10, { now: () => Date.now() });
  const state = { connected: true, busy: false };
  let queue: ControlMove[] = [];
  const listen = vi.fn(async () => {});
  const deps: ControlDeps = {
    connected: () => state.connected,
    busy: () => state.busy,
    listen,
    poll: async () => {
      const q = queue;
      queue = [];
      return q;
    },
  };
  const link = new ControlLink();
  link.attach(deps, { patch: store }, catalog);
  const send = async (...m: ControlMove[]): Promise<void> => {
    queue.push(...m);
    await link.tick();
  };
  return { link, store, send, listen, state };
}

const target = { kind: 'patch', block: 'tone1', id: 'cutoff_frequency', name: 'Cutoff Frequency' } as const;
const move = (controller: number, value: number, channel = 0): ControlMove => ({ channel, kind: 'cc', number: controller, value, max: 127 });
const nrpn = (number: number, value: number): ControlMove => ({ channel: 0, kind: 'nrpn', number, value, max: 16383 });

describe('driving parameters from a controller', () => {
  it('learns the next controller that moves, then follows it', async () => {
    const { link, store, send } = setup();
    link.learn(target);
    expect(link.learning).not.toBeNull();
    await send(move(74, 30));
    expect(link.learning).toBeNull();
    expect(link.linked(target)).toBe('CC 74');
    expect(link.notice).toContain('linked to CC 74');
    // Learning consumed that move: the parameter did not change from it.
    expect(store.get('tone1', 'cutoff_frequency')).not.toBe(30);
    await send(move(74, 127));
    expect(store.get('tone1', 'cutoff_frequency')).toBe(127);
    await send(move(74, 0));
    expect(store.get('tone1', 'cutoff_frequency')).toBe(0);
  });

  it('turns one sweep into one undo step', async () => {
    const { link, store, send } = setup();
    link.learn(target);
    await send(move(74, 1));
    await send(move(74, 20));
    await send(move(74, 60));
    await send(move(74, 100));
    expect(store.items).toHaveLength(0); // still one open group
    await vi.advanceTimersByTimeAsync(500);
    expect(store.items).toHaveLength(1);
    await send(move(74, 10)); // a new sweep after a pause is a new step
    await vi.advanceTimersByTimeAsync(500);
    expect(store.items).toHaveLength(2);
    store.undo();
    expect(store.get('tone1', 'cutoff_frequency')).toBe(100);
  });

  it('scales over the parameter range and ignores controllers it is not bound to', async () => {
    const { link, store, send } = setup();
    const before = store.get('tone1', 'cutoff_frequency');
    link.learn({ ...target, id: 'coarse_tune', name: 'Coarse Tune' });
    await send(move(1, 5));
    await send(move(2, 99)); // some other controller
    expect(store.get('tone1', 'cutoff_frequency')).toBe(before);
    await send(move(1, 127));
    expect(store.get('tone1', 'coarse_tune')).toBe(store.catalog.tone.params.find((p) => p.id === 'coarse_tune')?.max);
  });

  it('does not listen until a binding or a learn exists, and stops when they are gone', async () => {
    const { link, listen, send } = setup();
    await send();
    expect(listen).not.toHaveBeenCalled();
    link.learn(target);
    await send();
    expect(listen).toHaveBeenLastCalledWith(true);
    link.cancelLearn();
    await send();
    expect(listen).toHaveBeenLastCalledWith(false);
    expect(link.notice).toBe('');
  });

  it('stays quiet while the instrument is busy or disconnected', async () => {
    const { link, store, send, state } = setup();
    link.learn(target);
    await send(move(74, 1));
    const v = store.get('tone1', 'cutoff_frequency');
    state.busy = true;
    await send(move(74, 3));
    expect(store.get('tone1', 'cutoff_frequency')).toBe(v);
    state.busy = false;
    state.connected = false;
    await send(move(74, 3));
    expect(store.get('tone1', 'cutoff_frequency')).toBe(v);
  });

  it('can unlink one parameter or clear everything, and remembers the map', async () => {
    const { link, send } = setup();
    link.learn(target);
    await send(move(74, 1));
    const again = new ControlLink();
    again.attach({ connected: () => false, busy: () => false, listen: async () => {}, poll: async () => [] }, {}, catalog);
    expect(again.linked(target)).toBe('CC 74');
    link.unlink(target);
    expect(link.linked(target)).toBeNull();
    link.learn(target);
    await send(move(71, 1));
    link.clearAll();
    expect(link.bindings).toEqual([]);
  });

  it('never learns Bank Select', async () => {
    const { link, send } = setup();
    link.learn(target);
    await send(move(0, 5), move(32, 5));
    expect(link.learning).not.toBeNull();
    expect(link.bindings).toEqual([]);
  });

  it('learns and follows an NRPN over its 14-bit range', async () => {
    const { link, store, send } = setup();
    link.learn(target);
    await send(nrpn(130, 5000));
    expect(link.linked(target)).toBe('NRPN 130');
    await send(nrpn(130, 16383));
    expect(store.get('tone1', 'cutoff_frequency')).toBe(127);
    await send(nrpn(130, 0));
    expect(store.get('tone1', 'cutoff_frequency')).toBe(0);
    // A plain CC with a similar number is a different source and does nothing.
    await send(move(2, 99));
    expect(store.get('tone1', 'cutoff_frequency')).toBe(0);
  });
});
