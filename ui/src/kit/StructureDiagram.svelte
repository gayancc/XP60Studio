<script lang="ts">
  import { structureOf, type StructureNode } from '../model/structure';

  // The wiring of one Structure type, drawn from the data in model/structure.ts: two chains (Tone A above, Tone B below) of wave generator, filter and amplifier,
  // joined by a junction, a booster (B) or a ring modulator (R).
  interface Props {
    type: number;
    /** Colour of the Tones' boxes; the two rows are told apart by position and by the labels. */
    color?: string;
    width?: number;
    /** Draw the box labels (off for the smallest sizes). */
    labels?: boolean;
  }
  let { type, color = 'var(--accent)', width = 200, labels = true }: Props = $props();

  const s = $derived(structureOf(type));
  const byId = $derived(new Map(s.nodes.map((n) => [n.id, n])));
  const LABEL: Record<string, string> = { wg: 'WG', tvf: 'TVF', tva: 'TVA', booster: 'B', ring: 'R' };

  /** Half width and height of a box. */
  const half = (n: StructureNode): [number, number] => (n.kind === 'wg' || n.kind === 'tvf' || n.kind === 'tva' ? [13, 6.5] : n.kind === 'sum' ? [2.5, 2.5] : [7, 7]);

  function path(a: StructureNode, b: StructureNode): string {
    const [aw] = half(a);
    const [bw] = half(b);
    const forward = b.x >= a.x;
    const x1 = a.x + (forward ? aw : -aw);
    const x2 = b.x + (forward ? -bw : bw);
    const mid = (x1 + x2) / 2;
    // Roland draws a level run, a slanted run to the new height, then a level run again; the same here.
    return a.y === b.y ? `M${x1},${a.y} L${x2},${b.y}` : `M${x1},${a.y} L${mid - 5},${a.y} L${mid + 5},${b.y} L${x2},${b.y}`;
  }
</script>

<svg viewBox="0 0 200 64" {width} height={(width * 64) / 200} role="img" aria-label={`Structure type ${type}`} style:--c={color}>
  <g class="wires">
    {#each s.edges as [from, to] (from + to)}
      {@const a = byId.get(from)}
      {@const b = byId.get(to)}
      {#if a && b}<path d={path(a, b)} />{/if}
    {/each}
  </g>
  {#each s.nodes as n (n.id)}
    {@const [w, h] = half(n)}
    {#if n.kind === 'sum'}
      <circle class="sum" cx={n.x} cy={n.y} r="2.6" />
    {:else}
      <rect class="box {n.kind}" class:b={n.tone === 'B'} x={n.x - w} y={n.y - h} width={w * 2} height={h * 2} rx={n.kind === 'booster' || n.kind === 'ring' ? 2 : 6} />
      {#if labels}<text class="t {n.kind}" x={n.x} y={n.y + 3.2} text-anchor="middle">{LABEL[n.kind]}</text>{/if}
    {/if}
  {/each}
</svg>

<style>
  svg {
    display: block;
    overflow: visible;
  }
  .wires path {
    fill: none;
    stroke: var(--text-dim);
    stroke-width: 1.3;
    stroke-linejoin: round;
  }
  .box {
    fill: var(--track);
    stroke: color-mix(in srgb, var(--c) 70%, var(--text-dim));
    stroke-width: 1.2;
  }
  .box.booster,
  .box.ring {
    fill: color-mix(in srgb, var(--c) 28%, var(--track));
    stroke: var(--c);
  }
  .sum {
    fill: var(--text-dim);
  }
  .t {
    font: 600 7px var(--font-caps);
    fill: var(--text);
    pointer-events: none;
    letter-spacing: 0.02em;
  }
  .t.booster,
  .t.ring {
    fill: var(--text);
  }
</style>
