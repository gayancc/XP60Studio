<script lang="ts">
  import type { Snippet } from 'svelte';
  import { clearHint, showHint } from './hint.svelte';
  import { moveEnd, nearerEnd, noteName, NOTE_MAX, shiftRange, type KeyRange } from './keyRange';
  import PianoKeys from './PianoKeys.svelte';
  import { isAccidental } from './rollGeometry';

  // A key map: one real keyboard across the top and, under it on exactly the same scale, one lane per owner (a Part) showing
  // which keys it covers. Every semitone has the same width in the keyboard and in the lanes, so a block sits straight under
  // the keys it plays. The space is planned for the keys: the whole range is drawn at about 7 px per semitone (12 px per
  // white key), which is what real keys need to look like keys.

  export interface MapLane {
    id: number;
    low: number;
    high: number;
    color: string;
    /** Name of the owner, for the hint. */
    name: string;
    selected?: boolean;
    /** Drawn faint: the owner is switched off. */
    dim?: boolean;
  }

  interface Props {
    lanes: readonly MapLane[];
    /** What goes in the gutter, to the left of each lane (the part's number, its on switch, its patch). */
    gutter?: Snippet<[MapLane]>;
    keyboardHeight?: number;
    laneHeight?: number;
    gutterWidth?: number;
    context?: string;
    onselect?: (id: number) => void;
    onrange?: (id: number, range: KeyRange) => void;
    onbegin?: (id: number) => void;
    onend?: () => void;
    /** Double-click on a lane puts that owner's range back at the whole keyboard. */
    onreset?: (id: number) => void;
  }

  let {
    lanes,
    gutter,
    keyboardHeight = 56,
    laneHeight = 22,
    gutterWidth = 176,
    context = 'Key map',
    onselect,
    onrange,
    onbegin,
    onend,
    onreset,
  }: Props = $props();

  const GAP = 8;
  const EDGE = 6;
  let box = $state(0);
  /** Pixel width of the keys: whatever the container offers after the gutter, never less than a readable minimum. */
  const kbW = $derived(Math.max(420, (box || 900) - gutterWidth - GAP));
  const unit = $derived(kbW / (NOTE_MAX + 1));
  const H = $derived(lanes.length * laneHeight);
  const octaves = Array.from({ length: 11 }, (_, i) => i * 12);
  const columns = Array.from({ length: NOTE_MAX + 1 }, (_, n) => n).filter(isAccidental);

  let svg: SVGSVGElement | undefined = $state();
  let drag = $state<{ id: number; mode: 'low' | 'high' | 'body'; lastNote: number } | null>(null);

  // While a block is being dragged it follows the pointer from here, at once; the app's own state is told once per frame.
  // That keeps the drag smooth however much the rest of the screen has to redraw when a range changes.
  let cursor = $state('ew-resize');
  let live = $state<{ id: number; low: number; high: number } | null>(null);
  let queued: { id: number; range: KeyRange } | null = null;
  let frame = 0;
  const later = (fn: () => void): number => (typeof requestAnimationFrame === 'function' ? requestAnimationFrame(fn) : (setTimeout(fn, 16) as unknown as number));
  const cancelLater = (h: number): void => (typeof cancelAnimationFrame === 'function' ? cancelAnimationFrame(h) : clearTimeout(h));

  const shown = $derived(lanes.map((l) => (live && live.id === l.id ? { ...l, low: live.low, high: live.high } : l)));

  /** The lane whose keys are lit on the keyboard: the one being dragged, else the one under the pointer, else the selected one. */
  let hoverId = $state<number | null>(null);
  const active = $derived(shown.find((l) => l.id === (drag?.id ?? hoverId)) ?? shown.find((l) => l.selected) ?? null);
  const tints = $derived(active && !active.dim ? [{ low: active.low, high: active.high, color: active.color }] : []);
  const hasSelection = $derived(shown.some((l) => l.selected));

  function flush(): void {
    if (frame) cancelLater(frame);
    frame = 0;
    const q = queued;
    queued = null;
    if (q) onrange?.(q.id, q.range);
  }

  function propose(id: number, range: KeyRange): void {
    live = { id, ...range };
    queued = { id, range };
    if (!frame) {
      frame = later(() => {
        frame = 0;
        flush();
      });
    }
  }

  const x0 = (l: MapLane): number => l.low * unit;
  const x1 = (l: MapLane): number => (l.high + 1) * unit;
  const rangeText = (l: MapLane): string => (l.low === l.high ? noteName(l.low) : `${noteName(l.low)} – ${noteName(l.high)}`);

  function local(e: { clientX: number; clientY: number }): { x: number; y: number } {
    const r = svg?.getBoundingClientRect();
    return { x: (e.clientX - (r?.left ?? 0)) * (r?.width ? kbW / r.width : 1), y: (e.clientY - (r?.top ?? 0)) * (r?.height ? H / r.height : 1) };
  }

  const noteAtX = (x: number): number => Math.min(NOTE_MAX, Math.max(0, Math.floor(x / unit)));

  const announce = (l: MapLane): void =>
    showHint({ context, name: `${l.name} key range`, value: `${noteName(l.low)} to ${noteName(l.high)}`, raw: `${l.low} to ${l.high}`, shortcut: 'Drag an end, or the block to move it' });

  function down(e: PointerEvent): void {
    if (e.button !== 0) return;
    const { x, y } = local(e);
    const l = shown[Math.min(shown.length - 1, Math.max(0, Math.floor(y / laneHeight)))];
    if (!l) return;
    const note = noteAtX(x);
    // The grab zone at each end is at most a third of the block, so a one-note block is moved, not just stretched.
    const edge = Math.min(EDGE, (x1(l) - x0(l)) / 3);
    const mode: 'low' | 'high' | 'body' =
      x >= x0(l) && x <= x0(l) + edge
        ? 'low'
        : x <= x1(l) && x >= x1(l) - edge
          ? 'high'
          : x > x0(l) && x < x1(l)
            ? 'body'
            : nearerEnd({ low: l.low, high: l.high }, note);
    drag = { id: l.id, mode, lastNote: note };
    // The undo group opens before anything else can fail, so a drag is always one step.
    onselect?.(l.id);
    onbegin?.(l.id);
    try {
      svg?.setPointerCapture(e.pointerId);
    } catch {
      // No capture (a pointer the browser does not track): the drag still works while the pointer stays over the lanes.
    }
    if (mode !== 'body') propose(l.id, moveEnd({ low: l.low, high: l.high }, mode, note));
    announce(l);
  }

  function move(e: PointerEvent): void {
    const { x, y } = local(e);
    if (!drag) {
      const hover = shown[Math.floor(y / laneHeight)];
      hoverId = hover?.id ?? null;
      if (hover) {
        announce(hover);
        // Over the middle of a block it can be picked up and moved; at its ends or beside it, an end is set.
        const edge = Math.min(EDGE, (x1(hover) - x0(hover)) / 3);
        cursor = x > x0(hover) + edge && x < x1(hover) - edge ? 'grab' : 'ew-resize';
      }
      return;
    }
    const l = shown.find((c) => c.id === drag?.id);
    if (!l) return;
    const note = noteAtX(x);
    if (drag.mode === 'body') {
      const next = shiftRange({ low: l.low, high: l.high }, note - drag.lastNote);
      drag = { ...drag, lastNote: note };
      if (next.low !== l.low || next.high !== l.high) propose(l.id, next);
    } else {
      const next = moveEnd({ low: l.low, high: l.high }, drag.mode, note);
      if (next.low !== l.low || next.high !== l.high) propose(l.id, next);
    }
    announce(live ? { ...l, low: live.low, high: live.high } : l);
  }

  function up(e: PointerEvent): void {
    try {
      if (svg?.hasPointerCapture(e.pointerId)) svg.releasePointerCapture(e.pointerId);
    } catch {
      // Nothing was captured.
    }
    if (drag) {
      flush();
      drag = null;
      onend?.();
      live = null;
    }
  }

  // Removed mid-drag: still close the undo group.
  $effect(() => () => {
    if (frame) cancelLater(frame);
    if (drag) {
      const q = queued;
      if (q) onrange?.(q.id, q.range);
      onend?.();
    }
  });
</script>

<div class="km" bind:clientWidth={box} style:--gutter="{gutterWidth}px" style:--gap="{GAP}px">
  <div class="left">
    <div class="spacer" style:height="{keyboardHeight}px"></div>
    {#each lanes as l (l.id)}
      <div class="g" class:sel={l.selected} class:off={l.dim} style:height="{laneHeight}px" style:--c={l.color}>
        {#if gutter}{@render gutter(l)}{/if}
      </div>
    {/each}
  </div>
  <div class="right">
    <div class="kb" style:--c={active?.color ?? 'transparent'}>
      <PianoKeys width={kbW} height={keyboardHeight} {tints} />
      {#if active && !active.dim}
        <!-- the live readout: both ends of the active range ride on top of the keyboard while it is edited -->
        <span class="chip" class:drag={!!drag} style:left="{Math.min(kbW - 30, Math.max(0, x0(active) - 2))}px">{noteName(active.low)}</span>
        {#if active.high !== active.low}
          <span class="chip" class:drag={!!drag} style:left="{Math.min(kbW - 30, Math.max(0, x1(active) - 26))}px">{noteName(active.high)}</span>
        {/if}
      {/if}
    </div>
    <svg
      bind:this={svg}
      class="lanes"
      width={kbW}
      height={H}
      viewBox="0 0 {kbW} {H}"
      role="group"
      aria-label="{context}: key range of each part"
      onpointerdown={down}
      onpointermove={move}
      style:cursor={drag ? (drag.mode === 'body' ? 'grabbing' : 'ew-resize') : cursor}
      onpointerup={up}
      ondblclick={(e) => {
        const l = shown[Math.floor(local(e).y / laneHeight)];
        if (l) onreset?.(l.id);
      }}
      onpointercancel={up}
      onpointerleave={() => {
        hoverId = null;
        if (!drag) clearHint();
      }}
    >
      {#each shown as l, i (l.id)}
        <rect x="0" y={i * laneHeight} width={kbW} height={laneHeight} fill={i % 2 === 0 ? 'var(--lane-a)' : 'var(--lane-b)'} />
        {#if l.selected}<rect x="0" y={i * laneHeight} width={kbW} height={laneHeight} fill={l.color} fill-opacity="0.16" />{/if}
      {/each}
      <!-- black-key columns are a little darker, as the black-key rows are in a piano roll -->
      {#each columns as n (n)}
        <rect x={n * unit} y="0" width={unit} height={H} fill="var(--lane-col)" />
      {/each}
      {#each octaves as c (c)}
        <line x1={c * unit} x2={c * unit} y1="0" y2={H} stroke="var(--seam)" stroke-width="1" />
        {#if c + 5 <= NOTE_MAX}<line x1={(c + 5) * unit} x2={(c + 5) * unit} y1="0" y2={H} stroke="var(--seam-soft)" stroke-width="1" />{/if}
      {/each}
      {#each shown as _, i (i)}
        <line x1="0" x2={kbW} y1={(i + 1) * laneHeight} y2={(i + 1) * laneHeight} stroke="var(--lane-line)" stroke-width="1" />
      {/each}
      {#each shown as l, i (l.id)}
        {@const y = i * laneHeight + 3}
        {@const h = laneHeight - 6}
        {@const bx = x0(l)}
        {@const bw = Math.max(5, x1(l) - x0(l))}
        {@const gripFill = l.dim ? 0.25 : 0.9}
        {#if l.selected && !l.dim}
          <rect x={bx - 3} y={y - 3} width={bw + 6} height={h + 6} rx="5" fill={l.color} fill-opacity="0.22" pointer-events="none" />
        {/if}
        <rect x={bx} {y} width={bw} height={h} rx="2.5" fill={l.color} fill-opacity={l.dim ? 0.2 : l.selected ? 1 : hasSelection ? 0.5 : 0.8} stroke="#0e1417" stroke-opacity="0.75" />
        <rect x={bx + 1} y={y + 1} width={Math.max(0, bw - 2)} height={(h - 2) / 2} rx="1.5" fill="#fff" fill-opacity={l.dim ? 0.03 : 0.16} />
        <!-- raised grips at both ends, each with two grooves, so the block reads as something to take hold of -->
        {#each [bx, bx + bw - 5] as gx, k (k)}
          <rect x={gx} {y} width="5" height={h} rx="2" fill="#d5dcdf" fill-opacity={gripFill} stroke="#0e1417" stroke-opacity="0.6" />
          <line x1={gx + 1.8} x2={gx + 1.8} y1={y + 3} y2={y + h - 3} stroke="#0e1417" stroke-opacity={l.dim ? 0.1 : 0.35} />
          <line x1={gx + 3.2} x2={gx + 3.2} y1={y + 3} y2={y + h - 3} stroke="#0e1417" stroke-opacity={l.dim ? 0.1 : 0.35} />
        {/each}
        {#if x1(l) - x0(l) >= 84}
          <text x={(x0(l) + x1(l)) / 2} y={y + h / 2 + 3.5} text-anchor="middle" class="t dark" opacity={l.dim ? 0.5 : 1}>{rangeText(l)}</text>
        {:else if x1(l) + 74 <= kbW}
          <text x={x1(l) + 6} y={y + h / 2 + 3.5} class="t">{rangeText(l)}</text>
        {:else}
          <text x={x0(l) - 6} y={y + h / 2 + 3.5} text-anchor="end" class="t">{rangeText(l)}</text>
        {/if}
      {/each}
    </svg>
  </div>
</div>

<style>
  .km {
    display: grid;
    grid-template-columns: var(--gutter) minmax(0, 1fr);
    column-gap: var(--gap);
    align-items: start;
    width: 100%;
  }
  .left {
    display: flex;
    flex-direction: column;
  }
  .g {
    display: flex;
    align-items: center;
    gap: 8px;
    padding: 0 6px 0 8px;
    border-left: 3px solid var(--c);
    border-bottom: 1px solid var(--lane-line);
    background: var(--body-deep);
    min-width: 0;
  }
  .g.sel {
    background: color-mix(in srgb, var(--c) 14%, var(--body-deep));
  }
  .g.off > :global(:not(.tag)) {
    opacity: 0.5;
  }
  .right {
    min-width: 0;
  }
  .kb {
    position: relative;
    border: 1px solid var(--edge);
    border-bottom: 0;
    line-height: 0;
  }
  .chip {
    position: absolute;
    top: 2px;
    min-width: 26px;
    padding: 1px 4px;
    border-radius: 3px;
    background: var(--c);
    color: #0e1417;
    font: 700 10px/12px var(--font-mono);
    text-align: center;
    box-shadow: 0 1px 3px rgba(0, 0, 0, 0.6);
    pointer-events: none;
    transition: transform 0.08s;
  }
  .chip.drag {
    transform: translateY(-1px) scale(1.08);
  }
  .g.sel {
    background: color-mix(in srgb, var(--c) 26%, var(--body-deep));
  }
  .g:not(.sel) :global(.tag) {
    opacity: 0.75;
  }
  .kb :global(svg) {
    cursor: default;
  }
  .lanes {
    display: block;
    touch-action: none;
    user-select: none;
    border: 1px solid var(--edge);
    border-top: 0;
  }
  .t {
    font: 600 10.5px var(--font-caps);
    fill: var(--lane-text);
    pointer-events: none;
  }
  .t.dark {
    fill: #0e1417;
  }
</style>
