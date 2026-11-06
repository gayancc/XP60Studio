import { describe, expect, it } from 'vitest';
import { ExpansionSettings, MAX_BOARDS, parseInstalled, type Storage2 } from './expansions.svelte';

const memory = (initial: string | null = null): Storage2 => {
  const m = new Map<string, string>();
  if (initial !== null) m.set('xppatch.installedExpansions', initial);
  return { getItem: (k) => m.get(k) ?? null, setItem: (k, v) => void m.set(k, v) };
};

describe('installed expansion boards', () => {
  it('starts with none', () => {
    expect(new ExpansionSettings(memory()).installed).toEqual([]);
  });

  it('keeps the boards in number order and remembers them', () => {
    const store = memory();
    const s = new ExpansionSettings(store);
    s.set(14, true);
    s.set(2, true);
    expect(s.installed).toEqual([2, 14]);
    expect(new ExpansionSettings(store).installed).toEqual([2, 14]);
  });

  it('puts the boards in slots EXP-A to EXP-D in the order they were ticked, and remembers that order', () => {
    const store = memory();
    const s = new ExpansionSettings(store);
    s.set(14, true);
    s.set(2, true);
    s.set(9, true);
    expect(s.slots).toEqual([14, 2, 9]);
    expect(s.slotOf(2)).toBe(1);
    expect(s.slotOf(5)).toBe(-1);
    s.set(14, false);
    expect(s.slots).toEqual([2, 9]);
    expect(new ExpansionSettings(store).slots).toEqual([2, 9]);
  });

  it('refuses a fifth board, because the instrument has four slots, and says why', () => {
    const s = new ExpansionSettings(memory());
    for (const n of [1, 2, 3, 4]) s.set(n, true);
    expect(s.full).toBe(true);
    s.set(5, true);
    expect(s.installed).toEqual([1, 2, 3, 4]);
    expect(s.notice).toContain(`${MAX_BOARDS} expansion slots`);
    s.set(4, false);
    s.set(5, true);
    expect(s.installed).toEqual([1, 2, 3, 5]);
    expect(s.notice).toBe('');
  });

  it('ignores damaged saved values', () => {
    expect(parseInstalled('not json')).toEqual([]);
    expect(parseInstalled('{"a":1}')).toEqual([]);
    expect(parseInstalled('[1,2,2,"x",0,500,3,4,5,6]')).toEqual([1, 2, 3, 4]);
  });

  it('works when nothing can be stored', () => {
    const s = new ExpansionSettings(undefined);
    s.set(1, true);
    expect(s.installed).toEqual([1]);
  });
});
