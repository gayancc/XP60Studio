// A one-glance summary of the open patch (parity row B12): which Tones sound, with what wave, how loud, over which keys and
// velocities, and what the effects section is doing. Everything comes from the same values the editor shows.

import { waveLabel } from './factory';
import { TONE_BLOCKS } from './catalog';
import { noteName } from '../kit/keyRange';
import type { PatchStore } from './store.svelte';

export interface ToneSummary {
  /** 1 to 4. */
  number: number;
  on: boolean;
  /** "INT 001": the wave group and number. */
  wave: string;
  level: string;
  /** "C2 to C6", or "all keys" when the range covers the keyboard. */
  keys: string;
  /** "1 to 127", or "all velocities". */
  velocity: string;
}

export interface PatchSummary {
  name: string;
  level: string;
  structure: string;
  effects: Array<{ label: string; value: string }>;
  tones: ToneSummary[];
}

const range = (low: number, high: number, all: [number, number], text: (n: number) => string, everything: string): string =>
  low <= all[0] && high >= all[1] ? everything : low === high ? text(low) : `${text(low)} to ${text(high)}`;

/** "INT 026", and Roland's name for the wave after it when the lists we hold have it. */
function waveText(store: PatchStore, b: string): string {
  const base = `${store.format(b, 'wave_group_type')} ${store.format(b, 'wave_number')}`;
  return `${base} · ${waveLabel(store.get(b, 'wave_group_type'), store.get(b, 'wave_group_id'), store.get(b, 'wave_number') + 1)}`;
}

export function summarisePatch(store: PatchStore): PatchSummary {
  const f = (block: string, id: string): string => store.format(block, id);
  const tones = TONE_BLOCKS.map((b, i): ToneSummary => ({
    number: i + 1,
    on: store.get(b, 'tone_switch') === 1,
    wave: waveText(store, b),
    level: f(b, 'tone_level'),
    keys: range(store.get(b, 'keyboard_range_lower'), store.get(b, 'keyboard_range_upper'), [0, 127], noteName, 'all keys'),
    velocity: range(store.get(b, 'velocity_range_lower'), store.get(b, 'velocity_range_upper'), [1, 127], String, 'all velocities'),
  }));
  return {
    name: store.name.trimEnd() || '(unnamed)',
    level: f('common', 'patch_level'),
    structure: `1&2 ${f('common', 'structure_type_1_2')}, 3&4 ${f('common', 'structure_type_3_4')}`,
    effects: [
      { label: 'EFX', value: f('common', 'efx_type') },
      { label: 'Chorus level', value: f('common', 'chorus_level') },
      { label: 'Reverb', value: `${f('common', 'reverb_type')}, level ${f('common', 'reverb_level')}` },
    ],
    tones,
  };
}
