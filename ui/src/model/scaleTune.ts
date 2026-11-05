// The keyboard scale: how far each of the twelve notes is moved from equal temperament, in cents (1/100 of a semitone). The instrument stores each note as 0 to 127
// for -64 to +63 cents, so equal temperament is 64 on every note. Tuning one octave tunes every octave (Owner's Manual, Tune display).

/** Notes of the octave in the order the instrument stores them. */
export const NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'] as const;

/** The raw value for equal temperament (0 cents). */
export const EQUAL_RAW = 64;

export const centsOf = (raw: number): number => raw - EQUAL_RAW;
export const rawOf = (cents: number): number => Math.min(127, Math.max(0, Math.round(cents) + EQUAL_RAW));

/** "+16", "−8" (a true minus sign) or "0". */
export function formatCents(cents: number): string {
  return cents > 0 ? `+${cents}` : cents < 0 ? `−${-cents}` : '0';
}

export interface ScalePreset {
  readonly id: 'equal' | 'pure' | 'arabian';
  readonly name: string;
  /** Cents for C, C#, D, D#, E, F, F#, G, G#, A, A#, B. */
  readonly cents: readonly number[];
  /** One line on what the scale is for. */
  readonly note: string;
}

/**
 * The three example scales the Owner's Manual prints in the Tune section (the table of note names against Equal temperament, Pure temperament with tonic C and
 * Arabian scale). They are starting points to edit, not a statement about what an instrument holds.
 */
export const SCALE_PRESETS: readonly ScalePreset[] = [
  { id: 'equal', name: 'Equal', cents: [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0], note: 'Twelve equal steps: the usual Western tuning. Every note at 0 cents.' },
  {
    id: 'pure',
    name: 'Pure, tonic C',
    cents: [0, -8, 4, 16, -14, -2, -10, 2, 14, -16, 14, -12],
    note: 'The three basic chords sound richer, but only in the key of C; transposing makes chords ambiguous.',
  },
  {
    id: 'arabian',
    name: 'Arabian',
    cents: [-6, 45, -2, -12, -51, -8, 43, -4, 47, 0, -10, -49],
    note: 'E and B about a quarter tone down, C#, F# and G# about a quarter tone up. Suits the keys of G, C and F.',
  },
];

/** True when every raw value is equal temperament. */
export const isEqualScale = (raws: readonly number[]): boolean => raws.every((r) => r === EQUAL_RAW);

/** The preset whose twelve notes match `raws` exactly, if any. */
export function presetOf(raws: readonly number[]): ScalePreset | undefined {
  return SCALE_PRESETS.find((p) => p.cents.length === raws.length && p.cents.every((c, i) => rawOf(c) === raws[i]));
}

/** Label of a tuning target: Part 1 to 16, or the Patch-mode scale. */
export const targetName = (index: number): string => (index === 16 ? 'Patch mode' : `Part ${index + 1}`);
