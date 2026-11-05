import { beforeEach, describe, expect, it } from 'vitest';
import {
  arrangement,
  BUILT_IN,
  DEFAULT_ARRANGEMENT,
  layouts,
  MAX_NAME,
  MAX_NAMED,
  parseArrangement,
  parseLayouts,
  tabIn,
} from './layouts.svelte';

beforeEach(() => {
  globalThis.localStorage?.clear();
  layouts.reset();
  for (const l of [...layouts.custom]) layouts.remove(l.name);
  layouts.message = '';
});

describe('reading an arrangement from storage', () => {
  it('keeps good fields and replaces every bad one with its default', () => {
    const a = parseArrangement({ mode: 'bank', patchTab: 4, patchSide: -1, perfPage: 1.5, rhythmTab: '2', systemTab: 9999, bankTab: 2 });
    expect(parseArrangement({ toneView: 1 }).toneView).toBe(1);
    expect(parseArrangement({ toneView: -3 }).toneView).toBe(0);
    expect(a).toEqual({ ...DEFAULT_ARRANGEMENT, mode: 'bank', patchTab: 4, bankTab: 2 });
  });

  it('is the default for anything that is not an object, and an unknown mode is ignored', () => {
    for (const bad of [null, undefined, 7, 'x', []]) expect(parseArrangement(bad)).toEqual(DEFAULT_ARRANGEMENT);
    expect(parseArrangement({ mode: 'nope' }).mode).toBe('patch');
  });

  it('a tab index is always one that exists', () => {
    expect(tabIn(5, 3)).toBe(2);
    expect(tabIn(-1, 3)).toBe(0);
    expect(tabIn(1, 3)).toBe(1);
    expect(tabIn(4, 0)).toBe(0);
  });
});

describe('reading named layouts', () => {
  const entry = (name: unknown, mode = 'rhythm'): unknown => ({ name, arrangement: { mode } });

  it('drops damaged entries, repeated names and built-in names, and cleans the rest', () => {
    const text = JSON.stringify([entry('  My   rack  '), entry('my rack'), entry('Sound design'), entry(''), entry(5), null, 'x']);
    const got = parseLayouts(text);
    expect(got.map((l) => l.name)).toEqual(['My rack']);
    expect(got[0]?.arrangement.mode).toBe('rhythm');
  });

  it('keeps at most the limit, cuts long names, and survives bad text', () => {
    const many = Array.from({ length: 30 }, (_, i) => entry(`L${i}`));
    expect(parseLayouts(JSON.stringify(many))).toHaveLength(MAX_NAMED);
    expect(parseLayouts(JSON.stringify([entry('x'.repeat(80))]))[0]?.name).toHaveLength(MAX_NAME);
    expect(parseLayouts('not json')).toEqual([]);
    expect(parseLayouts(null)).toEqual([]);
    expect(parseLayouts('{}')).toEqual([]);
  });
});

describe('saved layouts', () => {
  it('saves what is on screen under a name and brings it back', () => {
    arrangement.mode = 'rhythm';
    arrangement.rhythmTab = 3;
    expect(layouts.saveAs('  Drums  ')).toBe(true);
    layouts.reset();
    expect(arrangement.mode).toBe('patch');
    expect(layouts.apply('drums')).toBe(true);
    expect(arrangement.mode).toBe('rhythm');
    expect(arrangement.rhythmTab).toBe(3);
  });

  it('remembers the layouts between runs', () => {
    arrangement.systemTab = 5;
    layouts.saveAs('Setup');
    const stored = JSON.parse(globalThis.localStorage.getItem('xppatch.layouts.v1') ?? '[]') as Array<{ name: string }>;
    expect(stored.map((l) => l.name)).toEqual(['Setup']);
  });

  it('replaces a layout saved under the same name instead of adding a second', () => {
    arrangement.bankPage = 1;
    layouts.saveAs('Same');
    arrangement.bankPage = 3;
    layouts.saveAs('same');
    expect(layouts.custom).toHaveLength(1);
    expect(layouts.custom[0]?.arrangement.bankPage).toBe(3);
  });

  it('refuses an empty name, a built-in name and a thirteenth layout', () => {
    expect(layouts.saveAs('   ')).toBe(false);
    expect(layouts.message).toContain('name');
    expect(layouts.saveAs('Key zones')).toBe(false);
    for (let i = 0; i < MAX_NAMED; i += 1) expect(layouts.saveAs(`Layout ${i}`)).toBe(true);
    expect(layouts.saveAs('One too many')).toBe(false);
    expect(layouts.custom).toHaveLength(MAX_NAMED);
  });

  it('built-in layouts can be applied but not deleted', () => {
    const zones = BUILT_IN.find((b) => b.name === 'Key zones');
    expect(zones).toBeDefined();
    expect(layouts.apply('Key zones')).toBe(true);
    expect(arrangement.patchTab).toBe(0);
    expect(arrangement.patchSide).toBe(1);
    expect(layouts.remove('Key zones')).toBe(false);
    expect(layouts.all.some((l) => l.name === 'Key zones')).toBe(true);
  });

  it('deletes a saved layout, and applying a name that does not exist changes nothing', () => {
    layouts.saveAs('Temp');
    expect(layouts.remove('temp')).toBe(true);
    expect(layouts.custom).toHaveLength(0);
    arrangement.mode = 'system';
    expect(layouts.apply('Temp')).toBe(false);
    expect(arrangement.mode).toBe('system');
  });

  it('writes the current arrangement for the next run', () => {
    arrangement.mode = 'bank';
    arrangement.bankTab = 2;
    layouts.persistCurrent();
    const saved = parseArrangement(JSON.parse(globalThis.localStorage.getItem('xppatch.arrangement.v1') ?? '{}'));
    expect(saved.mode).toBe('bank');
    expect(saved.bankTab).toBe(2);
  });
});
