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
  /** Draw the controls as a channel strip (one above the other), for a band, tap or voice. */
  readonly strip?: boolean;
  /** Short names for the controls, in order, where the group's title already says what they belong to (a band's "Freq", "Q", "Gain"). */
  readonly labels?: readonly string[];
  /** The row the group sits in (0 first); a group never shares a row with a group of another row. */
  readonly row?: number;
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
 * How an engineer reads these effects, in signal-flow order, where the plain "by what the parameter does" grouping would scatter related controls (an EQ band's frequency,
 * Q and gain; a delay tap's time and level; a pitch-shifter voice's pitch, pre-delay and pan). Each entry is a group of parameter numbers (1 to 12, the manual's order) and
 * whether to draw it as a strip. A type not listed here uses the generic grouping. Every parameter a type uses must appear exactly once (tested).
 */
type Group = { title: string; idx: readonly number[]; strip?: boolean; names?: readonly string[]; row?: number };
const g = (title: string, idx: readonly number[], row = 0, o: { strip?: boolean; names?: readonly string[] } = {}): Group => ({ title, idx, row, ...o });
const KF = ['Freq', 'Gain'];
const KFQ = ['Freq', 'Q', 'Gain'];
const TL = ['Time', 'Level'];

const LAYOUTS: Record<number, readonly Group[]> = {
  // 4-band EQ read low to high, like the front panel of a console EQ: one strip per band
  1: [g('Low', [1, 2], 0, { strip: true, names: KF }), g('Mid 1', [5, 6, 7], 0, { strip: true, names: KFQ }), g('Mid 2', [8, 9, 10], 0, { strip: true, names: KFQ }), g('High', [3, 4], 0, { strip: true, names: KF }), g('Output', [11], 0, { strip: true })],
  // 8-band graphic EQ: the bands in order, then the width of each, then out
  5: [g('Band gains', [1, 2, 3, 4, 5, 6, 7, 8], 0, { names: ['1', '2', '3', '4', '5', '6', '7', '8'] }), g('Width', [9], 1)],
  // distortion: drive, then the amp it goes through, then tone, then out
  2: [g('Drive', [1]), g('Amp', [3]), g('Tone', [4, 5])],
  3: [g('Drive', [1]), g('Amp', [3]), g('Tone', [4, 5])],
  4: [g('Sweep', [1, 2, 3, 4])],
  6: [g('Enhance', [1]), g('Tone', [3, 4])],
  // auto-wah: the filter, what moves it (the playing, or an LFO), then out
  7: [g('Filter', [1, 5, 6]), g('Movement', [4, 2, 3])],
  // rotary speaker: the horn and the drum each with their speeds, then the speed switch
  8: [g('Horn (high)', [1, 3, 6, 8], 0, { strip: true, names: ['Slow', 'Fast', 'Accel', 'Level'] }), g('Drum (low)', [2, 4, 7, 9], 0, { strip: true, names: ['Slow', 'Fast', 'Accel', 'Level'] }), g('Rotor', [5, 10], 0, { strip: true })],
  9: [g('Compression', [1, 2]), g('Tone', [5, 6]), g('Output gain', [4])],
  10: [g('Limiting', [1, 3, 2]), g('Tone', [6, 7]), g('Output gain', [5])],
  11: [g('Chorus', [1, 2, 3]), g('Deviation', [4, 5, 6]), g('Output', [7, 8], 1)],
  12: [g('Chorus', [1, 2, 3]), g('Tremolo', [4, 5, 6]), g('Output', [7, 8], 1)],
  13: [g('Chorus', [1, 2, 3, 4]), g('Tone', [5, 6]), g('Output', [7, 8], 1)],
  14: [g('Chorus', [3, 4, 5, 6]), g('Filter', [1, 2]), g('Tone', [8, 9], 1), g('Output', [10, 11], 1)],
  15: [g('Flanger', [3, 4, 5, 6, 7]), g('Filter', [1, 2], 1), g('Tone', [8, 9], 1), g('Output', [10, 11], 1)],
  16: [g('Flanger', [1, 2, 3, 4]), g('Step', [5, 6]), g('Tone', [7, 8], 1), g('Output', [9, 10], 1)],
  // stereo delay: each side with its time and feedback phase, then the feedback, what it is filtered by, and out
  17: [g('Left', [2, 4], 0, { names: ['Time', 'Phase'] }), g('Right', [3, 5], 0, { names: ['Time', 'Phase'] }), g('Feedback', [1, 6], 1), g('Tone', [7, 8, 9], 1), g('Output', [10, 11], 2)],
  18: [g('Delay', [2, 3]), g('Feedback', [1, 4, 5]), g('Modulation', [6, 7, 8], 1), g('Tone', [9, 10], 1), g('Output', [11, 12], 1)],
  // triple tap: each tap with its time and level, then the shared feedback
  19: [g('Tap left', [1, 6], 0, { strip: true, names: TL }), g('Tap centre', [3, 8], 0, { strip: true, names: TL }), g('Tap right', [2, 7], 0, { strip: true, names: TL }), g('Feedback', [4, 5], 0, { strip: true }), g('Tone', [9, 10], 0, { strip: true }), g('Output', [11, 12], 0, { strip: true })],
  20: [g('Tap 1', [1, 5], 0, { strip: true, names: TL }), g('Tap 2', [2, 6], 0, { strip: true, names: TL }), g('Tap 3', [3, 7], 0, { strip: true, names: TL }), g('Tap 4', [4, 8], 0, { strip: true, names: TL }), g('Feedback', [9, 10], 0, { strip: true }), g('Output', [11, 12], 0, { strip: true })],
  21: [g('Delay', [1, 3]), g('Feedback', [2, 4]), g('Tone', [6, 7]), g('Output', [5, 8, 9], 1)],
  // two-voice pitch shifter: each voice with its pitch, pre-delay and pan, then the mode and the mix between them
  22: [g('Voice A', [2, 4, 6, 8], 0, { strip: true, names: ['Coarse', 'Fine', 'Pre-delay', 'Pan'] }), g('Voice B', [3, 5, 7, 9], 0, { strip: true, names: ['Coarse', 'Fine', 'Pre-delay', 'Pan'] }), g('Shifter', [1], 0), g('Voice mix', [10], 0)],
  24: [g('Reverb', [1, 2, 3]), g('Damping and tone', [4, 5, 6]), g('Output', [7, 8], 1)],
  25: [g('Gate reverb', [1, 2, 3]), g('Tone', [4, 5]), g('Output', [6, 7], 1)],
  23: [g('Pitch', [1, 2, 3, 4]), g('Feedback', [5]), g('Tone', [7, 8], 1)],
};

// ---- combinations: each stage is a module in signal order, packed into rows without stranding a module on its own line.
const KNOB_W = 62;
const GAP_W = 14;
const ROW_W = 468;
/** The label a control gets inside its stage, from the manual's full name ("Chorus Pre Delay Time" in the Chorus module is just "Pre-delay"). */
const STAGE_LABELS: ReadonlyArray<readonly [RegExp, string]> = [
  [/Pre Delay Time$/, 'Pre-delay'],
  [/Feedback Level$/, 'Feedback'],
  [/Delay Time$/, 'Time'],
  [/HF Damp$/, 'HF damp'],
  [/Mix Level$/, 'Mix'],
  [/Pan$/, 'Pan'],
  [/Balance$/, 'Balance'],
  [/Output Level$/, 'Level'],
  [/(^|\s)Rate$/, 'Rate'],
  [/Depth$/, 'Depth'],
  [/Sens$/, 'Sens'],
  [/Drive$/, 'Drive'],
];
const labelOf = (name: string): string => STAGE_LABELS.find(([re]) => re.test(name))?.[1] ?? name;

function combinationSections(used: readonly EfxParam[]): EfxSection[] {
  const stages = new Map<string, EfxParam[]>();
  for (const p of used) {
    const m = p.name === 'Drive' ? ['Overdrive', 'Overdrive'] : STAGES.exec(p.name);
    const title = /^over ?drive$/i.test(m?.[1] ?? '') ? 'Overdrive' : (m?.[1] ?? 'Effect');
    stages.set(title, [...(stages.get(title) ?? []), p]);
  }
  const mods = [...stages.entries()].filter(([, ps]) => ps.length > 0);
  let row = 0;
  let width = 0;
  return mods.map(([title, params]) => {
    // a drop-down (HF Damp) is wider than a knob
    const w = params.length * KNOB_W + (params.some((p) => /HF Damp$/.test(p.name)) ? 36 : 0);
    if (width > 0 && width + w + GAP_W > ROW_W) {
      row += 1;
      width = 0;
    }
    width += w + GAP_W;
    return { title, params, row, labels: params.map((p) => labelOf(p.name)) };
  });
}

/**
 * The sections of a type, in order. A combination type is split by stage ("Chorus", "Delay"), then the shared Output Level closes the list. Every used parameter
 * appears in exactly one section.
 */
/**
 * The master controls of an effect: the output pan, the dry/wet Effect Balance or Mix Level (single effects) and the Output Level. Every effect has the Level, so it is drawn in one fixed place under the
 * sketch, like a plug-in's master section, instead of as a group of its own that would sit alone at the end of a row.
 */
export const isMaster = (type: EfxType, p: EfxParam): boolean => p.raw !== null && (p.name === 'Output Level' || p.name === 'Level' || (type.type < 26 && (p.name === 'Effect Balance' || p.name === 'Mix Level' || p.name === 'Output Pan')));
/** Pan first, then the dry/wet control, then the level: the order a channel's output section reads. */
const masterRank = (p: EfxParam): number => (p.name === 'Output Pan' ? 0 : /Level$/.test(p.name) && !/Mix/.test(p.name) ? 2 : 1);
export const masterOf = (type: EfxType): EfxParam[] => type.params.filter((p) => isMaster(type, p)).sort((a, b) => masterRank(a) - masterRank(b));

export function sectionsOf(type: EfxType): EfxSection[] {
  const used = type.params.filter((p) => p.raw !== null && !isMaster(type, p));
  const layout = LAYOUTS[type.type];
  if (layout) {
    return layout
      .map((g) => {
        // a group keeps its short names aligned with the controls that remain once the master controls are taken out
        const kept = g.idx.map((i, k) => [used.find((p) => p.index === i), g.names?.[k]] as const).filter((e): e is readonly [EfxParam, string | undefined] => !!e[0]);
        return { title: g.title, strip: g.strip, row: g.row, params: kept.map((e) => e[0]), labels: g.names ? kept.map((e) => e[1] ?? e[0].name) : undefined };
      })
      .filter((sec) => sec.params.length > 0);
  }
  if (type.type >= 26) return combinationSections(used);
  const by = new Map<SectionId, EfxParam[]>();
  for (const p of used) {
    const id = sectionOf(p.name);
    by.set(id, [...(by.get(id) ?? []), p]);
  }
  return ORDER.filter((id) => by.has(id)).map((id) => ({ title: TITLES[id], params: by.get(id) ?? [] }));
}

/** Types of a family, as the type records. */
export const typesOfFamily = (f: EfxFamily): EfxType[] => f.types.map((n) => EFX_TYPES[n - 1]).filter((t): t is EfxType => !!t);
