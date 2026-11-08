import { afterEach, beforeAll, beforeEach, describe, expect, it, vi } from 'vitest';
import { readFileSync } from 'node:fs';
import { catalogJson, initSync, paramAddress } from '../wasm/pkg/xppatch.js';
import { parseCatalog, TONE_BLOCKS, type Catalog } from './catalog';
import { PatchStore } from './store.svelte';
import {
  assertToneAddressMap,
  otherTonesUnchanged,
  toneBytes,
  TONE_COMPACT_OFFSETS,
  TONE_COMPACT_SPAN,
} from './toneIsolation';
import { MemoryTransport } from './transport';

let catalog: Catalog;

beforeAll(() => {
  initSync({ module: readFileSync('src/wasm/pkg/xppatch_bg.wasm') });
  catalog = parseCatalog(catalogJson());
});

beforeEach(() => vi.useFakeTimers());
afterEach(() => vi.useRealTimers());

function make(): { store: PatchStore; transport: MemoryTransport } {
  const transport = new MemoryTransport();
  const store = new PatchStore(catalog, transport, 0x10, { now: () => Date.now() });
  return { store, transport };
}

function prime(store: PatchStore, transport: MemoryTransport): void {
  store.sendPatch();
  vi.advanceTimersByTime(1000);
  transport.frames.length = 0;
}

/** Probe parameters that cover wave / filter / amp / LFO across all four Tones. */
const PROBES = ['tone_level', 'cutoff_frequency', 'resonance', 'lfo1_rate', 'coarse_tune'] as const;

describe('Step 4: Tone 1–4 isolation verification', () => {
  it('maps each Tone parameter to a distinct SysEx address window (Patch Base layout)', () => {
    for (const id of PROBES) {
      const rows = assertToneAddressMap(id);
      expect(rows).toHaveLength(4);
      rows.forEach((row, i) => {
        expect(row.block).toBe(TONE_BLOCKS[i]);
        expect(row.address).toBeGreaterThanOrEqual(TONE_COMPACT_OFFSETS[i]!);
        expect(row.address).toBeLessThan(TONE_COMPACT_OFFSETS[i]! + TONE_COMPACT_SPAN);
        expect(row.address).toBe(paramAddress(row.block, id));
      });
    }
  });

  it('traces before/after state independently for Tone 1, 2, 3, and 4', () => {
    const { store } = make();
    store.toneLog.clear();
    const targets = TONE_BLOCKS.map((block, i) => ({ block, id: 'tone_level' as const, value: 10 + i * 20 }));

    for (const t of targets) {
      const before = store.get(t.block, t.id);
      expect(before).not.toBe(t.value);
      store.set(t.block, t.id, t.value);
      expect(store.get(t.block, t.id)).toBe(t.value);
      const entry = store.toneLog.entries.at(-1);
      expect(entry).toMatchObject({ block: t.block, id: t.id, before, after: t.value });
      expect(entry?.address).toBe(paramAddress(t.block, t.id));
    }

    // Each Tone kept its own value; none were overwritten by a later Tone write.
    targets.forEach((t) => expect(store.get(t.block, t.id)).toBe(t.value));
    expect(store.toneLog.entries).toHaveLength(4);
    expect(new Set(store.toneLog.entries.map((e) => e.block))).toEqual(new Set(TONE_BLOCKS));
    expect(new Set(store.toneLog.entries.map((e) => e.address)).size).toBe(4);
  });

  it('changing any probe on one Tone leaves the other three Tone byte blocks untouched', () => {
    const { store } = make();
    // Seed distinct baselines so a silent bleed would be visible.
    TONE_BLOCKS.forEach((block, i) => {
      store.set(block, 'tone_level', 5 + i);
      store.set(block, 'cutoff_frequency', 20 + i * 3);
      store.set(block, 'resonance', 30 + i * 4);
    });

    for (const edited of TONE_BLOCKS) {
      for (const id of PROBES) {
        const before = store.snapshot();
        const next = (store.get(edited, id) + 7) % 100;
        store.set(edited, id, next === store.get(edited, id) ? next + 1 : next);
        const after = store.snapshot();
        expect(otherTonesUnchanged(catalog, before, after, edited)).toBe(true);
        expect(store.error).toBe('');
        // Edited tone bytes actually changed for this parameter's storage.
        expect([...toneBytes(catalog, before, edited)].join(',')).not.toBe(
          [...toneBytes(catalog, after, edited)].join(','),
        );
      }
    }
  });

  it('live DT1 frames for the same parameter name target four different temp addresses', () => {
    const { store, transport } = make();
    prime(store, transport);

    const framesByTone: Record<string, Uint8Array> = {};
    for (const block of TONE_BLOCKS) {
      store.set(block, 'tone_level', 40 + TONE_BLOCKS.indexOf(block));
      vi.advanceTimersByTime(50);
      const frame = transport.frames.at(-1);
      expect(frame).toBeTruthy();
      framesByTone[block] = frame!;
    }

    const addrs = TONE_BLOCKS.map((block) => {
      const f = framesByTone[block]!;
      // F0 41 dev 6A 12 a0 a1 a2 a3 data… cc F7
      expect([...f.slice(0, 5)]).toEqual([0xf0, 0x41, 0x10, 0x6a, 0x12]);
      return `${f[5]}-${f[6]}-${f[7]}-${f[8]}`;
    });
    expect(new Set(addrs).size).toBe(4);
    expect(addrs).toEqual(['3-0-16-101', '3-0-18-101', '3-0-20-101', '3-0-22-101']);

    // Confirm queue coalesce keys would not collide either.
    const keys = TONE_BLOCKS.map((b) => paramAddress(b, 'tone_level'));
    expect(new Set(keys).size).toBe(4);
  });

  it('rapid interleaved edits across all four Tones never cross-write state or addresses', () => {
    const { store, transport } = make();
    prime(store, transport);
    store.toneLog.clear();

    const plan = Array.from({ length: 40 }, (_, n) => {
      const block = TONE_BLOCKS[n % 4]!;
      return { block, id: PROBES[n % PROBES.length]!, value: 11 + n };
    });

    for (const step of plan) {
      const others = Object.fromEntries(
        TONE_BLOCKS.filter((b) => b !== step.block).map((b) => [b, store.get(b, step.id)]),
      );
      store.set(step.block, step.id, Math.min(127, step.value));
      for (const [b, v] of Object.entries(others)) {
        expect(store.get(b as (typeof TONE_BLOCKS)[number], step.id)).toBe(v);
      }
    }

    // Every logged mutation's address matches its declared block; final state matches the last write per path.
    for (const e of store.toneLog.entries) {
      expect(e.address).toBe(paramAddress(e.block, e.id));
      expect(e.after).not.toBe(e.before);
    }
    const lastByPath = new Map<string, number>();
    for (const e of store.toneLog.entries) lastByPath.set(`${e.block}:${e.id}`, e.after);
    for (const [key, after] of lastByPath) {
      const [block, id] = key.split(':') as [typeof TONE_BLOCKS[number], string];
      expect(store.get(block, id)).toBe(after);
    }

    vi.advanceTimersByTime(5000);
    // Each live frame's address lane (byte 7) stays in the Patch Base tone windows {0x10,0x12,0x14,0x16}.
    for (const f of transport.frames) {
      if (f[4] !== 0x12) continue;
      expect([0x10, 0x12, 0x14, 0x16]).toContain(f[7]);
    }
  });
});
