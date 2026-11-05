// Names shown on controls and in the hint bar.

import type { BlockName, DocKindName } from './catalog';
import { RHYTHM_FIRST_NOTE } from './catalog';
import {
  commonSectionOf,
  partSectionOf,
  noteSectionOf,
  systemSectionOf,
  performanceCommonSectionOf,
  toneSectionOf,
  type CommonSection,
  type NoteSection,
  type PartSection,
  type SystemSection,
  type PerformanceCommonSection,
  type ToneSection,
} from './layout';

const TONE_SECTION_TITLES: Record<ToneSection, string> = {
  mixer: 'Mixer',
  wave: 'Wave',
  zone: 'Zones',
  controllers: 'Controllers',
  lfo1: 'LFO 1',
  lfo2: 'LFO 2',
  pitch: 'Pitch',
  tvf: 'TVF',
  tva: 'TVA',
  output: 'Output',
};

const COMMON_SECTION_TITLES: Record<CommonSection, string> = {
  name: 'Name',
  patch: 'Patch',
  play: 'Play',
  portamento: 'Portamento',
  efx: 'EFX',
  chorus: 'Chorus',
  reverb: 'Reverb',
  controllers: 'Controllers',
  structure: 'Structure',
};

const PERF_COMMON_TITLES: Record<PerformanceCommonSection, string> = {
  name: 'Name',
  general: 'General',
  efx: 'EFX',
  chorus: 'Chorus',
  reverb: 'Reverb',
  voices: 'Voice reserve',
};

const PART_TITLES: Record<PartSection, string> = {
  source: 'Sound',
  mix: 'Mix',
  tune: 'Tune',
  range: 'Key range',
  midi: 'MIDI',
};

const NOTE_TITLES: Record<NoteSection, string> = {
  wave: 'Wave',
  pitch: 'Pitch',
  tvf: 'TVF',
  tva: 'TVA',
  output: 'Output',
};

const SYSTEM_TITLES: Record<SystemSection, string> = {
  sound: 'Sound and mode',
  controllers: 'Control sources',
  receive: 'Receive',
  keyboard: 'Keyboard and transmit',
  pedals: 'Pedals and controls',
  banks: 'Bank select',
  preview: 'Preview',
  arpeggio: 'Arpeggio',
};

export function systemSectionTitle(s: SystemSection): string {
  return SYSTEM_TITLES[s];
}

export function toneSectionTitle(s: ToneSection): string {
  return TONE_SECTION_TITLES[s];
}

export function commonSectionTitle(s: CommonSection): string {
  return COMMON_SECTION_TITLES[s];
}

export function performanceCommonTitle(s: PerformanceCommonSection): string {
  return PERF_COMMON_TITLES[s];
}

export function partSectionTitle(s: PartSection): string {
  return PART_TITLES[s];
}

export function noteSectionTitle(s: NoteSection): string {
  return NOTE_TITLES[s];
}

const NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];

/** Name of a MIDI note (60 is C4). */
export function midiNoteName(n: number): string {
  return `${NOTE_NAMES[((n % 12) + 12) % 12]}${Math.floor(n / 12) - 1}`;
}

/** The key number a rhythm block label stands for, or null. */
export function rhythmKey(block: BlockName): number | null {
  const k = Number(block.startsWith('note') ? block.slice(4) : NaN);
  return Number.isInteger(k) && k >= RHYTHM_FIRST_NOTE && k < RHYTHM_FIRST_NOTE + 64 ? k : null;
}

/** Title of a block: "Tone 2", "Part 5", "Key C#2 (37)". */
export function blockTitle(block: BlockName, kind: DocKindName = 'patch'): string {
  if (block === 'common') {
    return kind === 'patch' ? 'Patch' : kind === 'performance' ? 'Performance' : kind === 'rhythm' ? 'Rhythm set' : 'System';
  }
  if (kind === 'system') return Number(block.slice(4)) === 17 ? 'Scale tune, Patch mode' : `Scale tune, Part ${block.slice(4)}`;
  if (kind === 'patch') return `Tone ${block.slice(4)}`;
  if (kind === 'performance') return `Part ${block.slice(4)}`;
  const k = rhythmKey(block);
  return k === null ? block : `Key ${midiNoteName(k)} (${k})`;
}

/** Breadcrumb for the hint bar, for example "Tone 2 > TVF". */
export function contextOf(block: BlockName, id: string, kind: DocKindName = 'patch'): string {
  const title = blockTitle(block, kind);
  let section: string | undefined;
  if (kind === 'patch') {
    section =
      block === 'common'
        ? COMMON_SECTION_TITLES[commonSectionOf(id) ?? 'patch']
        : TONE_SECTION_TITLES[toneSectionOf(id) ?? 'mixer'];
  } else if (kind === 'performance') {
    section =
      block === 'common'
        ? PERF_COMMON_TITLES[performanceCommonSectionOf(id) ?? 'general']
        : PART_TITLES[partSectionOf(id) ?? 'source'];
  } else if (kind === 'system') {
    section = block === 'common' ? SYSTEM_TITLES[systemSectionOf(id) ?? 'sound'] : undefined;
  } else if (block !== 'common') {
    section = NOTE_TITLES[noteSectionOf(id) ?? 'wave'];
  }
  return section ? `${title} > ${section}` : title;
}

const REPLACEMENTS: ReadonlyArray<readonly [RegExp, string]> = [
  [/^(Pitch|Filter|Level) Envelope /, ''],
  [/^LFO[12] /, ''],
  [/^(Pitch|Filter|Level|Pan) LFO([12]) Depth$/, 'LFO$2 Depth'],
  [/^(EFX|Chorus|Reverb|Portamento) (?!Send)/, ""],
  [/^Controller (\d) (Destination|Depth) (\d)$/, '$2 $3'],
  [/Velocity/g, 'Vel'],
  [/Keyfollow/g, 'Key Follow'],
];

/** A short label that fits under a knob. The full name is still shown in the hint bar. */
export function shortLabel(name: string): string {
  let out = name;
  for (const [re, to] of REPLACEMENTS) out = out.replace(re, to);
  return out.trim() === '' ? name : out.trim();
}
