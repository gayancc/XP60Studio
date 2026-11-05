// Keyboard shortcuts: a default map, a user override map (editable, saved), conflict detection and fuzzy search for the
// command palette. Pure functions; the browser's storage is touched only through the two small helpers at the end.

export interface KeyEventLike {
  key: string;
  ctrlKey: boolean;
  metaKey: boolean;
  shiftKey: boolean;
  altKey: boolean;
}

/** Command id to key. An empty string means "no shortcut". */
export type KeyMap = Record<string, string>;

export const DEFAULT_KEYS: Readonly<KeyMap> = {
  palette: 'Ctrl+K',
  undo: 'Ctrl+Z',
  redo: 'Ctrl+Y',
  init: 'Ctrl+D',
  randomize: 'Ctrl+R',
  mutate: 'Ctrl+M',
  ab: 'Ctrl+B',
  copy: 'Ctrl+Shift+C',
  paste: 'Ctrl+Shift+V',
  'mode.patch': 'F1',
  'mode.performance': 'F2',
  'mode.rhythm': 'F3',
  'mode.system': 'F4',
  'mode.library': 'F5',
  'mode.bank': 'F6',
};

const MODIFIER_KEYS = new Set(['Control', 'Shift', 'Alt', 'Meta', 'AltGraph', 'CapsLock']);

/** A key event as a stable text such as `Ctrl+Shift+Z` or `F2`, or null for a bare modifier press. */
export function keyOf(e: KeyEventLike): string | null {
  if (MODIFIER_KEYS.has(e.key)) return null;
  const parts: string[] = [];
  if (e.ctrlKey || e.metaKey) parts.push('Ctrl');
  if (e.altKey) parts.push('Alt');
  if (e.shiftKey) parts.push('Shift');
  const k = e.key.length === 1 ? e.key.toUpperCase() : e.key;
  parts.push(k === ' ' ? 'Space' : k);
  return parts.join('+');
}

/** A shortcut must include Ctrl or Alt, or be a function key, so typing in a text box never triggers a command. */
export function isAcceptableKey(key: string): boolean {
  if (key === '') return true;
  return /^(Ctrl|Alt)\+/.test(key) || /^F([1-9]|1[0-2])$/.test(key) || /^Shift\+F([1-9]|1[0-2])$/.test(key);
}

/** Keep only well-formed overrides for known commands; anything else (a damaged file) is ignored. */
export function sanitizeOverrides(raw: unknown, knownIds: ReadonlySet<string>): KeyMap {
  const out: KeyMap = {};
  if (typeof raw !== 'object' || raw === null || Array.isArray(raw)) return out;
  for (const [id, key] of Object.entries(raw)) {
    if (knownIds.has(id) && typeof key === 'string' && key.length <= 30 && isAcceptableKey(key)) out[id] = key;
  }
  return out;
}

export function effectiveKeys(overrides: KeyMap): KeyMap {
  return { ...DEFAULT_KEYS, ...overrides };
}

/** Keys that more than one command wants, as `[key, command ids]`. */
export function conflicts(map: KeyMap): Array<[string, string[]]> {
  const byKey = new Map<string, string[]>();
  for (const [id, key] of Object.entries(map)) {
    if (!key) continue;
    byKey.set(key, [...(byKey.get(key) ?? []), id]);
  }
  return [...byKey.entries()].filter(([, ids]) => ids.length > 1);
}

/** The command a key event triggers, or null. When two commands share a key neither runs (the conflict is shown). */
export function matchCommand(map: KeyMap, e: KeyEventLike): string | null {
  const k = keyOf(e);
  if (!k) return null;
  const ids = Object.entries(map)
    .filter(([, key]) => key === k)
    .map(([id]) => id);
  return ids.length === 1 ? (ids[0] ?? null) : null;
}

/**
 * A score for how well `query` matches `text` (higher is better, 0 is no match). Every query word must appear, in any
 * order; earlier and word-start matches score more.
 */
export function fuzzyScore(query: string, text: string): number {
  const words = query.toLowerCase().split(/\s+/).filter(Boolean);
  if (words.length === 0) return 1;
  const hay = text.toLowerCase();
  let score = 0;
  for (const w of words) {
    const at = hay.indexOf(w);
    if (at < 0) return 0;
    const wordStart = at === 0 || /[\s·>:_-]/.test(hay[at - 1] ?? ' ');
    score += 10 + (wordStart ? 5 : 0) - Math.min(at, 9) * 0.1;
  }
  return score;
}

export function search<T>(query: string, items: readonly T[], text: (item: T) => string, limit = 30): T[] {
  return items
    .map((item) => ({ item, score: fuzzyScore(query, text(item)) }))
    .filter((x) => x.score > 0)
    .sort((a, b) => b.score - a.score)
    .slice(0, limit)
    .map((x) => x.item);
}

const STORAGE_KEY = 'xppatch.shortcuts';

export function loadOverrides(knownIds: ReadonlySet<string>): KeyMap {
  try {
    const raw = globalThis.localStorage?.getItem(STORAGE_KEY);
    return raw ? sanitizeOverrides(JSON.parse(raw), knownIds) : {};
  } catch {
    return {};
  }
}

export function saveOverrides(overrides: KeyMap): void {
  try {
    globalThis.localStorage?.setItem(STORAGE_KEY, JSON.stringify(overrides));
  } catch {
    // Storage may be unavailable (private window); shortcuts then last for the session only.
  }
}
