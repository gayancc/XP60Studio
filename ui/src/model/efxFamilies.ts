// How the 40 EFX types are grouped for the Effects panel, and how each type's parameters fall into labelled sections. Grouping is by what the effect does, taken
// from the manual's own descriptions (pp.74-88); the sections are by what each parameter does. Nothing here changes a value or an address.

import { EFX_TYPES, type EfxParam, type EfxType } from './efx';

export interface EfxFamily {
  readonly id: string;
  readonly label: string;
  /** Accent colour of the family (the plug-in's header and knobs). */
  readonly color: string;
  /** The 1-based type numbers in the family. */
  readonly types: readonly number[];
  /** One line on what the family is for. */
  readonly blurb: string;
}

const range = (a: number, b: number): number[] => Array.from({ length: b - a + 1 }, (_, i) => a + i);

export const EFX_FAMILIES: readonly EfxFamily[] = [
  { id: 'eq', label: 'EQ', color: '#59b6e8', types: [1, 5], blurb: 'Shapes the tone: raises or lowers bands of the sound.' },
  { id: 'drive', label: 'Drive', color: '#e8603c', types: [2, 3], blurb: 'Distortion with a guitar-amp simulation.' },
  { id: 'dynamics', label: 'Dynamics', color: '#9bd05a', types: [6, 9, 10], blurb: 'Evens out or brightens the sound.' },
  { id: 'filter', label: 'Filter', color: '#c78be8', types: [4, 7, 8], blurb: 'Sweeping, swirling and rotating tone.' },
  { id: 'chorus', label: 'Chorus', color: '#5fd0b4', types: range(11, 16), blurb: 'Thickens the sound with short modulated delays.' },
  { id: 'delay', label: 'Delay', color: '#e8b44c', types: range(17, 21), blurb: 'Echoes, from a single repeat to four taps.' },
  { id: 'pitch', label: 'Pitch', color: '#e87aa8', types: [22, 23], blurb: 'Copies the sound at another pitch.' },
  { id: 'reverb', label: 'Reverb', color: '#7a9ce8', types: [24, 25], blurb: 'Places the sound in a room or hall.' },
  { id: 'combo', label: 'Combos', color: '#d9a07a', types: range(26, 40), blurb: 'Two effects in one: in series (A to B) or side by side.' },
];

/** The family of a 1-based type number. */
export function familyOf(type: number): EfxFamily {
  return EFX_FAMILIES.find((f) => f.types.includes(type)) ?? (EFX_FAMILIES[0] as EfxFamily);
}

const BLURBS: Record<number, string> = {
  1: 'Four-band stereo equaliser: low shelf, two peaking mids and a high shelf.',
  2: 'Warm overdrive through a guitar-amp simulator.',
  3: 'Harder distortion through a guitar-amp simulator.',
  4: 'Phase shifting that sweeps notches through the sound.',
  5: 'Eight-band graphic equaliser with a width control.',
  6: 'Adds brightness and presence.',
  7: 'A filter that follows the playing, or sweeps on its own.',
  8: 'Rotary speaker, with separate horn and drum speeds.',
  9: 'Evens out loud and soft notes and lengthens sustain.',
  10: 'Holds the peaks of the sound at a ceiling.',
  11: 'Six-voice chorus for a wide, rich sound.',
  12: 'Chorus with a stereo tremolo.',
  13: 'A wide, shimmering chorus.',
  14: 'Stereo chorus with a filter on the effect sound.',
  15: 'Stereo flanger with feedback and a filter.',
  16: 'A flanger that moves in steps.',
  17: 'Independent left and right delays with feedback.',
  18: 'Stereo delay with a modulated delay time.',
  19: 'Three taps: left, right and centre.',
  20: 'Four taps with their own levels.',
  21: 'Delay time that can be changed while playing.',
  22: 'Two pitch-shifted voices.',
  23: 'One pitch-shifted voice that feeds back into itself.',
  24: 'Reverb in six room, stage and hall types.',
  25: 'Gated reverb: normal, reverse and two sweeping types.',
};

/** A sentence on the effect: our own words for a single effect, or the two-part chain for a combination. */
export function efxBlurb(t: EfxType): string {
  const single = BLURBS[t.type];
  if (single) return single;
  const parallel = !t.name.includes('→');
  const parts = t.name.replace(/\s*\((serial|parallel)\)/, '').split(/→|\//);
  const a = (parts[0] ?? '').toLowerCase();
  const b = (parts[1] ?? '').toLowerCase();
  return parallel ? `${a} and ${b} side by side.` : `${a} into ${b}, in series.`;
}

export type SectionId = 'source' | 'time' | 'motion' | 'tone' | 'feedback' | 'output';

export interface EfxSection {
  /** Section title; for a combination, the stage it belongs to. */
  readonly title: string;
  readonly params: readonly EfxParam[];
}

const RULES: ReadonlyArray<readonly [SectionId, RegExp]> = [
  ['output', /\b(Pan|Post Gain|Balance|Mix Level|Output Level)\b|^Level$|Left Level|Right Level|Center Level|^Level \d$/],
  ['feedback', /Feedback|HF Damp|Acceleration|Gate Time/],
  ['time', /Delay Time|Pre Delay|Reverb Type|Gate-Reverb Type|Pitch Shifter Mode|Coarse Pitch|Fine Pitch/],
  ['motion', /Rate|Depth|Phase|Manual|Resonance|Speed|Separation|Deviation|Step|^Peak$/],
  ['tone', /Gain|Frequency|Filter Type|^Q$|Peaking\d Q|Amp Simulator|Band|Cutoff/],
  ['source', /./],
];
const TITLES: Record<SectionId, string> = { source: 'Effect', time: 'Time and pitch', motion: 'Modulation', tone: 'Tone', feedback: 'Feedback', output: 'Output' };
const ORDER: readonly SectionId[] = ['source', 'time', 'motion', 'feedback', 'tone', 'output'];
const AMOUNT = /^(Drive|Sens|Sustain|Attack|Threshold Level|Release Time|Compression Ratio)$/;
const STAGE_PREFIX = /^(Over ?Drive|Distortion|Enhancer|EH|Chorus|Flanger|Delay)\s+/i;

/** The section a single parameter name falls in (the stage prefix of a combination type removed first). */
export function sectionOf(name: string): SectionId {
  const bare = name.replace(STAGE_PREFIX, '');
  if (AMOUNT.test(bare)) return 'source';
  return RULES.find(([, re]) => re.test(bare))?.[0] ?? 'source';
}

const STAGES = /^(Over ?Drive|Distortion|Enhancer|Chorus|Flanger|Delay)\b/i;

/**
 * The sections of a type, in order. A combination type is split by stage ("Chorus", "Delay"), then the shared Output Level closes the list. Every used parameter
 * appears in exactly one section.
 */
export function sectionsOf(type: EfxType): EfxSection[] {
  const used = type.params.filter((p) => p.raw !== null);
  if (type.type >= 26) {
    const stages = new Map<string, EfxParam[]>();
    const tail: EfxParam[] = [];
    for (const p of used) {
      const m = p.name === 'Drive' ? ['Overdrive', 'Overdrive'] : STAGES.exec(p.name);
      if (!m || p.name === 'Output Level') {
        tail.push(p);
        continue;
      }
      const key = /^over ?drive$/i.test(m[1] ?? '') ? 'Overdrive' : (m[1] ?? '');
      stages.set(key, [...(stages.get(key) ?? []), p]);
    }
    const out: EfxSection[] = [...stages].map(([title, params]) => ({ title, params }));
    if (tail.length) out.push({ title: 'Output', params: tail });
    return out;
  }
  const by = new Map<SectionId, EfxParam[]>();
  for (const p of used) {
    const id = sectionOf(p.name);
    by.set(id, [...(by.get(id) ?? []), p]);
  }
  return ORDER.filter((id) => by.has(id)).map((id) => ({ title: TITLES[id], params: by.get(id) ?? [] }));
}

/** Types of a family, as the type records. */
export const typesOfFamily = (f: EfxFamily): EfxType[] => f.types.map((n) => EFX_TYPES[n - 1]).filter((t): t is EfxType => !!t);
