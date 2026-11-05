// Note and key-range helpers. MIDI note 60 is C4 (the common convention); the XP-60 manual numbers octaves from C-1.

const NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'] as const;
const BLACK = new Set([1, 3, 6, 8, 10]);

export const NOTE_MIN = 0;
export const NOTE_MAX = 127;

export function noteName(note: number): string {
  const n = Math.min(NOTE_MAX, Math.max(NOTE_MIN, Math.round(note)));
  return `${NAMES[n % 12]}${Math.floor(n / 12) - 1}`;
}

export function isBlackKey(note: number): boolean {
  return BLACK.has(((note % 12) + 12) % 12);
}

export interface KeyRange {
  low: number;
  high: number;
}

/** Move one end of a range; the ends can meet but never cross. */
export function moveEnd(range: KeyRange, end: 'low' | 'high', note: number): KeyRange {
  const n = Math.min(NOTE_MAX, Math.max(NOTE_MIN, Math.round(note)));
  return end === 'low' ? { low: Math.min(n, range.high), high: range.high } : { low: range.low, high: Math.max(n, range.low) };
}

/** Shift the whole range, keeping its width, stopping at the ends of the keyboard. */
export function shiftRange(range: KeyRange, by: number): KeyRange {
  const width = range.high - range.low;
  const low = Math.min(NOTE_MAX - width, Math.max(NOTE_MIN, range.low + Math.round(by)));
  return { low, high: low + width };
}

/** The nearer end to a pointer note, used to pick which handle a click on the strip should move. */
export function nearerEnd(range: KeyRange, note: number): 'low' | 'high' {
  // When the ends meet, the side of the click decides, so a one-note range can still be widened either way.
  if (note > range.high) return 'high';
  if (note < range.low) return 'low';
  return Math.abs(note - range.low) <= Math.abs(note - range.high) ? 'low' : 'high';
}

export interface OctaveMark {
  /** Left edge in pixels and width of this octave's band (C up to the next C, clipped at the last note). */
  x: number;
  w: number;
  /** Name of the C that starts the band ("C4"), or null when the ruler is too small to print it without crowding. */
  label: string | null;
  /** Alternate bands are lighter, so octaves can be told apart without a label. */
  alt: boolean;
}

/** Minimum pixels a label ("C-1" is the widest) needs to stay readable next to the next one. */
const LABEL_PX = 24;

/**
 * The octave bands of a key range ruler `width` pixels wide that spans notes 0 to 127. Every octave is a band; labels are
 * printed on every octave when there is room, otherwise on every second or fourth, so the ruler never turns into clutter.
 */
export function octaveMarks(width: number): OctaveMark[] {
  const unit = width / (NOTE_MAX + 1);
  const perOctave = unit * 12;
  const every = perOctave >= LABEL_PX ? 1 : perOctave * 2 >= LABEL_PX ? 2 : 4;
  const marks: OctaveMark[] = [];
  for (let o = 0; o * 12 <= NOTE_MAX; o++) {
    const x = o * 12 * unit;
    const end = Math.min(width, (o * 12 + 12) * unit);
    marks.push({ x, w: end - x, label: o % every === 0 ? noteName(o * 12) : null, alt: o % 2 === 1 });
  }
  return marks;
}
