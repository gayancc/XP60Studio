// Reference drawings of the XP-60's LFO, taken from the Owner's Manual (Chapter 3, "Modulating Sounds (LFO)"): the eight Forms (TRI, SIN, SAW, SQR, TRP, S&H,
// RND, CHS) and the four Fade Mode graphs (ON-IN, ON-OUT, OFF-IN, OFF-OUT). The manual gives no units for Rate, Delay or Fade, so the time course drawn here is
// schematic: it moves the right way with every setting but is not to scale.

export const LFO_FORM_NAMES = ['TRI', 'SIN', 'SAW', 'SQR', 'TRP', 'S&H', 'RND', 'CHS'] as const;
export const LFO_FORM_FULL = ['triangle wave', 'sine wave', 'sawtooth wave', 'square wave', 'trapezoidal wave', 'sample & hold wave', 'random wave', 'chaos wave'] as const;
export const FADE_MODES = ['ON-IN', 'ON-OUT', 'OFF-IN', 'OFF-OUT'] as const;
/** Offset list entries (0..4) as shown by the editor: -100, -50, 0, +50, +100. */
export const OFFSET_FRACTION = [-1, -0.5, 0, 0.5, 1] as const;

const frac = (x: number): number => x - Math.floor(x);

/** A repeatable pseudo-random value in -1..1 for step `n` (the manual's S&H, RND and CHS waves are random; the drawing keeps one fixed sequence). */
function noise(n: number): number {
  const s = Math.sin(n * 127.1 + 311.7) * 43758.5453;
  return (s - Math.floor(s)) * 2 - 1;
}

/** The waveform at `phase` (in cycles), from -1 (low) to +1 (high). Shapes follow the manual's chart; TRI starts on the centre line as on the XP-60's own display (Owner's Manual p.57). */
export function lfoWave(form: number, phase: number): number {
  const p = frac(phase);
  switch (form) {
    case 0: // TRI: the XP-60's own LFO display starts a triangle on the centre line, rising
      return p < 0.25 ? 4 * p : p < 0.75 ? 2 - 4 * p : 4 * p - 4;
    case 1: // SIN
      return Math.sin(2 * Math.PI * p);
    case 2: // SAW: rises from the centre, then falls straight back
      return p < 0.5 ? 2 * p : 2 * p - 2;
    case 3: // SQR
      return p < 0.5 ? 1 : -1;
    case 4: {
      // TRP: rises, holds high, falls, holds low (starting on the centre line like TRI)
      const q = frac(phase + 0.1);
      return q < 0.2 ? -1 + 10 * q : q < 0.5 ? 1 : q < 0.7 ? 1 - 10 * (q - 0.5) : -1;
    }
    case 5: // S&H: a new random level held for half a cycle
      return noise(Math.floor(phase * 2));
    case 6: {
      // RND: random levels joined smoothly
      const k = Math.floor(phase * 2);
      const t = frac(phase * 2);
      const w = (1 - Math.cos(Math.PI * t)) / 2;
      return noise(k) * (1 - w) + noise(k + 1) * w;
    }
    default: // CHS: no wavelength, a dense random signal
      return noise(Math.floor(phase * 40));
  }
}

/** Points (x 0..w, y 0..h) of `cycles` periods of one Form, for the small icons in the list. */
export function formPath(form: number, w: number, h: number, cycles = 2, pad = 1.5): string {
  const n = form === 7 ? 90 : 160;
  const out: string[] = [];
  for (let i = 0; i <= n; i++) {
    const x = (i / n) * w;
    const y = pad + ((1 - lfoWave(form, (i / n) * cycles + (form === 7 ? 0.003 : 0.0005))) / 2) * (h - 2 * pad);
    out.push(`${i === 0 ? 'M' : 'L'}${x.toFixed(2)} ${y.toFixed(2)}`);
  }
  return out.join('');
}

export interface LfoSettings {
  form: number;
  /** 0..127 */
  rate: number;
  /** 0..4, see {@link OFFSET_FRACTION} */
  offset: number;
  /** 0..127 */
  delay: number;
  /** 0..3, see {@link FADE_MODES} */
  fadeMode: number;
  /** 0..127 */
  fade: number;
  /** Key Trigger: the cycle restarts at each key press. */
  keySync: boolean;
}

export interface LfoTrace {
  /** The modulation over time, x 0..1, y -1..1 (1 = top of the drawing). */
  line: { x: number; y: number }[];
  /** The edge of the depth, where the manual draws its dashed "Depth" triangle: the offset line plus and minus the fade amount. */
  upper: { x: number; y: number }[];
  lower: { x: number; y: number }[];
  keyOn: number;
  /** Where the key is released; only the OFF modes are defined by that moment. */
  keyOff: number | null;
  /** Where the offset puts the middle line. */
  centre: number;
}

const AMPLITUDE = 0.5;
const KEY_ON = 0.02;
const HOLD_BEFORE_RELEASE = 0.16;

/** Width of the Delay and Fade stages on the 0..1 time axis; a larger setting gives a longer stage. */
export const delayWidth = (delay: number): number => 0.02 + 0.26 * (delay / 127);
export const fadeWidth = (fade: number): number => 0.04 + 0.5 * (fade / 127);

/** How strongly the LFO applies at time `t` (0..1) under a Fade Mode, following the manual's four graphs. */
export function fadeAmount(mode: number, t: number, delay: number, fade: number): number {
  const d = delayWidth(delay);
  const f = fadeWidth(fade);
  const start = mode >= 2 ? KEY_ON + HOLD_BEFORE_RELEASE + d : KEY_ON + d; // OFF modes wait for the release first
  const ramp = Math.min(1, Math.max(0, (t - start) / f));
  return mode % 2 === 0 ? (t < start ? 0 : ramp) : t < start ? 1 : 1 - ramp;
}

export function lfoTrace(s: LfoSettings, samples = 300): LfoTrace {
  const centre = (OFFSET_FRACTION[s.offset] ?? 0) * AMPLITUDE;
  const freq = s.form === 7 ? 1 : 2 + 14 * (s.rate / 127);
  const startPhase = s.keySync ? 0 : 0.37; // free-running: the cycle is wherever it happens to be
  const line: LfoTrace['line'] = [];
  const upper: LfoTrace['upper'] = [];
  const lower: LfoTrace['lower'] = [];
  for (let i = 0; i <= samples; i++) {
    const t = i / samples;
    const a = fadeAmount(s.fadeMode, t, s.delay, s.fade);
    const w = lfoWave(s.form, startPhase + Math.max(0, t - KEY_ON) * freq);
    line.push({ x: t, y: centre + a * w * AMPLITUDE });
    upper.push({ x: t, y: centre + a * AMPLITUDE });
    lower.push({ x: t, y: centre - a * AMPLITUDE });
  }
  return { line, upper, lower, keyOn: KEY_ON, keyOff: s.fadeMode >= 2 ? KEY_ON + HOLD_BEFORE_RELEASE : null, centre };
}
