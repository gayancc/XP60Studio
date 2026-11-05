// What each of the 40 EFX (multi-effect) types calls its twelve parameters. Generated into data/efx-parameters.json by scripts/build_efx_data.py from Roland's
// Owner's Manual appendix "Correspondence of the EFX Algorithm and Exclusive Address (EFX Parameter 1-12)": parameter 1 to 12 of each type, with its name and its raw
// range. 339 of the 351 parameter names were checked against the XP-60/80 manual's own parameter list (the others differ only in spelling, such as "Over Drive Pan").

import raw from '../../../data/efx-parameters.json';

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

/** "19 TRIPLE-TAP-DELAY", for the type list. */
export const efxTypeLabel = (t: EfxType): string => `${String(t.type).padStart(2, '0')} ${t.name}`;

// Words shortened so a name fits under a knob. Only whole words are replaced, and only with the spelling the manual itself uses in its short column.
const SHORT: ReadonlyArray<readonly [RegExp, string]> = [
  [/\bFrequency\b/g, 'Freq'],
  [/\bOutput Level\b/g, 'Level'],
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

/**
 * The meaning of a raw value, where the manual's own range settles it: a gain given as -15 to +15 dB over 0 to 30, the effect balance given as D100:0W to D0:100W over 0
 * to 100, and the high-frequency damp list of 17 frequencies and BYPASS over 0 to 17. Anything else is shown as its raw number: the manual's tables for rates and delay
 * times are not linear and are not transcribed here.
 */
export function efxValueText(name: string, range: readonly [number, number], value: number): string {
  const [lo, hi] = range;
  if (/\bGain$/.test(name) && lo === 0 && hi === 30) {
    const db = value - 15;
    return `${db > 0 ? '+' : ''}${db} dB`;
  }
  if (/\bBalance$/.test(name) && lo === 0 && hi === 100) return `D${100 - value}:${value}W`;
  if (/HF Damp$/.test(name) && lo === 0 && hi === 17) return HF_DAMP[value] ?? String(value);
  return String(value);
}
