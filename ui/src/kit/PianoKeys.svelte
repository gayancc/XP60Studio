<script lang="ts">
  import { noteName } from './keyRange';
  import { blackWidth, clippedSpan, isAccidental, keyOutline, noteAtPosition } from './rollGeometry';

  interface Props {
    first?: number;
    last?: number;
    /** Pixel box of the keyboard. In `vertical` the pitch axis is the height, in `horizontal` the width. */
    width: number;
    height: number;
    orientation?: 'horizontal' | 'vertical';
    /** Notes drawn as pressed. */
    held?: ReadonlySet<number>;
    /** The one note drawn with an outline, for example the selected rhythm key. */
    marked?: number | null;
    color?: string;
    /** Print the octave name on every C. */
    labels?: boolean;
    /** Note spans washed with a colour on the keys themselves, for example the keys one Tone plays. */
    tints?: readonly { low: number; high: number; color: string }[];
    ondown?: (note: number, e: PointerEvent) => void;
    onmove?: (note: number, e: PointerEvent) => void;
    onup?: (e: PointerEvent) => void;
    svg?: SVGSVGElement | undefined;
  }

  let {
    first = 0,
    last = 127,
    width,
    height,
    orientation = 'horizontal',
    held,
    marked = null,
    color = 'var(--accent)',
    labels = true,
    tints = [],
    ondown,
    onmove,
    onup,
    svg = $bindable(),
  }: Props = $props();

  const uid = `pk${Math.random().toString(36).slice(2, 9)}`;
  const vertical = $derived(orientation === 'vertical');
  const rows = $derived(last - first + 1);
  /** Pixels per semitone row along the pitch axis, and the size across the keyboard. */
  const unit = $derived((vertical ? height : width) / rows);
  const across = $derived(vertical ? width : height);
  const blackLen = $derived(blackWidth(across));

  interface Face {
    note: number;
    x: number;
    y: number;
    w: number;
    h: number;
    black: boolean;
  }

  /** The face of a note, clipped to the window. Vertical: higher notes up and black keys start at the left edge. */
  function face(note: number): Face | null {
    const s = clippedSpan(first, last, note);
    if (!s) return null;
    const black = isAccidental(note);
    const len = black ? blackLen : across;
    return vertical
      ? { note, black, x: 0, y: (last + 1 - s[1]) * unit, w: len, h: (s[1] - s[0]) * unit }
      : { note, black, x: (s[0] - first) * unit, y: 0, w: (s[1] - s[0]) * unit, h: len };
  }

  const faces = $derived.by(() => {
    const out: Face[] = [];
    for (let n = Math.max(0, first - 1); n <= Math.min(127, last + 1); n++) {
      const f = face(n);
      if (f) out.push(f);
    }
    return out;
  });
  const naturals = $derived(faces.filter((f) => !f.black));
  const accidentals = $derived(faces.filter((f) => f.black));
  const tintOf = (note: number): string | null => tints.find((t) => note >= t.low && note <= t.high)?.color ?? null;
  const tintedNaturals = $derived(naturals.flatMap((f) => { const c = tintOf(f.note); return c ? [{ f, c }] : []; }));
  const tintedAccidentals = $derived(accidentals.flatMap((f) => { const c = tintOf(f.note); return c ? [{ f, c }] : []; }));
  const octaves = Array.from({ length: 11 }, (_, i) => i * 12);
  /** Rows (from note 0) where a full-width separator is drawn: the C boundary and the E/F boundary. */
  const seams = $derived(octaves.flatMap((c) => [c, c + 5]).filter((r) => r > first && r < last + 1));
  const cNotes = $derived(octaves.filter((c) => c >= first && c <= last && c <= 127));
  const showLabels = $derived(labels && unit * 12 >= (vertical ? 14 : 30));

  const pos = (row: number): number => (vertical ? (last + 1 - row) * unit : (row - first) * unit);

  function locate(e: PointerEvent): number {
    const r = (svg ?? (e.currentTarget as SVGSVGElement)).getBoundingClientRect();
    const px = ((e.clientX - r.left) / (r.width || 1)) * width;
    const py = ((e.clientY - r.top) / (r.height || 1)) * height;
    const row = vertical ? last + 1 - py / unit : first + px / unit;
    return noteAtPosition(first, last, row, vertical ? px : py, blackLen);
  }

  const o = $derived(vertical ? 'v' : 'h');
  const fill = (f: Face): string =>
    held?.has(f.note) ? color : f.black ? `url(#${uid}b${o})` : `url(#${uid}${f.note % 12 === 0 ? 'c' : 'w'}${o})`;
</script>

<svg
  bind:this={svg}
  class="piano"
  {width}
  {height}
  viewBox="0 0 {width} {height}"
  role="presentation"
  onpointerdown={(e) => ondown?.(locate(e), e)}
  onpointermove={(e) => onmove?.(locate(e), e)}
  onpointerup={(e) => onup?.(e)}
  onpointercancel={(e) => onup?.(e)}
>
  <defs>
    <!-- the shading runs across the key: from the black-key side to the open end -->
    <linearGradient id="{uid}w{o}" x1="0" y1="0" x2={vertical ? 1 : 0} y2={vertical ? 0 : 1}>
      <stop offset="0" stop-color="#dfe4e7" />
      <stop offset="0.7" stop-color="#eef1f3" />
      <stop offset="1" stop-color="#f7f9fa" />
    </linearGradient>
    <linearGradient id="{uid}c{o}" x1="0" y1="0" x2={vertical ? 1 : 0} y2={vertical ? 0 : 1}>
      <stop offset="0" stop-color="#c3cbd0" />
      <stop offset="0.7" stop-color="#d3d9dd" />
      <stop offset="1" stop-color="#e2e7ea" />
    </linearGradient>
    <linearGradient id="{uid}b{o}" x1="0" y1="0" x2={vertical ? 1 : 0} y2={vertical ? 0 : 1}>
      <stop offset="0" stop-color="#2a2e31" />
      <stop offset="0.8" stop-color="#3d4347" />
      <stop offset="1" stop-color="#565d62" />
    </linearGradient>
  </defs>
  <rect x="0" y="0" {width} {height} fill="#1c2124" />

  <!-- natural faces first (uninterrupted), then the shorter accidentals on top -->
  {#each naturals as f (f.note)}
    <rect x={f.x} y={f.y} width={f.w} height={f.h} fill={fill(f)} />
  {/each}
  {#each tintedNaturals as t (t.f.note)}
    <rect x={t.f.x} y={t.f.y} width={t.f.w} height={t.f.h} fill={t.c} fill-opacity="0.62" pointer-events="none" />
    {#if !vertical}<rect x={t.f.x} y={t.f.h - 3} width={t.f.w} height="3" fill={t.c} pointer-events="none" />{/if}
  {/each}
  {#each seams as r (r)}
    {#if vertical}
      <line x1="0" x2={width} y1={pos(r)} y2={pos(r)} stroke="#79858b" stroke-width="1" />
    {:else}
      <line x1={pos(r)} x2={pos(r)} y1="0" y2={height} stroke="#79858b" stroke-width="1" />
    {/if}
  {/each}
  {#each accidentals as f (f.note)}
    <rect x={f.x + 0.5} y={f.y + 0.5} width={Math.max(2, f.w - 1)} height={Math.max(2, f.h - 1)} rx="1" fill={fill(f)} stroke="#15181a" stroke-width="0.8" />
  {/each}
  {#each tintedAccidentals as t (t.f.note)}
    <rect x={t.f.x + 0.5} y={t.f.y + 0.5} width={Math.max(2, t.f.w - 1)} height={Math.max(2, t.f.h - 1)} rx="1" fill={t.c} fill-opacity="0.8" pointer-events="none" />
  {/each}

  {#if showLabels}
    {#each cNotes as c (c)}
      {#if vertical}
        <text x={width - 4} y={pos(c + 0.5) + 3} text-anchor="end" class="lbl">{noteName(c)}</text>
      {:else}
        <text x={pos(c + 0.75)} y={height - 3} text-anchor="middle" class="lbl">{noteName(c)}</text>
      {/if}
    {/each}
  {/if}

  {#if marked !== null}
    {@const outline = keyOutline(first, last, marked, blackLen / across)}
    {#if outline}
      <!-- only what can be seen of the key: a white key's outline goes round the black keys that cover it -->
      <polygon
        points={outline.map(([a, c]) => (vertical ? `${c * width},${(last + 1 - a) * unit}` : `${(a - first) * unit},${c * height}`)).join(' ')}
        fill={color}
        fill-opacity="0.32"
        stroke={color}
        stroke-width="1.6"
        stroke-linejoin="round"
        pointer-events="none"
      />
    {/if}
  {/if}
</svg>

<style>
  .piano {
    display: block;
    touch-action: none;
    user-select: none;
    cursor: pointer;
  }
  .lbl {
    font: 600 9px var(--font-caps);
    fill: #434e55;
    pointer-events: none;
  }
</style>
