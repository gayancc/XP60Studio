// Heuristic waveform base-energy priors for the SALE Estimated Loudness Score (ELS).
// PoC category weights from Roland wave *names* and, when waves are unknown (EXP/PCM),
// from the patch name. Not measured spectra — tune against listening on the XP-60.

import { waveName } from './factory';

/** Baseline energy factor in ~0.15..1.15 for an XP-60 wave / patch category. */
export type WaveEnergy = number;

const NEUTRAL: WaveEnergy = 0.7;

/**
 * Keyword → energy. First match wins.
 * Synthetic / distorted / bright leads score high (they cut through).
 * Soft acoustic / dark winds score lower (they sit back).
 */
const WAVE_RULES: ReadonlyArray<{ re: RegExp; energy: WaveEnergy; label: string }> = [
  { re: /tb[- ]?303|acid|dist|overdrive|fuzz/i, energy: 1.28, label: 'acid-dist' },
  { re: /saw|supersaw|synsaw|syn\s*brass/i, energy: 1.05, label: 'saw' },
  { re: /square|pulse|pwm/i, energy: 1.0, label: 'square' },
  { re: /\blead\b|solo\s*syn|sync\b/i, energy: 1.05, label: 'lead' },
  { re: /flash|screaming|scream/i, energy: 1.2, label: 'bright-lead' },
  { re: /alto\s*sax|tenor\s*sax|sax|reed/i, energy: 1.08, label: 'sax' },
  { re: /noise|cym|hi[- ]?hat|hh\b|crash|ride|shaker/i, energy: 0.7, label: 'noise' },
  { re: /\bsine\b|sinwave|pure/i, energy: 0.38, label: 'sine' },
  { re: /flute|panflute|recorder|whistle|soft|harmonica|harm\b/i, energy: 0.42, label: 'soft-wind' },
  { re: /organ|drawbar|hammond/i, energy: 0.82, label: 'organ' },
  { re: /piano|epiano|e\.?piano|rhodes|wurli|clav|celesta|harpsi/i, energy: 0.55, label: 'keys' },
  { re: /guitar|gtr|pluck|harp|koto|sitar/i, energy: 0.58, label: 'pluck' },
  { re: /bass|bs\b|sub\b/i, energy: 0.78, label: 'bass' },
  { re: /string|violin|viola|cello|ensemble|orch|strins/i, energy: 0.72, label: 'strings' },
  { re: /pad|atmos|voice|choir|aah|ooh/i, energy: 0.62, label: 'pad' },
  // Acoustic brass / mariachi sit quieter than synth brass at the same raw level.
  { re: /mariachi|trump|tp\b|cornet|flugel/i, energy: 0.42, label: 'acoustic-brass' },
  { re: /brass|trombon|horn|sect/i, energy: 0.7, label: 'brass' },
  { re: /oboe|clarinet|bassoon/i, energy: 0.55, label: 'woodwind' },
  { re: /bell|chime|mallet|marimba|vibra|xyl|glock/i, energy: 0.72, label: 'mallet' },
  { re: /drum|kick|snare|tom|perc|conga|bongo|timb/i, energy: 0.55, label: 'perc' },
];

/** Patch-title hints when the wave itself is unnamed (EXP / missing list). */
const PATCH_RULES: ReadonlyArray<{ re: RegExp; energy: WaveEnergy; label: string }> = [
  { re: /tb[- ]?303|acid|dist\b/i, energy: 1.3, label: 'patch-acid' },
  { re: /flash\s*sax|scream|tearing/i, energy: 1.22, label: 'patch-flash-sax' },
  { re: /\bsax/i, energy: 1.05, label: 'patch-sax' },
  { re: /mariachi|\btp\b|trumpet/i, energy: 0.4, label: 'patch-mariachi' },
  { re: /flute|harmonica|soft/i, energy: 0.4, label: 'patch-soft' },
  { re: /lead|syn|saw/i, energy: 1.0, label: 'patch-lead' },
];

export function waveEnergyFromName(name: string | undefined): { energy: WaveEnergy; category: string } {
  if (!name) return { energy: NEUTRAL, category: 'unknown' };
  for (const rule of WAVE_RULES) {
    if (rule.re.test(name)) return { energy: rule.energy, category: rule.label };
  }
  return { energy: NEUTRAL, category: 'generic' };
}

export function patchNameEnergyHint(patchName: string | undefined): { energy: WaveEnergy; category: string } | null {
  if (!patchName?.trim()) return null;
  for (const rule of PATCH_RULES) {
    if (rule.re.test(patchName)) return { energy: rule.energy, category: rule.label };
  }
  return null;
}

/**
 * `W_base` for a Tone. Internal waves use the name LUT. When the wave has no listed name
 * (typical for EXP), fall back to the patch title so "FLASH SAX" / "Dist TB-303" are not
 * stuck on a bland neutral that under-cuts them.
 */
export function waveformBaseEnergy(
  groupType: number,
  groupId: number,
  waveNumberRaw: number,
  patchName?: string,
): {
  energy: WaveEnergy;
  category: string;
  name: string | undefined;
} {
  const name = waveName(groupType, groupId, waveNumberRaw + 1);
  if (name) {
    const hit = waveEnergyFromName(name);
    // Still allow patch title to raise energy for known screamers when the wave name is mild.
    const hint = patchNameEnergyHint(patchName);
    if (hint && hint.energy > hit.energy) return { energy: hint.energy, category: hint.category, name };
    return { ...hit, name };
  }
  const hint = patchNameEnergyHint(patchName);
  if (hint) return { energy: hint.energy, category: hint.category, name: undefined };
  return { energy: NEUTRAL, category: 'unknown', name: undefined };
}

export const WAVE_ENERGY_NEUTRAL = NEUTRAL;
