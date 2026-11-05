import { beforeAll, describe, expect, it } from 'vitest';
import { readFileSync } from 'node:fs';
import { catalogJson, initSync } from '../wasm/pkg/xppatch.js';
import { parseCatalog, blockInfo, type Catalog, type DocKindName } from './catalog';
import { newDoc } from './kinds';
import {
  Rng,
  copyBlock,
  diff,
  initChanges,
  isProtected,
  isRoutingProtected,
  morph,
  mutate,
  pasteClip,
  randomize,
  readParam,
  type Change,
} from './tools';

let catalog: Catalog;

beforeAll(() => {
  initSync({ module: readFileSync('src/wasm/pkg/xppatch_bg.wasm') });
  catalog = parseCatalog(catalogJson());
});

/** Apply changes to a fresh codec document, so a test also proves every value is accepted by the Rust ranges. */
function apply(kind: DocKindName, changes: Change[], base?: Uint8Array): ReturnType<typeof newDoc> {
  const doc = newDoc(kind);
  if (base) {
    // Load a base state by replaying it parameter by parameter is overkill; tests that need one use fromBlocks.
  }
  for (const c of changes) doc.set(c.block, c.id, c.value);
  return doc;
}

const zeroGet = (): number => 0;

describe('Rng', () => {
  it('is deterministic for a seed and different for another', () => {
    const a = new Rng(42);
    const b = new Rng(42);
    const c = new Rng(43);
    const seqA = [a.next(), a.next(), a.next()];
    expect(seqA).toEqual([b.next(), b.next(), b.next()]);
    expect(seqA).not.toEqual([c.next(), c.next(), c.next()]);
  });
  it('stays in range and survives a zero seed', () => {
    const r = new Rng(0);
    for (let i = 0; i < 1000; i++) {
      const f = r.next();
      expect(f).toBeGreaterThanOrEqual(0);
      expect(f).toBeLessThan(1);
      const n = r.int(3, 9);
      expect(n).toBeGreaterThanOrEqual(3);
      expect(n).toBeLessThanOrEqual(9);
    }
  });
});

describe('protection', () => {
  it('protects names and routing, not sound parameters', () => {
    const find = (id: string) => catalog.tone.params.find((p) => p.id === id)!;
    expect(isProtected(find('output_assign'))).toBe(true);
    expect(isProtected(catalog.common.params.find((p) => p.id === 'patch_name_1')!)).toBe(true);
    expect(isProtected(catalog.common.params.find((p) => p.id === 'efx_output_assign')!)).toBe(true);
    expect(isProtected(find('keyboard_range_lower'))).toBe(true);
    expect(isProtected(find('velocity_range_upper'))).toBe(true);
    expect(isProtected(find('tone_switch'))).toBe(true);
    expect(isProtected(find('cutoff_frequency'))).toBe(false);
    expect(isProtected(find('tone_level'))).toBe(true); // mix levels: a roll must not silence a tone
    expect(isProtected(find('resonance'))).toBe(false);
  });
});

describe('randomize', () => {
  it('is reproducible from its seed and always inside the Rust ranges', () => {
    const a = randomize(catalog, 'patch', 'all', zeroGet, 1, 7);
    const b = randomize(catalog, 'patch', 'all', zeroGet, 1, 7);
    expect(a).toEqual(b);
    expect(a.length).toBeGreaterThan(300);
    expect(() => apply('patch', a)).not.toThrow(); // every value passes the codec's range check
  });

  it('never touches protected parameters', () => {
    const changes = randomize(catalog, 'patch', 'all', zeroGet, 1, 99);
    const info = (c: Change) => blockInfo(catalog, 'patch', c.block).params.find((p) => p.id === c.id)!;
    expect(changes.every((c) => !isProtected(info(c)))).toBe(true);
  });

  it('strength scales how much changes, and zero changes nothing', () => {
    const none = randomize(catalog, 'patch', 'all', zeroGet, 0, 5);
    const some = randomize(catalog, 'patch', 'all', zeroGet, 0.2, 5);
    const all = randomize(catalog, 'patch', 'all', zeroGet, 1, 5);
    expect(none).toHaveLength(0);
    expect(some.length).toBeGreaterThan(20);
    expect(some.length).toBeLessThan(all.length);
  });

  it('a seed picks the same values whatever the strength (the weaker run is a subset)', () => {
    const weak = randomize(catalog, 'patch', 'all', zeroGet, 0.3, 11);
    const strong = randomize(catalog, 'patch', 'all', zeroGet, 0.9, 11);
    for (const c of weak) expect(strong).toContainEqual(c);
  });

  it('can be limited to one block', () => {
    const changes = randomize(catalog, 'patch', 'tone2', zeroGet, 1, 3);
    expect(changes.length).toBeGreaterThan(50);
    expect(changes.every((c) => c.block === 'tone2')).toBe(true);
  });

  it('works for performances, rhythm sets and system settings', () => {
    for (const kind of ['performance', 'rhythm', 'system'] as const) {
      const changes = randomize(catalog, kind, 'all', zeroGet, 1, 21);
      expect(changes.length).toBeGreaterThan(20);
      expect(() => apply(kind, changes)).not.toThrow();
    }
  });
});

describe('mutate', () => {
  it('moves values by at most the given share of the range', () => {
    const doc = newDoc('patch');
    const start = (b: string, id: string): number => doc.get(b, id);
    const changes = mutate(catalog, 'patch', 'tone1', start, 0.1, 17);
    expect(changes.length).toBeGreaterThan(10);
    for (const c of changes) {
      const p = catalog.tone.params.find((x) => x.id === c.id)!;
      expect(Math.abs(c.value - start(c.block, c.id))).toBeLessThanOrEqual(Math.ceil(0.1 * (p.max - p.min)));
    }
    expect(() => apply('patch', changes)).not.toThrow();
  });

  it('with no amount changes nothing', () => {
    const doc = newDoc('patch');
    expect(mutate(catalog, 'patch', 'all', (b, id) => doc.get(b, id), 0, 1)).toHaveLength(0);
  });
});

describe('morph', () => {
  const tone = (level: number, wave: number): Uint8Array => {
    const d = newDoc('patch');
    d.set('tone1', 'tone_level', level);
    d.set('tone1', 'lfo1_waveform', wave);
    return d.toBlocks();
  };

  it('is A at 0 and B at 1, interpolates numbers and switches choices halfway', () => {
    const a = tone(10, 1);
    const b = tone(110, 6);
    const get = (): number => 0;
    const value = (t: number, id: string): number | undefined =>
      morph(catalog, 'patch', a, b, t, get).find((c) => c.block === 'tone1' && c.id === id)?.value;
    expect(value(0, 'tone_level')).toBe(10);
    expect(value(1, 'tone_level')).toBe(110);
    expect(value(0.5, 'tone_level')).toBe(60);
    expect(value(0.25, 'tone_level')).toBe(35);
    expect(value(0.49, 'lfo1_waveform')).toBe(1);
    expect(value(0.5, 'lfo1_waveform')).toBe(6);
  });

  it('keeps names and routing as they are and clamps t', () => {
    const a = newDoc('patch');
    a.setName('Alpha');
    const b = newDoc('patch');
    b.setName('Beta');
    const changes = morph(catalog, 'patch', a.toBlocks(), b.toBlocks(), 0.5, zeroGet);
    expect(changes.some((c) => c.id.startsWith('patch_name_'))).toBe(false);
    expect(morph(catalog, 'patch', tone(10, 1), tone(110, 1), 5, zeroGet).find((c) => c.id === 'tone_level')?.value).toBe(110);
  });
});

describe('diff and readParam', () => {
  it('lists exactly what differs, with names', () => {
    const a = newDoc('patch');
    const b = newDoc('patch');
    b.set('tone3', 'resonance', 55);
    b.set('common', 'patch_level', 9);
    const d = diff(catalog, 'patch', a.toBlocks(), b.toBlocks());
    expect(d.map((e) => [e.block, e.id, e.a, e.b])).toEqual([
      ['common', 'patch_level', 0, 9],
      ['tone3', 'resonance', 0, 55],
    ]);
    expect(d[1]?.name).toBe('Resonance');
    expect(diff(catalog, 'patch', a.toBlocks(), a.toBlocks())).toEqual([]);
  });

  it('reads 7-bit and nibble parameters straight from a snapshot', () => {
    const d = newDoc('patch');
    d.set('common', 'patch_tempo', 133);
    d.set('tone1', 'tone_level', 99);
    const snap = d.toBlocks();
    expect(readParam(snap, 0, catalog.common.params.find((p) => p.id === 'patch_tempo')!)).toBe(133);
    expect(readParam(snap, 73, catalog.tone.params.find((p) => p.id === 'tone_level')!)).toBe(99);
  });
});

describe('init', () => {
  it('produces values the codec accepts, with neutral signed values, open levels and the first item on', () => {
    for (const kind of ['patch', 'performance', 'rhythm', 'system'] as const) {
      const changes = initChanges(catalog, kind);
      const doc = apply(kind, changes);
      expect(changes.length).toBeGreaterThan(10);
      if (kind === 'patch') {
        expect(doc.get('tone1', 'tone_switch')).toBe(1);
        expect(doc.get('tone2', 'tone_switch')).toBe(0);
        expect(doc.get('tone1', 'tone_level')).toBe(127);
        expect(doc.format('tone1', 'coarse_tune')).toBe('0');
        expect(doc.format('tone1', 'tone_pan')).toBe('0');
        expect(doc.format('common', 'patch_pan')).toBe('0');
        expect(doc.get('tone1', 'cutoff_frequency')).toBe(127);
        expect(doc.get('tone1', 'level_envelope_level_1')).toBe(127);
        // A range that starts empty would play nothing.
        expect(doc.get('tone1', 'keyboard_range_lower')).toBe(0);
        expect(doc.get('tone1', 'keyboard_range_upper')).toBe(127);
        expect(doc.get('tone1', 'velocity_range_upper')).toBe(127);
      }
    }
  });
});

describe('names of performances and rhythm sets', () => {
  it('are never changed by Init, Randomize or Mutate, nor copied with a block', () => {
    for (const kind of ['performance', 'rhythm'] as const) {
      const doc = newDoc(kind);
      doc.setName('Keep This');
      const get = (b: string, id: string): number => doc.get(b, id);
      const isName = (id: string): boolean => /_name_\d+$/.test(id);
      expect(initChanges(catalog, kind).some((c) => isName(c.id))).toBe(false);
      expect(randomize(catalog, kind, 'all', get, 1, 7).some((c) => isName(c.id))).toBe(false);
      expect(mutate(catalog, kind, 'all', get, 1, 7).some((c) => isName(c.id))).toBe(false);
      const clip = copyBlock(catalog, kind, 'common', get, 'Common');
      expect(clip.values.some((v) => isName(v.id))).toBe(false);
    }
  });
});

describe('copy and paste', () => {
  it('copies a whole block or only some parameters, and pastes into another block', () => {
    const doc = newDoc('patch');
    doc.set('tone1', 'cutoff_frequency', 80);
    doc.set('tone1', 'resonance', 40);
    doc.set('tone1', 'tone_level', 100);
    const get = (b: string, id: string): number => doc.get(b, id);
    const whole = copyBlock(catalog, 'patch', 'tone1', get, 'Tone 1');
    expect(whole.values).toHaveLength(128);
    const tvf = copyBlock(catalog, 'patch', 'tone1', get, 'Tone 1 TVF', ['cutoff_frequency', 'resonance']);
    expect(tvf.values).toEqual([
      { id: 'cutoff_frequency', value: 80 },
      { id: 'resonance', value: 40 },
    ]);
    const into = pasteClip(catalog, 'patch', tvf, 'tone3', get);
    expect(into).toEqual([
      { block: 'tone3', id: 'cutoff_frequency', value: 80 },
      { block: 'tone3', id: 'resonance', value: 40 },
    ]);
    for (const c of into) doc.set(c.block, c.id, c.value);
    expect(pasteClip(catalog, 'patch', tvf, 'tone3', get)).toEqual([]); // already equal
  });

  it('refuses a clip from another kind of document and skips values outside the target range', () => {
    const doc = newDoc('patch');
    const get = (b: string, id: string): number => doc.get(b, id);
    const clip = copyBlock(catalog, 'patch', 'tone1', get, 'Tone 1');
    expect(pasteClip(catalog, 'performance', clip, 'part1', get)).toEqual([]);
    const bad = { kind: 'patch' as const, label: 'x', values: [{ id: 'tone_level', value: 500 }, { id: 'nope', value: 1 }] };
    expect(pasteClip(catalog, 'patch', bad, 'tone2', get)).toEqual([]);
  });

  it('never copies names', () => {
    const doc = newDoc('patch');
    const get = (b: string, id: string): number => doc.get(b, id);
    const clip = copyBlock(catalog, 'patch', 'common', get, 'Patch');
    expect(clip.values.some((v) => v.id.startsWith('patch_name_'))).toBe(false);
  });

  it('Randomize across many seeds never silences or routes away a Tone', () => {
    const doc = newDoc('patch');
    for (const t of ['tone1', 'tone2', 'tone3', 'tone4']) {
      doc.set(t, 'tone_switch', 1);
      doc.set(t, 'tone_level', 100);
    }
    const get = (b: string, id: string): number => doc.get(b, id);
    for (let seed = 1; seed <= 200; seed++) {
      const changes = randomize(catalog, 'patch', 'all', get, 1, seed);
      for (const c of changes) {
        expect(['tone_switch', 'tone_level', 'output_assign']).not.toContain(c.id);
        expect(c.id.startsWith('keyboard_range') || c.id.startsWith('velocity_range')).toBe(false);
      }
    }
  });

  it('Morph blends levels (unlike Randomize) but still leaves routing alone', () => {
    const a = newDoc('patch');
    const b = newDoc('patch');
    a.set('tone1', 'output_assign', 1);
    b.set('tone1', 'output_assign', 2);
    b.set('tone1', 'tone_level', 100);
    const ch = morph(catalog, 'patch', a.toBlocks(), b.toBlocks(), 0.5, zeroGet);
    expect(ch.find((c) => c.id === 'tone_level')?.value).toBe(50);
    expect(ch.some((c) => c.id === 'output_assign')).toBe(false);
    expect(isRoutingProtected(catalog.tone.params.find((p) => p.id === 'tone_level')!)).toBe(false);
  });
});
