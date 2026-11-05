// Which SR-JV80 Wave Expansion boards are physically installed in the owner's keyboard. The wave picker lists only these
// (besides the instrument's own waves). The XP-60 takes four boards (EXP-A to EXP-D). Kept in the app's own settings.

export const MAX_BOARDS = 4;
const KEY = 'xppatch.installedExpansions';

export interface Storage2 {
  getItem(key: string): string | null;
  setItem(key: string, value: string): void;
}

/** The boards chosen so far, read defensively: a damaged or hand-edited value is ignored rather than trusted. */
export function parseInstalled(text: string | null): number[] {
  if (!text) return [];
  try {
    const v: unknown = JSON.parse(text);
    if (!Array.isArray(v)) return [];
    const nums = v.filter((n): n is number => Number.isInteger(n) && n >= 1 && n <= 127);
    return [...new Set(nums)].slice(0, MAX_BOARDS);
  } catch {
    return [];
  }
}

export class ExpansionSettings {
  installed = $state<number[]>([]);
  /** Why the last change was refused, or ''. */
  notice = $state('');

  constructor(private readonly storage?: Storage2) {
    try {
      this.installed = parseInstalled(storage?.getItem(KEY) ?? null);
    } catch {
      this.installed = [];
    }
  }

  has(board: number): boolean {
    return this.installed.includes(board);
  }

  get full(): boolean {
    return this.installed.length >= MAX_BOARDS;
  }

  /** Mark a board installed or not. A fifth board is refused: the instrument has four slots. */
  set(board: number, on: boolean): void {
    this.notice = '';
    if (on === this.has(board)) return;
    if (on) {
      if (this.full) {
        this.notice = `The XP-60 has ${MAX_BOARDS} expansion slots. Untick a board to install another.`;
        return;
      }
      this.installed = [...this.installed, board].sort((a, b) => a - b);
    } else {
      this.installed = this.installed.filter((n) => n !== board);
    }
    try {
      this.storage?.setItem(KEY, JSON.stringify(this.installed));
    } catch {
      this.notice = 'The choice could not be saved on this computer.';
    }
  }
}

function browserStorage(): Storage2 | undefined {
  try {
    return typeof localStorage === 'undefined' ? undefined : localStorage;
  } catch {
    return undefined;
  }
}

export const expansions = new ExpansionSettings(browserStorage());
