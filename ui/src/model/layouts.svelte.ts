// The arrangement of the window (parity row F1): which workspace is open and which tab each workspace shows. It is kept
// between runs, and the user can save it under a name and bring it back. This is the "saved layouts" half of docking: the
// panels themselves are tabbed rather than free-floating (artifact/docs/xppatchstudio/06 section 8.1).

import { reportError, reportInfo, reportSuccess, reportWarning } from './messages.svelte';

export type WorkspaceMode = 'patch' | 'performance' | 'rhythm' | 'system' | 'library' | 'bank';

export interface Arrangement {
  mode: WorkspaceMode;
  patchTab: number;
  patchSide: number;
  /** 0: the sound sections show the selected Tone; 1: they show all four Tones side by side. */
  toneView: number;
  perfPage: number;
  perfDetail: number;
  rhythmTab: number;
  systemTab: number;
  bankPage: number;
  bankTab: number;
}

export interface NamedLayout {
  name: string;
  arrangement: Arrangement;
}

export const DEFAULT_ARRANGEMENT: Readonly<Arrangement> = {
  mode: 'patch',
  patchTab: 1,
  patchSide: 0,
  toneView: 0,
  perfPage: 0,
  perfDetail: 0,
  rhythmTab: 0,
  systemTab: 0,
  bankPage: 0,
  bankTab: 0,
};

/** Layouts that ship with the app: they can be applied but not changed or deleted. */
export const BUILT_IN: readonly NamedLayout[] = [
  { name: 'Sound design', arrangement: { ...DEFAULT_ARRANGEMENT, patchTab: 1, patchSide: 0 } },
  { name: 'Key zones', arrangement: { ...DEFAULT_ARRANGEMENT, patchTab: 0, patchSide: 1 } },
  { name: 'Performance rack', arrangement: { ...DEFAULT_ARRANGEMENT, mode: 'performance' } },
  { name: 'Librarian', arrangement: { ...DEFAULT_ARRANGEMENT, mode: 'library' } },
];

const MODES = new Set<string>(['patch', 'performance', 'rhythm', 'system', 'library', 'bank']);
const TAB_KEYS = ['patchTab', 'patchSide', 'toneView', 'perfPage', 'perfDetail', 'rhythmTab', 'systemTab', 'bankPage', 'bankTab'] as const;
/** The largest tab index kept, so damaged storage cannot hold absurd numbers. */
const MAX_TAB = 63;
export const MAX_NAMED = 12;
export const MAX_NAME = 24;

const ARRANGEMENT_KEY = 'xppatch.arrangement.v1';
const LAYOUTS_KEY = 'xppatch.layouts.v1';

/** Read an arrangement from untrusted data: every field is checked, anything wrong falls back to the default. */
export function parseArrangement(raw: unknown): Arrangement {
  const out: Arrangement = { ...DEFAULT_ARRANGEMENT };
  if (typeof raw !== 'object' || raw === null) return out;
  const r = raw as Record<string, unknown>;
  if (typeof r.mode === 'string' && MODES.has(r.mode)) out.mode = r.mode as WorkspaceMode;
  for (const k of TAB_KEYS) {
    const v = r[k];
    if (typeof v === 'number' && Number.isInteger(v) && v >= 0 && v <= MAX_TAB) out[k] = v;
  }
  return out;
}

/** Named layouts from stored text: well-formed entries only, unique names, at most `MAX_NAMED`. Built-in names are reserved. */
export function parseLayouts(text: string | null): NamedLayout[] {
  if (!text) return [];
  let raw: unknown;
  try {
    raw = JSON.parse(text);
  } catch {
    return [];
  }
  if (!Array.isArray(raw)) return [];
  const taken = new Set(BUILT_IN.map((b) => b.name.toLowerCase()));
  const out: NamedLayout[] = [];
  for (const item of raw) {
    if (typeof item !== 'object' || item === null) continue;
    const r = item as Record<string, unknown>;
    const name = typeof r.name === 'string' ? cleanName(r.name) : '';
    if (name === '' || taken.has(name.toLowerCase()) || out.length >= MAX_NAMED) continue;
    taken.add(name.toLowerCase());
    out.push({ name, arrangement: parseArrangement(r.arrangement) });
  }
  return out;
}

export const cleanName = (name: string): string => name.replace(/\s+/g, ' ').trim().slice(0, MAX_NAME);

/** A tab index that is certain to exist: a saved layout may come from a version with a different number of tabs. */
export const tabIn = (value: number, count: number): number => Math.min(Math.max(0, value), Math.max(0, count - 1));

function read(key: string): string | null {
  try {
    return globalThis.localStorage?.getItem(key) ?? null;
  } catch {
    return null;
  }
}

function write(key: string, text: string): void {
  try {
    globalThis.localStorage?.setItem(key, text);
  } catch {
    // The layout still works for this run; it is only not remembered.
  }
}

function load(): Arrangement {
  const text = read(ARRANGEMENT_KEY);
  if (!text) return { ...DEFAULT_ARRANGEMENT };
  try {
    return parseArrangement(JSON.parse(text));
  } catch {
    return { ...DEFAULT_ARRANGEMENT };
  }
}

/** The arrangement on screen. Views read and write its fields directly. */
export const arrangement: Arrangement = $state(load());

class Layouts {
  custom = $state<NamedLayout[]>(parseLayouts(read(LAYOUTS_KEY)));
  #message = $state('');
  get message(): string {
    return this.#message;
  }
  /** Setting text here also shows it in the bottom-left corner with every other message. */
  set message(text: string) {
    this.#message = text;
    if (text) reportInfo(text);
  }

  get all(): NamedLayout[] {
    return [...BUILT_IN, ...this.custom];
  }

  isBuiltIn(name: string): boolean {
    return BUILT_IN.some((b) => b.name.toLowerCase() === name.toLowerCase());
  }

  /** Remember what is on screen now (called when it changes). */
  persistCurrent(): void {
    write(ARRANGEMENT_KEY, JSON.stringify($state.snapshot(arrangement)));
  }

  apply(name: string): boolean {
    const found = this.all.find((l) => l.name.toLowerCase() === name.toLowerCase());
    if (!found) return false;
    Object.assign(arrangement, found.arrangement);
    this.message = `Layout "${found.name}" applied.`;
    return true;
  }

  reset(): void {
    Object.assign(arrangement, DEFAULT_ARRANGEMENT);
    this.message = 'Layout reset.';
  }

  /** Save what is on screen under `name`. A built-in name is refused; an existing custom name is replaced. */
  saveAs(rawName: string): boolean {
    const name = cleanName(rawName);
    if (name === '') {
      this.message = 'Give the layout a name.';
      return false;
    }
    if (this.isBuiltIn(name)) {
      this.message = `"${name}" is a built-in layout; choose another name.`;
      return false;
    }
    const entry: NamedLayout = { name, arrangement: parseArrangement($state.snapshot(arrangement)) };
    const at = this.custom.findIndex((l) => l.name.toLowerCase() === name.toLowerCase());
    if (at >= 0) this.custom = this.custom.map((l, i) => (i === at ? entry : l));
    else if (this.custom.length >= MAX_NAMED) {
      this.message = `At most ${MAX_NAMED} layouts can be saved; delete one first.`;
      return false;
    } else this.custom = [...this.custom, entry];
    write(LAYOUTS_KEY, JSON.stringify(this.custom));
    this.message = `Layout "${name}" saved.`;
    return true;
  }

  remove(name: string): boolean {
    if (this.isBuiltIn(name)) return false;
    const before = this.custom.length;
    this.custom = this.custom.filter((l) => l.name.toLowerCase() !== name.toLowerCase());
    write(LAYOUTS_KEY, JSON.stringify(this.custom));
    return this.custom.length < before;
  }
}

export const layouts = new Layouts();
