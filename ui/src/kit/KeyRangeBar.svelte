<script lang="ts">
  import { clearHint, showHint } from './hint.svelte';
  import { moveEnd, nearerEnd, noteName, NOTE_MAX, octaveMarks, shiftRange, type KeyRange } from './keyRange';
  import PianoKeys from './PianoKeys.svelte';
  import { clippedSpan, noteAtPosition } from './rollGeometry';

  interface Props {
    low: number;
    high: number;
    name?: string;
    context?: string;
    color?: string;
    width?: number;
    /** Height of the keys, in pixels. */
    height?: number;
    disabled?: boolean;
    /** 'velocity' draws plain segments and numbers instead of a piano strip. */
    kind?: 'key' | 'velocity';
    /**
     * How a key range is drawn: `keys` is a real keyboard with the range lane under it (needs room: about 5 px per semitone);
     * `ruler` is octave bands with labels, for small sizes where 128 keys would only be noise.
     */
    display?: 'keys' | 'ruler';
    /** Lowest value either end may take (velocity starts at 1). */
    floor?: number;
    onchange?: (range: KeyRange) => void;
    onbegin?: () => void;
    onend?: () => void;
  }

  let {
    low = $bindable(),
    high = $bindable(),
    name = 'Key range',
    context = '',
    color = 'var(--accent)',
    width = 420,
    height = 30,
    disabled = false,
    kind = 'key',
    display = 'keys',
    floor = 0,
    onchange,
    onbegin,
    onend,
  }: Props = $props();

  /** Height of the lane under the keys that holds the range block, like a one-row piano roll. */
  const LANE = 16;
  const keyW = $derived(width / (NOTE_MAX + 1));
  let strip: HTMLDivElement | undefined = $state();
  let drag: { mode: 'low' | 'high' | 'body'; lastNote: number } | null = null;

  /** Left and right pixel edge of a note's key: every semitone is one equal step along the strip. */
  function edges(n: number): [number, number] {
    const [a, b] = kind === 'velocity' || display === 'ruler' ? [n, n + 1] : (clippedSpan(0, NOTE_MAX, n) ?? [n, n + 1]);
    return [a * keyW, b * keyW];
  }

  const noteAt = (e: PointerEvent): number => {
    const r = strip?.getBoundingClientRect();
    // Measure inside the border, where the keys are laid out.
    const x = e.clientX - (r?.left ?? 0) - (strip?.clientLeft ?? 0);
    const w = strip?.clientWidth || width;
    const row = (x / w) * (NOTE_MAX + 1);
    if (kind === 'velocity' || display === 'ruler') return Math.min(NOTE_MAX, Math.max(0, Math.floor(row)));
    const y = e.clientY - (r?.top ?? 0) - (strip?.clientTop ?? 0);
    return noteAtPosition(0, NOTE_MAX, row, y, Math.round(((height - LANE) * 2) / 3));
  };

  const label = (n: number): string => (kind === 'velocity' ? String(n) : noteName(n));
  const marks = $derived(octaveMarks(width));
  const RULER = 15;
  const ticks = Array.from({ length: 9 }, (_, i) => Math.min(127, i * 16));
  const rangeText = $derived(low === high ? label(low) : `${label(low)} – ${label(high)}`);

  // While dragging, the bar follows the pointer from its own state at once and tells the app once per frame, so the drag
  // stays smooth however much the screen has to redraw when a range changes.
  let pending: KeyRange | null = null;
  let frame = 0;
  const later = (fn: () => void): number => (typeof requestAnimationFrame === 'function' ? requestAnimationFrame(fn) : (setTimeout(fn, 16) as unknown as number));
  const cancelLater = (h: number): void => (typeof cancelAnimationFrame === 'function' ? cancelAnimationFrame(h) : clearTimeout(h));

  function flush(): void {
    if (frame) cancelLater(frame);
    frame = 0;
    const p = pending;
    pending = null;
    if (p) onchange?.(p);
  }

  function apply(raw: KeyRange, defer = false): void {
    const next = { low: Math.max(floor, raw.low), high: Math.max(floor, raw.high) };
    if (next.low === low && next.high === high) return;
    low = next.low;
    high = next.high;
    publish();
    if (!defer) {
      onchange?.(next);
      return;
    }
    pending = next;
    if (!frame) {
      frame = later(() => {
        frame = 0;
        flush();
      });
    }
  }

  const publish = (): void =>
    showHint({
      context,
      name,
      value: `${label(low)} to ${label(high)}`,
      raw: `${low} to ${high}`,
      shortcut: 'Drag an end, or the bar to shift',
    });

  function down(e: PointerEvent): void {
    if (disabled || drag || e.button !== 0) return;
    const n = noteAt(e);
    const inside = n > low && n < high;
    const target = e.target as HTMLElement;
    const handle = target.dataset.handle as 'low' | 'high' | undefined;
    drag = handle
      ? { mode: handle, lastNote: n }
      : inside && e.altKey
        ? { mode: 'body', lastNote: n }
        : { mode: nearerEnd({ low, high }, n), lastNote: n };
    strip?.setPointerCapture(e.pointerId);
    onbegin?.();
    if (drag.mode !== 'body') apply(moveEnd({ low, high }, drag.mode, n));
  }

  function move(e: PointerEvent): void {
    if (!drag) return;
    if (disabled) {
      cancel();
      return;
    }
    const n = noteAt(e);
    if (drag.mode === 'body') {
      apply(shiftRange({ low, high }, n - drag.lastNote), true);
      drag = { mode: 'body', lastNote: n };
    } else {
      apply(moveEnd({ low, high }, drag.mode, n), true);
    }
  }

  function cancel(): void {
    if (!drag) return;
    flush();
    drag = null;
    onend?.();
  }

  function up(e: PointerEvent): void {
    if (!drag) return;
    if (strip?.hasPointerCapture(e.pointerId)) strip.releasePointerCapture(e.pointerId);
    cancel();
  }

  // Removed mid-drag: still close the undo group.
  $effect(() => () => cancel());

  function key(end: 'low' | 'high', e: KeyboardEvent): void {
    const dir = e.key === 'ArrowRight' || e.key === 'ArrowUp' ? 1 : e.key === 'ArrowLeft' || e.key === 'ArrowDown' ? -1 : 0;
    if (dir === 0 || disabled) return;
    e.preventDefault();
    onbegin?.();
    apply(moveEnd({ low, high }, end, (end === 'low' ? low : high) + dir * (e.shiftKey ? 12 : 1)));
    onend?.();
  }
</script>

<div class="krb" class:disabled style:--c={color} style:width="{width + 2}px">
  <div
    class="strip"
    class:slider={kind === 'velocity'}
    style:width="{width}px"
    style:height="{height}px"
    bind:this={strip}
    role="group"
    aria-label={name}
    onpointerdown={down}
    onpointermove={move}
    onpointerup={up}
    onpointercancel={up}
    onpointerenter={publish}
    onpointerleave={() => {
      if (!drag) clearHint();
    }}
  >
    {#if kind === 'key' && display === 'ruler'}
      <svg class="lane" {width} {height} role="presentation">
        <rect {width} {height} fill="var(--lane-a)" />
        {#each marks as m (m.x)}
          <rect x={m.x} y="0" width={m.w} {height} fill={m.alt ? 'var(--ruler-a)' : 'var(--ruler-b)'} />
          <line x1={m.x} x2={m.x} y1="0" y2={height} stroke="var(--seam)" stroke-width="1" />
          {#if m.label}<text x={m.x + 3} y="11" class="oct">{m.label}</text>{/if}
        {/each}
        <line x1="0" x2={width} y1={RULER} y2={RULER} stroke="var(--seam-soft)" stroke-width="1" />
        <rect x={edges(low)[0]} y={RULER + 2} width={Math.max(3, edges(high)[1] - edges(low)[0])} height={height - RULER - 4} rx="3" fill={color} fill-opacity="0.9" stroke="#0e1417" stroke-opacity="0.7" />
        {#if edges(high)[1] - edges(low)[0] >= 66}
          <text x={(edges(low)[0] + edges(high)[1]) / 2} y={RULER + (height - RULER) / 2 + 4} text-anchor="middle" class="rng dark">{rangeText}</text>
        {:else if edges(high)[1] + 70 <= width}
          <text x={edges(high)[1] + 5} y={RULER + (height - RULER) / 2 + 4} class="rng">{rangeText}</text>
        {:else}
          <text x={edges(low)[0] - 5} y={RULER + (height - RULER) / 2 + 4} text-anchor="end" class="rng">{rangeText}</text>
        {/if}
      </svg>
    {:else if kind === 'key'}
      <PianoKeys {width} height={height - LANE} labels={false} />
      <svg class="lane" {width} height={LANE} role="presentation">
        <rect {width} height={LANE} fill="var(--lane-a)" />
        <rect x={edges(low)[0]} y="1.5" width={Math.max(2, edges(high)[1] - edges(low)[0])} height={LANE - 3} rx="2" fill={color} fill-opacity="0.9" stroke="#0e1417" stroke-opacity="0.7" />
        <!-- the range written on the strip: inside the block when it fits, beside it when it does not -->
        {#if edges(high)[1] - edges(low)[0] >= 62}
          <text x={(edges(low)[0] + edges(high)[1]) / 2} y={LANE - 4.5} text-anchor="middle" class="rng dark">{rangeText}</text>
        {:else if edges(high)[1] + 64 <= width}
          <text x={edges(high)[1] + 4} y={LANE - 4.5} class="rng">{rangeText}</text>
        {:else}
          <text x={edges(low)[0] - 4} y={LANE - 4.5} text-anchor="end" class="rng">{rangeText}</text>
        {/if}
      </svg>
    {:else}
      <div class="ramp" style:width="{width}px" style:height="{height}px">
        {#each ticks as t (t)}<i class="tick" class:major={t % 32 === 0 || t === 127} style:left="{t * keyW + keyW / 2}px"></i>{/each}
        <span class="fill" style:left="{low * keyW}px" style:width="{(high - low + 1) * keyW}px"></span>
      </div>
    {/if}
    <b class="handle" class:grip={kind === 'velocity'} data-handle="low" style:left="{edges(low)[0] - (kind === 'velocity' ? 5 : 0)}px" style:height="{height}px" role="slider" tabindex={disabled ? -1 : 0}
      aria-label="{name} low" aria-valuemin={0} aria-valuemax={high} aria-valuenow={low}
      aria-valuetext={label(low)} onkeydown={(e) => key('low', e)} onfocus={publish} onblur={clearHint}>{#if kind === 'velocity'}<s class="tip">{low}</s>{/if}</b>
    <b class="handle" class:grip={kind === 'velocity'} data-handle="high" style:left="{edges(high)[1] - (kind === 'velocity' ? 5 : 3)}px" style:height="{height}px" role="slider" tabindex={disabled ? -1 : 0}
      aria-label="{name} high" aria-valuemin={low} aria-valuemax={NOTE_MAX} aria-valuenow={high}
      aria-valuetext={label(high)} onkeydown={(e) => key('high', e)} onfocus={publish} onblur={clearHint}>{#if kind === 'velocity'}<s class="tip">{high}</s>{/if}</b>
  </div>
</div>

<style>
  .krb {
    display: inline-flex;
    flex-direction: column;
    gap: 2px;
  }
  .strip {
    position: relative;
    display: flex;
    /* the keys above, the range lane below: stacked, never side by side */
    flex-direction: column;
    box-sizing: content-box;
    background: var(--track);
    border: 1px solid var(--edge);
    border-radius: 2px;
    overflow: hidden;
    cursor: col-resize;
    touch-action: none;
    user-select: none;
  }
  .lane {
    display: block;
  }
  /* The velocity slider: a thin recessed groove with a scale, a lit range between two thumbs you take hold of and set. */
  .strip.slider {
    overflow: visible;
    background: transparent;
    border-color: transparent;
  }
  .ramp {
    position: relative;
    background: linear-gradient(var(--groove-a), var(--groove-b)) center / 100% 8px no-repeat;
    border-radius: 4px;
  }
  .tick {
    position: absolute;
    bottom: 1px;
    width: 1px;
    height: 3px;
    background: var(--tick);
  }
  .tick.major {
    height: 5px;
    background: var(--tick-major);
  }
  .fill {
    position: absolute;
    top: calc(50% - 3px);
    height: 6px;
    border-radius: 3px;
    background: linear-gradient(color-mix(in srgb, var(--c) 100%, #fff 22%), color-mix(in srgb, var(--c) 80%, #000));
    box-shadow: 0 0 0 1px rgba(0, 0, 0, 0.55), 0 0 6px color-mix(in srgb, var(--c) 45%, transparent);
  }
  .handle.grip {
    width: 11px;
    top: 2px;
    bottom: 2px;
    height: auto !important;
    border-radius: 4px;
    background:
      repeating-linear-gradient(90deg, transparent 0 1px, rgba(0, 0, 0, 0.35) 1px 2px, transparent 2px 3px) center / 5px 10px no-repeat,
      linear-gradient(#e6ebee, #a2adb3);
    box-shadow: 0 0 0 1px rgba(0, 0, 0, 0.65), 0 2px 3px rgba(0, 0, 0, 0.55);
    cursor: grab;
  }
  .handle.grip:hover {
    filter: brightness(1.12);
  }
  .strip:active .handle.grip {
    cursor: grabbing;
  }
  /* The value rides above the thumb while it is hovered, focused or being dragged. */
  .tip {
    position: absolute;
    left: 50%;
    bottom: calc(100% + 3px);
    transform: translateX(-50%);
    padding: 1px 5px;
    border-radius: 3px;
    background: var(--chip-bg);
    color: var(--chip-text);
    font: 600 10px var(--font-mono);
    text-decoration: none;
    box-shadow: 0 0 0 1px var(--chip-edge);
    opacity: 0;
    pointer-events: none;
    z-index: 2;
  }
  .handle:hover .tip,
  .handle:focus-visible .tip,
  .strip:active .tip {
    opacity: 1;
  }
  .handle {
    position: absolute;
    top: 0;
    width: 3px;
    background: var(--c);
    box-shadow: 0 0 0 1px rgba(0, 0, 0, 0.55);
    cursor: ew-resize;
    outline: none;
  }
  .handle:focus-visible {
    box-shadow: 0 0 0 2px var(--accent);
  }
  .oct {
    font: 600 9px var(--font-caps);
    fill: #8b959b;
    pointer-events: none;
  }
  .rng {
    font: 600 10px var(--font-caps);
    fill: var(--lane-text);
    pointer-events: none;
  }
  .rng.dark {
    fill: #0e1417;
  }
  .disabled {
    opacity: 0.4;
    pointer-events: none;
  }
</style>
