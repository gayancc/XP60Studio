import { describe, expect, it } from 'vitest';
import {
  DEFAULT_KEYS,
  conflicts,
  effectiveKeys,
  fuzzyScore,
  isAcceptableKey,
  keyOf,
  matchCommand,
  sanitizeOverrides,
  search,
} from './shortcuts';

const ev = (key: string, mods: Partial<{ ctrlKey: boolean; metaKey: boolean; shiftKey: boolean; altKey: boolean }> = {}) => ({
  key,
  ctrlKey: false,
  metaKey: false,
  shiftKey: false,
  altKey: false,
  ...mods,
});

describe('keyOf', () => {
  it('normalises letters, modifiers and function keys', () => {
    expect(keyOf(ev('z', { ctrlKey: true }))).toBe('Ctrl+Z');
    expect(keyOf(ev('Z', { ctrlKey: true, shiftKey: true }))).toBe('Ctrl+Shift+Z');
    expect(keyOf(ev('k', { metaKey: true }))).toBe('Ctrl+K');
    expect(keyOf(ev('F2'))).toBe('F2');
    expect(keyOf(ev(' ', { altKey: true }))).toBe('Alt+Space');
  });
  it('ignores a bare modifier', () => {
    expect(keyOf(ev('Control', { ctrlKey: true }))).toBeNull();
    expect(keyOf(ev('Shift', { shiftKey: true }))).toBeNull();
  });
});

describe('acceptable keys', () => {
  it('needs Ctrl or Alt, or a function key, so typing never fires a command', () => {
    expect(isAcceptableKey('Ctrl+Z')).toBe(true);
    expect(isAcceptableKey('Alt+X')).toBe(true);
    expect(isAcceptableKey('F7')).toBe(true);
    expect(isAcceptableKey('Shift+F7')).toBe(true);
    expect(isAcceptableKey('')).toBe(true);
    expect(isAcceptableKey('Z')).toBe(false);
    expect(isAcceptableKey('Shift+Z')).toBe(false);
    expect(isAcceptableKey('F13')).toBe(false);
  });
});

describe('overrides', () => {
  const known = new Set(Object.keys(DEFAULT_KEYS));
  it('keeps well-formed overrides for known commands and drops the rest', () => {
    const out = sanitizeOverrides(
      { undo: 'Ctrl+U', bogus: 'Ctrl+Q', redo: 'Z', paste: 42, init: '', palette: 'x'.repeat(50) },
      known,
    );
    expect(out).toEqual({ undo: 'Ctrl+U', init: '' });
  });
  it('survives damaged input', () => {
    expect(sanitizeOverrides(null, known)).toEqual({});
    expect(sanitizeOverrides([1, 2], known)).toEqual({});
    expect(sanitizeOverrides('nope', known)).toEqual({});
  });
  it('overrides replace defaults and an empty string removes a shortcut', () => {
    const map = effectiveKeys({ undo: 'Ctrl+U', init: '' });
    expect(map.undo).toBe('Ctrl+U');
    expect(map.init).toBe('');
    expect(map.redo).toBe('Ctrl+Y');
  });
});

describe('conflicts and matching', () => {
  it('the defaults have no conflicts', () => {
    expect(conflicts(DEFAULT_KEYS)).toEqual([]);
  });
  it('reports a shared key and runs neither command', () => {
    const map = effectiveKeys({ init: 'Ctrl+Z' });
    expect(conflicts(map)).toEqual([['Ctrl+Z', ['undo', 'init']]]);
    expect(matchCommand(map, ev('z', { ctrlKey: true }))).toBeNull();
  });
  it('finds the command for a key', () => {
    expect(matchCommand(DEFAULT_KEYS, ev('k', { ctrlKey: true }))).toBe('palette');
    expect(matchCommand(DEFAULT_KEYS, ev('F3'))).toBe('mode.rhythm');
    expect(matchCommand(DEFAULT_KEYS, ev('q', { ctrlKey: true }))).toBeNull();
    expect(matchCommand(DEFAULT_KEYS, ev('Control'))).toBeNull();
  });
});

describe('fuzzy search', () => {
  it('needs every word, in any order, and prefers word starts', () => {
    expect(fuzzyScore('tvf cutoff', 'Tone 2 · TVF · Cutoff Frequency')).toBeGreaterThan(0);
    expect(fuzzyScore('cutoff tvf', 'Tone 2 · TVF · Cutoff Frequency')).toBeGreaterThan(0);
    expect(fuzzyScore('cutoff resonance', 'Cutoff Frequency')).toBe(0);
    expect(fuzzyScore('cut', 'Cutoff')).toBeGreaterThan(fuzzyScore('cut', 'Biscuit'));
    expect(fuzzyScore('', 'anything')).toBe(1);
  });
  it('ranks and limits results', () => {
    const items = ['Undo', 'Redo', 'Randomize', 'Init patch', 'Randomise tone'];
    expect(search('ran', items, (s) => s)).toEqual(['Randomize', 'Randomise tone']);
    expect(search('', items, (s) => s, 2)).toHaveLength(2);
    expect(search('zzz', items, (s) => s)).toEqual([]);
  });
});
