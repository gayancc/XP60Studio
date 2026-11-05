import { beforeAll, describe, expect, it } from 'vitest';
import { readFileSync } from 'node:fs';
import { initSync, catalogJson } from '../wasm/pkg/xppatch.js';
import { parseCatalog, type Catalog, type ParamInfo } from './catalog';
import { bipolarOrigin, controlFor } from './controlKind';
import { History } from './history';
import {
  ENVELOPES,
  commonSectionOf,
  envelopeParamIds,
  toneSectionOf,
} from './layout';
import { LiveQueue } from './liveQueue';

let catalog: Catalog;

beforeAll(() => {
  initSync({ module: readFileSync('src/wasm/pkg/xppatch_bg.wasm') });
  catalog = parseCatalog(catalogJson());
});

const find = (block: 'common' | 'tone', id: string): ParamInfo => {
  const p = (block === 'common' ? catalog.common : catalog.tone).params.find((x) => x.id === id);
  if (!p) throw new Error(`no ${id}`);
  return p;
};

describe('catalogue from the Rust codec', () => {
  it('has every patch parameter', () => {
    expect(catalog.common.params).toHaveLength(72);
    expect(catalog.tone.params).toHaveLength(128);
  });
});

describe('layout covers every parameter exactly once', () => {
  it('every tone parameter belongs to a section', () => {
    const orphans = catalog.tone.params.filter((p) => !toneSectionOf(p.id)).map((p) => p.id);
    expect(orphans).toEqual([]);
  });
  it('every common parameter belongs to a section', () => {
    const orphans = catalog.common.params.filter((p) => !commonSectionOf(p.id)).map((p) => p.id);
    expect(orphans).toEqual([]);
  });
  it('envelope parameters all exist and sit in the section their graph belongs to', () => {
    for (const spec of ENVELOPES) {
      for (const id of envelopeParamIds(spec)) {
        expect(find('tone', id), id).toBeDefined();
        expect(toneSectionOf(id), id).toBe(spec.key);
      }
    }
  });
  it('the sections hold sensible counts', () => {
    const count = (s: string): number => catalog.tone.params.filter((p) => toneSectionOf(p.id) === s).length;
    expect(count('controllers')).toBe(5 + 24);
    expect(count('lfo1')).toBe(8);
    expect(count('lfo2')).toBe(8);
    expect(count('mixer')).toBe(3);
  });
});

describe('control choice', () => {
  it('uses a switch for OFF/ON, a drop-down for long lists, knobs for numbers', () => {
    expect(controlFor(find('tone', 'tone_switch')).type).toBe('toggle');
    expect(controlFor(find('tone', 'controller_1_destination_1')).type).toBe('dropdown');
    expect(controlFor(find('tone', 'lfo1_waveform')).type).toBe('dropdown');
    expect(controlFor(find('tone', 'filter_type')).type).toBe('segmented');
    expect(controlFor(find('tone', 'cutoff_frequency')).type).toBe('knob');
  });
  it('signed and pan knobs have an origin at zero or centre', () => {
    expect(bipolarOrigin(find('tone', 'cutoff_frequency'))).toBeNull();
    expect(bipolarOrigin(find('tone', 'tone_pan'))).toBeCloseTo(64 / 127);
    expect(bipolarOrigin(find('tone', 'pitch_lfo1_depth'))).toBeCloseTo(0.5);
    expect(bipolarOrigin(find('tone', 'coarse_tune'))).toBeCloseTo(0.5);
    expect(bipolarOrigin(find('tone', 'pitch_envelope_velocity_sens'))).toBeCloseTo(100 / 250);
  });
});

describe('History', () => {
  it('undoes, redoes and drops the redo tail on a new step', () => {
    const h = new History<number>();
    h.record('a', 0, 1, 1);
    h.record('b', 1, 2, 2);
    expect(h.undo()).toBe(1);
    expect(h.undo()).toBe(0);
    expect(h.undo()).toBeNull();
    expect(h.redo()).toBe(1);
    h.record('c', 1, 9, 3);
    expect(h.canRedo).toBe(false);
    expect(h.entries.map((e) => e.label)).toEqual(['a', 'c']);
  });
  it('jumps to any point in the list', () => {
    const h = new History<number>();
    h.record('a', 0, 1, 1);
    h.record('b', 1, 2, 2);
    h.record('c', 2, 3, 3);
    expect(h.jumpTo(1)).toBe(1);
    expect(h.position).toBe(1);
    expect(h.jumpTo(0)).toBe(0);
    expect(h.jumpTo(3)).toBe(3);
    expect(h.jumpTo(4)).toBeNull();
    expect(h.jumpTo(-1)).toBeNull();
  });
  it('keeps only the newest entries past its limit', () => {
    const h = new History<number>(3);
    for (let i = 0; i < 10; i++) h.record(`s${i}`, i, i + 1, i);
    expect(h.entries.map((e) => e.label)).toEqual(['s7', 's8', 's9']);
    expect(h.position).toBe(3);
  });
});

class Harness {
  t = 0;
  sent: Array<{ at: number; frame: number[] }> = [];
  timers: Array<{ at: number; fn: () => void; id: number }> = [];
  nextId = 1;
  fail = false;
  queue = new LiveQueue({
    send: (f) => {
      if (this.fail) throw new Error('boom');
      this.sent.push({ at: this.t, frame: [...f] });
    },
    now: () => this.t,
    setTimer: (fn, ms) => {
      const id = this.nextId++;
      this.timers.push({ at: this.t + ms, fn, id });
      return id;
    },
    clearTimer: (h) => {
      this.timers = this.timers.filter((x) => x.id !== h);
    },
  });
  advance(ms: number): void {
    const end = this.t + ms;
    for (;;) {
      this.timers.sort((a, b) => a.at - b.at);
      const next = this.timers[0];
      if (!next || next.at > end) break;
      this.timers.shift();
      this.t = next.at;
      next.fn();
    }
    this.t = end;
  }
}

describe('LiveQueue', () => {
  it('sends the first message at once and paces the rest 20 ms apart', () => {
    const h = new Harness();
    h.queue.push(1, new Uint8Array([1]));
    h.queue.push(2, new Uint8Array([2]));
    h.queue.push(3, new Uint8Array([3]));
    h.advance(100);
    expect(h.sent.map((s) => s.at)).toEqual([0, 20, 40]);
    expect(h.sent.map((s) => s.frame[0])).toEqual([1, 2, 3]);
  });
  it('coalesces: a drag over one parameter sends only its latest value', () => {
    const h = new Harness();
    h.queue.push(1, new Uint8Array([1]));
    h.advance(0);
    for (let v = 10; v < 60; v++) h.queue.push(2, new Uint8Array([v]));
    h.advance(200);
    expect(h.sent.map((s) => s.frame[0])).toEqual([1, 59]);
  });
  it('reports pending, behind, and sent counts', () => {
    const h = new Harness();
    for (let i = 0; i < 10; i++) h.queue.push(i, new Uint8Array([i]));
    expect(h.queue.stats.state).toBe('behind');
    h.advance(1000);
    expect(h.queue.stats).toMatchObject({ pending: 0, state: 'ok', sent: 10, failed: 0 });
  });
  it('a failing send is reported, later sends continue, and clear resets it', () => {
    const h = new Harness();
    h.fail = true;
    h.queue.push(1, new Uint8Array([1]));
    h.advance(0);
    expect(h.queue.stats.state).toBe('failed');
    expect(h.queue.stats.failed).toBe(1);
    h.fail = false;
    h.queue.push(2, new Uint8Array([2]));
    h.advance(50);
    expect(h.queue.stats.state).toBe('ok');
    h.fail = true;
    h.queue.push(3, new Uint8Array([3]));
    h.advance(50);
    h.queue.clear();
    expect(h.queue.stats.state).toBe('ok');
  });
  it('a whole-patch send replaces waiting single edits and keeps its own order', () => {
    const h = new Harness();
    h.queue.push(5, new Uint8Array([5]));
    h.queue.push(6, new Uint8Array([6]));
    h.queue.replaceAll([new Uint8Array([100]), new Uint8Array([101]), new Uint8Array([102])]);
    h.advance(200);
    expect(h.sent.map((s) => s.frame[0])).toEqual([100, 101, 102]);
  });
  it('clear drops everything waiting', () => {
    const h = new Harness();
    h.queue.push(1, new Uint8Array([1]));
    h.queue.push(2, new Uint8Array([2]));
    h.queue.clear();
    h.advance(100);
    expect(h.sent).toEqual([]);
  });
});

describe('LiveQueue failures', () => {
  it('retries a failed frame and delivers it', () => {
    const h = new Harness();
    h.fail = true;
    h.queue.push(1, new Uint8Array([1]));
    h.advance(0);
    h.fail = false;
    h.advance(200);
    expect(h.sent.map((s) => s.frame[0])).toEqual([1]);
    expect(h.queue.stats).toMatchObject({ state: 'ok', lost: 0, inFlight: 0 });
  });

  it('gives up after three tries and stays failed until cleared', () => {
    const h = new Harness();
    h.fail = true;
    h.queue.push(1, new Uint8Array([1]));
    h.advance(500);
    expect(h.queue.stats).toMatchObject({ state: 'failed', lost: 1, failed: 3 });
    h.fail = false;
    h.queue.push(2, new Uint8Array([2]));
    h.advance(100);
    expect(h.queue.stats.state).toBe('failed'); // a later success does not hide the lost frame
    h.queue.clear();
    expect(h.queue.stats.state).toBe('ok');
  });

  it('a newer value for the same key wins over a retry of the old one', () => {
    const h = new Harness();
    h.fail = true;
    h.queue.push(1, new Uint8Array([1]));
    h.advance(0);
    h.fail = false;
    h.queue.push(1, new Uint8Array([2]));
    h.advance(200);
    expect(h.sent.map((s) => s.frame[0])).toEqual([2]);
  });

  it('counts frames in flight until an asynchronous send settles', async () => {
    let release: () => void = () => undefined;
    const q = new LiveQueue({
      send: () => new Promise<void>((r) => (release = r)),
      now: () => 0,
      setTimer: (fn) => {
        fn();
        return 1;
      },
      clearTimer: () => undefined,
    });
    q.push(1, new Uint8Array([1]));
    expect(q.stats.inFlight).toBe(1);
    const idle = q.whenIdle();
    release();
    await idle;
    expect(q.stats).toMatchObject({ inFlight: 0, sent: 1 });
  });
});

describe('PacedTransport', () => {
  it('serialises sends from several queues with the minimum gap', async () => {
    const { PacedTransport } = await import('./transport');
    let t = 0;
    const sentAt: number[] = [];
    const inner = {
      label: 'x',
      connected: true,
      send: (): void => {
        sentAt.push(t);
      },
    };
    const gate = new PacedTransport(inner, 20, () => t, async (ms) => {
      t += ms;
    });
    await Promise.all([1, 2, 3, 4].map(() => gate.send(new Uint8Array([1]))));
    expect(sentAt).toEqual([0, 20, 40, 60]);
  });

  it('a failed send does not block the ones behind it', async () => {
    const { PacedTransport } = await import('./transport');
    let n = 0;
    const inner = {
      label: 'x',
      connected: true,
      send: (): void => {
        n += 1;
        if (n === 1) throw new Error('boom');
      },
    };
    const gate = new PacedTransport(inner, 0, () => 0, async () => undefined);
    await expect(gate.send(new Uint8Array([1]))).rejects.toThrow('boom');
    await gate.send(new Uint8Array([2]));
    expect(n).toBe(2);
  });
});
