<script lang="ts">
  import { hitStage, layoutPoints, pathOf, stageFromPointer, type EnvelopeBox, type Stage } from './envelope';
  import { clearHint, showHint } from './hint.svelte';

  interface Props {
    stages: Stage[];
    /** Level the envelope starts from (fixed). */
    startLevel?: number;
    /** Names for the hint bar, one per stage, for example "Time 1 / Level 1". */
    stageNames?: readonly string[];
    name?: string;
    context?: string;
    color?: string;
    width?: number;
    height?: number;
    disabled?: boolean;
    /** Stages whose level is fixed (for example the level envelope's release ends at 0). */
    levelLocked?: readonly boolean[];
    onchange?: (stages: Stage[]) => void;
    onbegin?: () => void;
    onend?: () => void;
  }

  let {
    stages = $bindable(),
    startLevel = 0,
    stageNames = [],
    name = 'Envelope',
    context = '',
    color = 'var(--accent)',
    width = 220,
    height = 110,
    disabled = false,
    levelLocked = [],
    onchange,
    onbegin,
    onend,
  }: Props = $props();

  const box = $derived<EnvelopeBox>({ width, height, pad: 12, max: 127 });
  const pts = $derived(layoutPoints(stages, startLevel, box));
  const path = $derived(pathOf(pts));
  const area = $derived(`${path} L ${pts.at(-1)?.x ?? 0} ${height - box.pad} L ${box.pad} ${height - box.pad} Z`);

  let svg: SVGSVGElement | undefined = $state();
  let dragging = $state(-1);
  let hovered = $state(-1);

  function local(e: PointerEvent): { x: number; y: number } {
    const r = svg?.getBoundingClientRect();
    return { x: (e.clientX - (r?.left ?? 0)) * (width / (r?.width || width)), y: (e.clientY - (r?.top ?? 0)) * (height / (r?.height || height)) };
  }

  function publish(i: number): void {
    const s = stages[i];
    if (!s) return;
    const label = stageNames[i] ?? `Stage ${i + 1}`;
    showHint({ context, name: `${name} · ${label}`, value: `time ${s.time}, level ${s.level}`, shortcut: 'Drag · arrows nudge' });
  }

  function down(e: PointerEvent): void {
    if (disabled || dragging >= 0 || e.button !== 0) return;
    const p = local(e);
    const i = hitStage(pts, p.x, p.y, 10);
    if (i < 0) return;
    dragging = i;
    svg?.setPointerCapture(e.pointerId);
    onbegin?.();
  }

  function move(e: PointerEvent): void {
    if (disabled && dragging >= 0) {
      cancel();
      return;
    }
    const p = local(e);
    if (dragging < 0) {
      const h = hitStage(pts, p.x, p.y, 10);
      if (h !== hovered) {
        hovered = h;
        if (h >= 0) publish(h);
        else clearHint();
      }
      return;
    }
    const picked = stageFromPointer(dragging, stages.length, p.x, p.y, box);
    const cur = stages[dragging];
    const next = levelLocked[dragging] && cur ? { ...picked, level: cur.level } : picked;
    if (cur && cur.time === next.time && cur.level === next.level) return;
    stages = stages.map((s, i) => (i === dragging ? next : s));
    onchange?.(stages);
    publish(dragging);
  }

  function cancel(): void {
    if (dragging < 0) return;
    dragging = -1;
    onend?.();
  }

  function up(e: PointerEvent): void {
    if (dragging < 0) return;
    if (svg?.hasPointerCapture(e.pointerId)) svg.releasePointerCapture(e.pointerId);
    cancel();
  }

  // Removed mid-drag: still close the undo group.
  $effect(() => () => cancel());

  function nudge(i: number, e: KeyboardEvent): void {
    const s = stages[i];
    if (!s || disabled) return;
    const big = e.shiftKey ? 10 : 1;
    const clamp = (v: number): number => Math.min(127, Math.max(0, v));
    let next: Stage | null = null;
    if (e.key === 'ArrowUp') next = { ...s, level: clamp(s.level + big) };
    else if (e.key === 'ArrowDown') next = { ...s, level: clamp(s.level - big) };
    else if (e.key === 'ArrowRight') next = { ...s, time: clamp(s.time + big) };
    else if (e.key === 'ArrowLeft') next = { ...s, time: clamp(s.time - big) };
    if (!next) return;
    e.preventDefault();
    onbegin?.();
    stages = stages.map((x, k) => (k === i ? (next as Stage) : x));
    onchange?.(stages);
    onend?.();
    publish(i);
  }
</script>

<svg
  bind:this={svg}
  class="env"
  class:disabled
  style:--c={color}
  {width}
  {height}
  viewBox="0 0 {width} {height}"
  role="group"
  aria-label={name}
  onpointerdown={down}
  onpointermove={move}
  onpointerup={up}
  onpointercancel={up}
  onpointerleave={() => {
    if (dragging < 0) {
      hovered = -1;
      clearHint();
    }
  }}
>
  <rect x="0.5" y="0.5" width={width - 1} height={height - 1} rx="3" class="well" />
  {#each [0.25, 0.5, 0.75] as f (f)}
    <line x1={box.pad} x2={width - box.pad} y1={box.pad + (height - box.pad * 2) * f} y2={box.pad + (height - box.pad * 2) * f} class="grid" />
  {/each}
  <path d={area} class="area" />
  <path d={path} class="line" />
  {#each pts.slice(1) as p, i (i)}
    <circle
      cx={p.x}
      cy={p.y}
      r={dragging === i || hovered === i ? 6 : 4.5}
      class="pt"
      class:active={dragging === i}
      role="slider"
      tabindex={disabled ? -1 : 0}
      aria-label="{name} {stageNames[i] ?? `stage ${i + 1}`}"
      aria-valuetext="time {stages[i]?.time}, level {stages[i]?.level}"
      aria-valuenow={stages[i]?.level}
      aria-valuemin={0}
      aria-valuemax={127}
      onkeydown={(e) => nudge(i, e)}
      onfocus={() => publish(i)}
      onblur={clearHint}
    />
  {/each}
</svg>

<style>
  .env {
    touch-action: none;
    cursor: crosshair;
    user-select: none;
  }
  .well {
    fill: var(--well);
    stroke: var(--edge);
  }
  .grid {
    stroke: var(--well-deep);
    stroke-width: 1;
  }
  .area {
    fill: var(--c);
    opacity: 0.16;
  }
  .line {
    fill: none;
    stroke: var(--c);
    stroke-width: 1.6;
    stroke-linejoin: round;
  }
  .pt {
    fill: var(--cap-top);
    stroke: var(--edge);
    stroke-width: 1.2;
    cursor: grab;
    outline: none;
  }
  .pt:hover,
  .pt.active {
    fill: var(--c);
  }
  .pt:focus-visible {
    stroke: var(--accent);
    stroke-width: 2;
  }
  .disabled {
    opacity: 0.4;
    pointer-events: none;
  }
</style>
