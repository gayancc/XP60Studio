// Small bits of UI state that must survive switching between the workspaces (each workspace is re-created on a switch).

import { messages } from './messages.svelte';
import type { Clip } from './tools';

export interface BankState {
  /** What the file's slots hold; patches unless it was opened in the Performance or Rhythm Set workspace. */
  kind?: 'patch' | 'performance' | 'rhythm';
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
  /** The group to point out after a jump from another tab (for example from the routing diagram): its name and the moment of the jump. */
  flash: { section: string; at: number } | null;
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
  flash: null,
});

/** Tell the person something short that just finished ("Copied ..."). It appears in the bottom-left corner with every other message. */
export function say(text: string, ms = 5000): void {
  messages.report('info', text, ms);
}
