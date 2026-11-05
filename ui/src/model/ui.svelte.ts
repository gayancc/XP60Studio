// Small bits of UI state that must survive switching between the workspaces (each workspace is re-created on a switch).

import type { Clip } from './tools';

export interface BankState {
  file: string;
  bytes: Uint8Array;
  names: Array<[number, string]>;
}

export interface UiState {
  bank: BankState | null;
  bankSlot: number;
  tone: number;
  part: number;
  key: number;
  tune: number;
  /** Settings of the sound-design tools. */
  tools: { strength: number; amount: number; allBlocks: boolean; seed: number; morph: number };
  /** The last thing copied with Copy tone / Copy section. */
  clip: Clip | null;
  paletteOpen: boolean;
  /** A short message about something that just finished ("Copied ..."). */
  notice: string;
}

export const ui: UiState = $state({
  bank: null,
  bankSlot: 0,
  tone: 0,
  part: 0,
  key: 9, // key 44, G#2
  tune: 16, // Patch mode
  tools: { strength: 0.3, amount: 0.1, allBlocks: false, seed: 1, morph: 0 },
  clip: null,
  paletteOpen: false,
  notice: '',
});

let noticeTimer: ReturnType<typeof setTimeout> | undefined;

/** Show `text` in the top bar for a few seconds. */
export function say(text: string, ms = 5000): void {
  clearTimeout(noticeTimer);
  ui.notice = text;
  noticeTimer = setTimeout(() => (ui.notice = ''), ms);
}
