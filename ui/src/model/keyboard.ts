// The on-screen and computer keyboard for auditioning a sound: which key plays which note, and which notes are held so
// none can be left sounding.

/** Semitones above the base C for each computer key (two rows, like a tracker keyboard). */
const KEY_OFFSETS: Readonly<Record<string, number>> = {
  KeyZ: 0,
  KeyS: 1,
  KeyX: 2,
  KeyD: 3,
  KeyC: 4,
  KeyV: 5,
  KeyG: 6,
  KeyB: 7,
  KeyH: 8,
  KeyN: 9,
  KeyJ: 10,
  KeyM: 11,
  Comma: 12,
  KeyQ: 12,
  Digit2: 13,
  KeyW: 14,
  Digit3: 15,
  KeyE: 16,
  KeyR: 17,
  Digit5: 18,
  KeyT: 19,
  Digit6: 20,
  KeyY: 21,
  Digit7: 22,
  KeyU: 23,
  KeyI: 24,
};

export const MIN_OCTAVE = 0;
export const MAX_OCTAVE = 8;

/** The MIDI note a computer key plays with `octave` as the base (4 puts the lower row at C4 = 60), or null. */
export function noteForCode(code: string, octave: number): number | null {
  const offset = KEY_OFFSETS[code];
  if (offset === undefined) return null;
  const note = (octave + 1) * 12 + offset;
  return note >= 0 && note <= 127 ? note : null;
}

const BLACK = new Set([1, 3, 6, 8, 10]);

export const isBlack = (note: number): boolean => BLACK.has(((note % 12) + 12) % 12);

export interface KeyShape {
  note: number;
  black: boolean;
  /** Left edge in white-key widths. */
  x: number;
}

/** Layout of `count` consecutive notes from `first`: white keys one unit wide, black keys centred on the gap. */
export function layoutKeys(first: number, count: number): KeyShape[] {
  const out: KeyShape[] = [];
  let whites = 0;
  for (let n = first; n < first + count && n <= 127; n++) {
    if (isBlack(n)) out.push({ note: n, black: true, x: whites - 0.3 });
    else {
      out.push({ note: n, black: false, x: whites });
      whites += 1;
    }
  }
  return out;
}

/** Velocity from where on the key the pointer is: near the top is soft, near the bottom hard. 1 to 127. */
export function velocityAt(fraction: number): number {
  const f = Math.min(1, Math.max(0, Number.isFinite(fraction) ? fraction : 0.5));
  return Math.round(30 + f * 97);
}

/** The notes currently sounding, so each is released exactly once and none can hang. */
export class HeldNotes {
  private readonly held = new Set<number>();

  /** True if the note was not already held (and should be sent). */
  press(note: number): boolean {
    if (this.held.has(note)) return false;
    this.held.add(note);
    return true;
  }

  /** True if the note was held (and its release should be sent). */
  release(note: number): boolean {
    return this.held.delete(note);
  }

  /** Release everything; returns the notes that were held. */
  releaseAll(): number[] {
    const notes = [...this.held];
    this.held.clear();
    return notes;
  }

  /** The held notes, without releasing them. */
  list(): number[] {
    return [...this.held];
  }

  has(note: number): boolean {
    return this.held.has(note);
  }

  get size(): number {
    return this.held.size;
  }
}
