<script lang="ts">
  import { clearHint, showHint } from './hint.svelte';
  import { moveEnd, nearerEnd, noteName, shiftRange, type KeyRange } from './keyRange';
  import PianoKeys from './PianoKeys.svelte';
  import { noteAtRow, rowHeight, yOfRow, type RollView } from './rollGeometry';

  // A vertical keyboard with the FL key shapes, and next to it a plain strip that shows what each owner of the keys
  // covers: a coloured block per range, or a bar per key. There is no note grid: this is not a note editor.

  export interface RollRange {
    lane: number;
    low: number;
    high: number;
    color: string;
    /** Name of the owner (a Tone), used for the hint. */
    name: string;
    selected?: boolean;
    /** Drawn faint: the owner is switched off (a Part that does not receive). */
    dim?: boolean;
  }

  export interface RollBar {
    note: number;
    /** 0..1 of the strip width. */
    value: number;
    color: string;
    dim?: boolean;
    selected?: boolean;
  }

  interface Props {
    first: number;
    last: number;
    width: number;
    height: number;
    kbW?: number;
    /** Columns of the strip (one per range owner); 1 when it holds bars. */
    lanes?: number;
    ranges?: readonly RollRange[];
    bars?: readonly RollBar[];
    held?: ReadonlySet<number>;
    marked?: number | null;
    context?: string;
    onkey?: (note: number, down: boolean) => void;
    onrange?: (lane: number, range: KeyRange) => void;
    onbegin?: (lane: number) => void;
    onend?: () => void;
    onbar?: (note: number) => void;
    /** Ask for a different first visible note (wheel, overview click). */
    onview?: (first: number) => void;
  }

  let {
    first,
    last,
    width,
    height,
    kbW = 64,
    lanes = 1,
    ranges = [],
    bars = [],
    held,
    marked = null,
    context = 'Keys',
    onkey,
    onrange,
    onbegin,
    onend,
    onbar,
    onview,
  }: Props = $props();

  const OVER_W = 10;
  const view = $derived<RollView>({ first, last, height });
  const rh = $derived(rowHeight(view));
  const stripW = $derived(width - kbW - OVER_W);
  const laneW = $derived(stripW / Math.max(1, lanes));

  let strip: SVGSVGElement | undefined = $state();
  let keyDrag: number | null = null;
  let rangeDrag: { lane: number; mode: 'low' | 'high' | 'body'; lastNote: number } | null = null;

  function local(e: { clientX: number; clientY: number }): { x: number; y: number } {
    const r = strip?.getBoundingClientRect();
    return { x: (e.clientX - (r?.left ?? 0)) * (r?.width ? stripW / r.width : 1), y: (e.clientY - (r?.top ?? 0)) * (r?.height ? height / r.height : 1) };
  }

  const announce = (r: RollRange): void =>
    showHint({ context, name: `${r.name} key range`, value: `${noteName(r.low)} to ${noteName(r.high)}`, raw: `${r.low} to ${r.high}`, shortcut: 'Drag an edge, or the block to shift' });

  // keyboard: press and slide
  function keyDown(note: number, e: PointerEvent): void {
    if (e.button !== 0) return;
    (e.currentTarget as Element).setPointerCapture(e.pointerId);
    keyDrag = note;
    onkey?.(note, true);
  }

  function keyMove(note: number): void {
    if (keyDrag === null || note === keyDrag) return;
    onkey?.(keyDrag, false);
    keyDrag = note;
    onkey?.(note, true);
  }

  function keyUp(e: PointerEvent): void {
    const el = e.currentTarget as Element;
    if (el.hasPointerCapture(e.pointerId)) el.releasePointerCapture(e.pointerId);
    if (keyDrag !== null) onkey?.(keyDrag, false);
    keyDrag = null;
  }

  // strip: edit ranges, or pick a bar
  function stripDown(e: PointerEvent): void {
    if (e.button !== 0) return;
    const { x, y } = local(e);
    const note = noteAtRow(view, y);
    if (bars.length > 0) {
      onbar?.(note);
      return;
    }
    const lane = Math.min(lanes - 1, Math.max(0, Math.floor(x / laneW)));
    const r = ranges.find((b) => b.lane === lane);
    if (!r) return;
    const topY = yOfRow(view, r.high + 1);
    const botY = yOfRow(view, r.low);
    // At most a third of the block is an edge, so a one-note block is moved, not just stretched.
    const grab = Math.min(4, (botY - topY) / 3);
    const mode: 'low' | 'high' | 'body' =
      y >= topY && y <= topY + grab ? 'high' : y <= botY && y >= botY - grab ? 'low' : y > topY && y < botY ? 'body' : nearerEnd({ low: r.low, high: r.high }, note);
    rangeDrag = { lane, mode, lastNote: note };
    onbegin?.(lane);
    try {
      strip?.setPointerCapture(e.pointerId);
    } catch {
      // No capture: the drag still works while the pointer stays over the strip.
    }
    if (mode !== 'body') onrange?.(lane, moveEnd({ low: r.low, high: r.high }, mode, note));
    announce(r);
  }

  function stripMove(e: PointerEvent): void {
    const { y } = local(e);
    if (!rangeDrag) {
      const lane = Math.min(lanes - 1, Math.max(0, Math.floor(local(e).x / laneW)));
      const r = ranges.find((b) => b.lane === lane);
      if (r && bars.length === 0) announce(r);
      return;
    }
    const r = ranges.find((b) => b.lane === rangeDrag?.lane);
    if (!r) return;
    const note = noteAtRow(view, y);
    if (rangeDrag.mode === 'body') {
      onrange?.(rangeDrag.lane, shiftRange({ low: r.low, high: r.high }, note - rangeDrag.lastNote));
      rangeDrag = { ...rangeDrag, lastNote: note };
    } else {
      onrange?.(rangeDrag.lane, moveEnd({ low: r.low, high: r.high }, rangeDrag.mode, note));
    }
    announce(r);
  }

  function stripUp(e: PointerEvent): void {
    if (strip?.hasPointerCapture(e.pointerId)) strip.releasePointerCapture(e.pointerId);
    if (rangeDrag) {
      rangeDrag = null;
      onend?.();
    }
  }

  const wheel = (e: WheelEvent): void => onview?.(first + (e.deltaY > 0 ? -12 : 12));
  const yTop = (n: number): number => yOfRow(view, n + 1);
</script>

<div class="roll" style:width="{width}px" style:height="{height}px" onwheel={wheel} role="presentation">
  <PianoKeys {first} {last} width={kbW} {height} orientation="vertical" {held} {marked} ondown={keyDown} onmove={(n) => keyMove(n)} onup={keyUp} />

  <svg
    bind:this={strip}
    class="strip"
    width={stripW}
    {height}
    viewBox="0 0 {stripW} {height}"
    role="group"
    aria-label="{context} ranges"
    onpointerdown={stripDown}
    onpointermove={stripMove}
    onpointerup={stripUp}
    onpointercancel={stripUp}
    onpointerleave={() => {
      if (!rangeDrag) clearHint();
    }}
  >
    <rect width={stripW} {height} fill="#232b2f" />
    {#each ranges as r (r.lane)}
      {@const y0 = yOfRow(view, Math.min(r.high, last) + 1)}
      {@const y1 = yOfRow(view, Math.max(r.low, first))}
      {#if r.high >= first && r.low <= last}
        <rect x={r.lane * laneW + 4} y={y0} width={laneW - 8} height={Math.max(2, y1 - y0)} rx="2" fill={r.color} fill-opacity={r.dim ? 0.22 : r.selected ? 0.95 : 0.7} stroke="#0e1417" stroke-opacity="0.7" />
        <rect x={r.lane * laneW + 4} y={y0} width={laneW - 8} height="3" rx="1" fill="#fff" fill-opacity="0.35" />
        <rect x={r.lane * laneW + 4} y={y1 - 3} width={laneW - 8} height="3" rx="1" fill="#000" fill-opacity="0.3" />
        {#if y1 - y0 > 26}
          <text x={r.lane * laneW + laneW / 2} y={y0 + 14} text-anchor="middle" class="blk">{noteName(r.high)}</text>
          <text x={r.lane * laneW + laneW / 2} y={y1 - 7} text-anchor="middle" class="blk">{noteName(r.low)}</text>
        {/if}
      {/if}
    {/each}
    {#each bars as b (b.note)}
      {#if b.note >= first && b.note <= last}
        <rect x="1" y={yTop(b.note) + 0.5} width={Math.max(2, b.value * (stripW - 2))} height={Math.max(2, rh - 1)} rx="1.5" fill={b.selected ? 'var(--accent)' : b.color} opacity={b.dim ? 0.12 : 1} />
      {/if}
    {/each}
  </svg>

  <!-- overview: the whole 0..127 range with the visible window outlined; click to move the window -->
  <svg
    class="over"
    width={OVER_W}
    {height}
    role="presentation"
    onpointerdown={(e) => {
      const r = (e.currentTarget as SVGElement).getBoundingClientRect();
      onview?.(Math.round((1 - (e.clientY - r.top) / (r.height || 1)) * 128 - (last - first + 1) / 2));
    }}
  >
    <rect width={OVER_W} {height} fill="#1c2427" />
    {#each ranges as r (r.lane)}
      <rect x="2" y={(1 - (r.high + 1) / 128) * height} width={OVER_W - 4} height={Math.max(1.5, ((r.high - r.low + 1) / 128) * height)} fill={r.color} fill-opacity="0.9" />
    {/each}
    {#each bars as b (b.note)}
      <rect x="1" y={(1 - (b.note + 1) / 128) * height} width={Math.max(1, b.value * (OVER_W - 2))} height="1.4" fill={b.color} opacity={b.dim ? 0.12 : 1} />
    {/each}
    <rect x="0.5" y={(1 - (last + 1) / 128) * height} width={OVER_W - 1} height={((last - first + 1) / 128) * height} fill="#fff" fill-opacity="0.12" stroke="#c7d1d6" />
  </svg>
</div>

<style>
  .roll {
    display: flex;
    background: #1c2427;
    border: 1px solid var(--edge);
    box-sizing: content-box;
  }
  .strip {
    display: block;
    touch-action: none;
    user-select: none;
    cursor: ns-resize;
  }
  .over {
    display: block;
    cursor: pointer;
  }
  .blk {
    font: 600 9px var(--font-caps);
    fill: #0e1417;
    pointer-events: none;
  }
</style>
