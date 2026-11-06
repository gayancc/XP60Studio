// What each of the 40 EFX (multi-effect) types calls its twelve parameters. Generated into data/efx-parameters.json by scripts/build_efx_data.py from Roland's
// Owner's Manual appendix "Correspondence of the EFX Algorithm and Exclusive Address (EFX Parameter 1-12)": parameter 1 to 12 of each type, with its name and its raw
// range. 339 of the 351 parameter names were checked against the XP-60/80 manual's own parameter list (the others differ only in spelling, such as "Over Drive Pan").

import raw from '../../../data/efx-parameters.json';
import rawDefaults from '../../../data/efx-defaults.json';

export interface EfxParam {
  /** Parameter number, 1 to 12: the stored value is `efx_parameter_<index>`. */
  readonly index: number;
  readonly name: string;
  /** Raw range, or null for a parameter the type does not use. */
  readonly raw: readonly [number, number] | null;
}

export interface EfxType {
  readonly type: number;
  readonly name: string;
  readonly params: readonly EfxParam[];
}

const data = raw as { types: EfxType[] };

export const EFX_TYPES: readonly EfxType[] = data.types;

/** The type for a stored EFX Type value (0 to 39). */
export function efxTypeOf(stored: number): EfxType | undefined {
  return EFX_TYPES[stored];
}

/** The parameter a type gives to parameter slot `index` (1 to 12), if it uses that slot. */
export function efxParam(stored: number, index: number): EfxParam | undefined {
  const p = efxTypeOf(stored)?.params.find((x) => x.index === index);
  return p && p.raw ? p : undefined;
}

const defaultsData = rawDefaults as { types: Array<{ type: number; defaults: number[] }> };

/**
 * The starting value of the twelve parameter slots for a stored EFX type (0 to 39): a pan centred, a gain at 0 dB, a level full, a balance half, and so on. They are
 * chosen from the names and ranges the manual gives (the manual lists no defaults) and checked against the factory patches; see scripts/build_efx_defaults.py.
 * A slot the type does not use is 0.
 */
export function efxDefaults(stored: number): readonly number[] {
  return defaultsData.types.find((t) => t.type === stored + 1)?.defaults ?? Array.from({ length: 12 }, () => 0);
}

/** The starting value of parameter slot `index` (1 to 12) of a type. */
export const efxDefault = (stored: number, index: number): number => efxDefaults(stored)[index - 1] ?? 0;

/** "19 TRIPLE-TAP-DELAY", for the type list. */
export const efxTypeLabel = (t: EfxType): string => `${String(t.type).padStart(2, '0')} ${t.name}`;

// Words shortened so a name fits under a knob. Only whole words are replaced, and only with the spelling the manual itself uses in its short column.
const SHORT: ReadonlyArray<readonly [RegExp, string]> = [
  [/\bFrequency\b/g, 'Freq'],
  [/\bOutput Level\b/g, 'Level'],
  [/\bOutput Pan\b/g, 'Pan'],
  [/\bMix Level\b/g, 'Mix'],
  [/\bEffect Balance\b/g, 'Balance'],
  [/\bFeedback Level\b/g, 'Feedback'],
  [/\bPre Delay Time\b/g, 'Pre Delay'],
  [/\bDelay Time\b/g, 'Delay'],
  [/\bLeft Level\b/g, 'Level L'],
  [/\bRight Level\b/g, 'Level R'],
  [/\bCenter Level\b/g, 'Level C'],
  [/\bLeft\b/g, 'L'],
  [/\bRight\b/g, 'R'],
  [/\bCenter\b/g, 'C'],
  [/\bAmp Simulator Type\b/g, 'Amp Sim'],
];

/** A name short enough for a knob label; the full name stays in the hint. */
export function efxShortName(name: string): string {
  return SHORT.reduce((s, [re, to]) => s.replace(re, to), name.replace(/\s+/g, ' ').trim());
}

const HF_DAMP = ['200', '250', '315', '400', '500', '630', '800', '1000', '1250', '1600', '2000', '2500', '3150', '4000', '5000', '6300', '8000', 'BYPASS'];
/** The 17 frequencies the manual lists for Peaking and Cutoff Frequency (200 Hz to 8 kHz); HF Damp adds BYPASS. */
const FREQ_LIST = HF_DAMP.slice(0, 17);

/**
 * Parameters whose values are a short list of words or numbers, taken from the manual's value column (pp.199-203) and its parameter descriptions (pp.74-87).
 * Keyed by the parameter name and its raw range, so a name that means something else in another type is not caught.
 */
const CHOICES: ReadonlyArray<{ name: RegExp; range: readonly [number, number]; items: readonly string[] }> = [
  { name: /^Low Frequency$/, range: [0, 1], items: ['200 Hz', '400 Hz'] },
  { name: /^High Frequency$/, range: [0, 1], items: ['4000 Hz', '8000 Hz'] },
  { name: /^Amp Simulator Type$/, range: [0, 3], items: ['SMALL', 'BUILT-IN', '2-STACK', '3-STACK'] },
  { name: /^Speed$/, range: [0, 1], items: ['SLOW', 'FAST'] },
  { name: /^Filter Type$/, range: [0, 1], items: ['LPF', 'BPF'] },
  { name: /^Filter Type$/, range: [0, 2], items: ['OFF', 'LPF', 'HPF'] },
  { name: /^Post Gain$/, range: [0, 3], items: ['0 dB', '+6 dB', '+12 dB', '+18 dB'] },
  { name: /^Compression Ratio$/, range: [0, 3], items: ['1.5:1', '2:1', '4:1', '100:1'] },
  { name: /^Feedback Mode$/, range: [0, 1], items: ['NORMAL', 'CROSS'] },
  { name: /^Feedback Phase (Left|Right)$/, range: [0, 1], items: ['NORMAL', 'INVERT'] },
  { name: /^Reverb Type$/, range: [0, 5], items: ['ROOM1', 'ROOM2', 'STAGE1', 'STAGE2', 'HALL1', 'HALL2'] },
  { name: /^Gate-Reverb Type$/, range: [0, 3], items: ['NORMAL', 'REVERSE', 'SWEEP1', 'SWEEP2'] },
  { name: /^Pitch Shifter Mode$/, range: [0, 4], items: ['1', '2', '3', '4', '5'] },
];

/** The word list for a parameter, or undefined when it is a plain amount (a knob). */
export function efxChoices(name: string, range: readonly [number, number]): readonly string[] | undefined {
  return CHOICES.find((c) => c.name.test(name) && c.range[0] === range[0] && c.range[1] === range[1])?.items;
}

/**
 * The meaning of a raw value, where the manual settles it: gains (-15 to +15 dB), effect balance (D100:0W to D0:100W), the frequency lists, output pan (L64 to 63R),
 * coarse pitch (-24 to +12 semitones), fine pitch (2-cent steps, -100 to +100), feedback level (2 % steps, -98 to +98 %), chorus and tremolo phase (2-degree steps,
 * 0 to 180) and the spectrum band width (1 to 5). Rates and times are not linear in the manual's tables, which are not transcribed here, so they show their raw number.
 */
export function efxValueText(name: string, range: readonly [number, number], value: number): string {
  const [lo, hi] = range;
  const list = efxChoices(name, range);
  if (list) return list[value] ?? String(value);
  if (/\bGain$/.test(name) && lo === 0 && hi === 30) {
    const db = value - 15;
    return `${db > 0 ? '+' : ''}${db} dB`;
  }
  if (/\bBalance$/.test(name) && lo === 0 && hi === 100) return `D${100 - value}:${value}W`;
  if (/HF Damp$/.test(name) && lo === 0 && hi === 17) return HF_DAMP[value] ?? String(value);
  if (/(Peaking\d Frequency|Cutoff Frequency)$/.test(name) && lo === 0 && hi === 16) return `${FREQ_LIST[value] ?? value} Hz`;
  if (/\bPan( [AB])?$/.test(name) && lo === 0 && hi === 127) return value === 64 ? '0' : value < 64 ? `L${64 - value}` : `${value - 64}R`;
  if (/^Coarse Pitch/.test(name) && lo === 0 && hi === 36) {
    const st = value - 24;
    return `${st > 0 ? '+' : ''}${st} st`;
  }
  if (/^Fine Pitch/.test(name) && lo === 0 && hi === 100) {
    const c = (value - 50) * 2;
    return `${c > 0 ? '+' : ''}${c} ct`;
  }
  if (/Feedback Level$/.test(name) && lo === 0 && hi === 98) {
    const pct = (value - 49) * 2;
    return `${pct > 0 ? '+' : ''}${pct}%`;
  }
  if (/\bPhase$/.test(name) && lo === 0 && hi === 90) return `${value * 2}°`;
  if (name === 'Q' && lo === 0 && hi === 4) return String(value + 1); // SPECTRUM band width, 1 to 5
  return String(value);
}

/**
 * What a raw range spans in real units, for the hint bar, where the manual states it. The step between is not linear for rates and times, so only the ends are given.
 */
export function efxUnitNote(name: string, range: readonly [number, number]): string {
  const [lo, hi] = range;
  if (/(^|\s)Rate$/.test(name) && lo === 0 && hi === 125) return 'Raw 0–125 spans 0.05–10.0 Hz (not linear)';
  if (/Pre Delay Time( [AB])?$/.test(name) && lo === 0 && hi >= 125) return 'Raw 0–125 spans 0.0–100 ms (not linear)';
  if (/Delay Time( Left| Right)?$/.test(name) && lo === 0 && hi === 126) return 'Raw 0–126 spans 0.0–500 ms (not linear)';
  return '';
}
