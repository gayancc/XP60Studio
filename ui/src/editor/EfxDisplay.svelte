<script lang="ts">
  import type { EfxType } from '../model/efx';
  import { familyOf } from '../model/efxFamilies';

  // A small schematic of the selected effect, drawn from its own parameter values: relative shapes (how deep, how fast, how loud), never calibrated units. The manual's
  // rate and time tables are not linear and are not transcribed, so nothing here claims a number of hertz or milliseconds.
  interface Props {
    type: EfxType;
    /** The twelve stored parameter values, index 0 = parameter 1. */
    values: readonly number[];
  }
  let { type, values }: Props = $props();

  const W = 220;
  const H = 96;
  const family = $derived(familyOf(type.type).id);

  /** Normalised 0..1 value of the first used parameter whose name matches; `fallback` when the type has none. */
  function norm(re: RegExp, fallback = 0.5): number {
    const p = type.params.find((x) => x.raw && re.test(x.name));
    if (!p?.raw) return fallback;
    const [lo, hi] = p.raw;
    return hi === lo ? fallback : Math.min(1, Math.max(0, ((values[p.index - 1] ?? lo) - lo) / (hi - lo)));
  }
  const has = (re: RegExp): boolean => type.params.some((x) => x.raw && re.test(x.name));

  const clampN = (v: number): number => Math.min(1, Math.max(0, v));

  // --- shapes
  const wave = $derived.by(() => {
    const depth = norm(/Depth/, 0.5);
    const rate = norm(/Rate/, 0.4);
    const cycles = 1 + rate * 5;
    const pts: string[] = [];
    for (let i = 0; i <= 96; i++) {
      const x = (i / 96) * W;
      const y = H / 2 - Math.sin((i / 96) * cycles * Math.PI * 2) * (6 + depth * (H / 2 - 12));
      pts.push(`${i === 0 ? 'M' : 'L'}${x.toFixed(1)} ${y.toFixed(1)}`);
    }
    return pts.join(' ');
  });

  const taps = $derived.by(() => {
    const out: { x: number; h: number }[] = [{ x: 0, h: 1 }];
    const fb = clampN(norm(/Feedback/, 0.4));
    const times: number[] = [];
    if (has(/Delay Time (Left|Right|Center)|Delay Time \d/)) {
      for (const p of type.params) if (p.raw && /Delay Time (Left|Right|Center|\d)/.test(p.name)) times.push(norm(new RegExp(`^${p.name}$`)));
    } else times.push(norm(/Delay Time$|^Delay$/, 0.4));
    const lvls = times.map((_, i) => {
      const names = type.params.filter((p) => p.raw && /Level (\d|[LRC])|(Left|Right|Center) Level/.test(p.name));
      const p = names[i];
      return p ? norm(new RegExp(`^${p.name}$`), 0.8) : 0.85;
    });
    times.forEach((t, i) => out.push({ x: 0.12 + t * 0.7, h: lvls[i] ?? 0.8 }));
    // repeats from feedback, after the first tap
    const first = times[0] ?? 0.4;
    for (let r = 1; r <= 3; r++) {
      const x = 0.12 + first * 0.7 + (first * 0.7 + 0.04) * r;
      if (x < 0.98) out.push({ x, h: (lvls[0] ?? 0.8) * fb ** r });
    }
    return out;
  });

  const decay = $derived.by(() => {
    const gate = norm(/Gate Time|Time$/, 0.5);
    const pre = norm(/Pre Delay/, 0.1);
    const x0 = 6 + pre * 60;
    const len = 40 + gate * (W - 60 - x0 + 6);
    const pts: string[] = [];
    for (let i = 0; i <= 24; i++) {
      const x = x0 + (i / 24) * len;
      const y = H - 8 - (H - 24) * Math.exp(-(i / 24) * 3.2);
      pts.push(`${i === 0 ? 'M' : 'L'}${x.toFixed(1)} ${y.toFixed(1)}`);
    }
    return { path: pts.join(' '), x0 };
  });

  const curve = $derived.by(() => {
    const amount = norm(/Drive|Sustain|Threshold Level/, 0.5);
    const k = family === 'drive' ? 1 + amount * 9 : family === 'dynamics' ? 1 + (1 - amount) * 1.5 : 1;
    const pts: string[] = [];
    for (let i = 0; i <= 48; i++) {
      const u = (i / 48) * 2 - 1;
      const y = Math.tanh(u * k) / Math.tanh(k);
      pts.push(`${i === 0 ? 'M' : 'L'}${(20 + ((u + 1) / 2) * (W - 40)).toFixed(1)} ${(H / 2 - y * (H / 2 - 10)).toFixed(1)}`);
    }
    return pts.join(' ');
  });

  const bars = $derived.by(() => {
    const gains = type.params.filter((p) => p.raw && /Gain$/.test(p.name) && p.raw[1] === 30);
    const order = type.type === 5 ? gains : gains.filter((p) => !/^(Output|Post)/.test(p.name));
    return order.map((p) => ({ v: ((values[p.index - 1] ?? 15) - 15) / 15, name: p.name }));
  });

  const pitch = $derived.by(() => {
    const out: { st: number; pan: number }[] = [];
    for (const p of type.params) {
      if (!p.raw || !/^Coarse Pitch/.test(p.name)) continue;
      out.push({ st: (values[p.index - 1] ?? 24) - 24, pan: 0 });
    }
    return out;
  });

  const stages = $derived(type.name.replace(/\s*\((serial|parallel)\)/, '').split(/→|\//));
  const serial = $derived(type.name.includes('→'));
  const barW = $derived(bars.length ? Math.min(22, (W - 40) / bars.length - 4) : 0);
</script>

<svg viewBox="0 0 {W} {H}" width={W} height={H} role="img" aria-label="Sketch of {type.name}, drawn from its settings">
  <rect x="0" y="0" width={W} height={H} rx="3" class="bg" />
  <line x1="6" x2={W - 6} y1={H / 2} y2={H / 2} class="mid" />
  {#if type.type >= 26}
    {#each stages as s, i (i)}
      {@const w = 84}
      {@const x = serial ? 10 + i * 112 : 10 + i * 112}
      <rect {x} y={serial ? 30 : 12} width={w} height="34" rx="3" class="stage" />
      <text x={x + w / 2} y={serial ? 51 : 33} text-anchor="middle" class="t">{s.trim().toUpperCase()}</text>
    {/each}
    {#if serial}
      <path d="M96 47 H120 M114 42 L120 47 L114 52" class="line" />
    {:else}
      <text x={W / 2} y="82" text-anchor="middle" class="t dim">SIDE BY SIDE</text>
    {/if}
  {:else if family === 'chorus' || (family === 'filter' && type.type !== 8) || type.type === 8}
    <path d={wave} class="line" />
  {:else if family === 'delay'}
    {#each taps as t, i (i)}
      <rect x={10 + t.x * (W - 20)} y={H - 10 - Math.max(3, t.h * (H - 24))} width="5" height={Math.max(3, t.h * (H - 24))} class="bar" opacity={i === 0 ? 0.55 : 1} />
    {/each}
  {:else if family === 'reverb'}
    <path d={decay.path} class="line" />
    <line x1={decay.x0} x2={decay.x0} y1="10" y2={H - 8} class="mid" />
  {:else if family === 'drive' || family === 'dynamics'}
    <path d={curve} class="line" />
  {:else if family === 'pitch'}
    {#each pitch as p, i (i)}
      <rect x={30 + i * 90} y={H / 2 - 8 - (p.st / 12) * 34} width="60" height="16" rx="2" class="bar" />
      <text x={60 + i * 90} y={H - 6} text-anchor="middle" class="t dim">{p.st > 0 ? '+' : ''}{p.st} st</text>
    {/each}
  {:else if family === 'eq'}
    {#each bars as b, i (i)}
      {@const x = 20 + i * ((W - 40) / bars.length) + 2}
      <rect {x} y={b.v >= 0 ? H / 2 - b.v * (H / 2 - 10) : H / 2} width={barW} height={Math.max(1, Math.abs(b.v) * (H / 2 - 10))} class="bar" />
    {/each}
  {/if}
</svg>

<style>
  .bg {
    fill: var(--well);
  }
  .mid {
    stroke: var(--edge);
    stroke-width: 1;
  }
  .line {
    fill: none;
    stroke: var(--fx);
    stroke-width: 2;
    stroke-linejoin: round;
    stroke-linecap: round;
  }
  .bar {
    fill: var(--fx);
  }
  .stage {
    fill: var(--caption);
    stroke: none;
  }
  .t {
    font: 600 10px var(--font-caps);
    letter-spacing: 0.08em;
    fill: var(--text);
  }
  .dim {
    fill: var(--text-dim);
  }
  svg {
    display: block;
    flex: none;
  }
</style>
