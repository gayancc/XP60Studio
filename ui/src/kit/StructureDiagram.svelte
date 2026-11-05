<script lang="ts">
  import { structureOf, type StructureNode } from '../model/structure';

  // The wiring of one Structure type, drawn from the data in model/structure.ts. Each Tone is a lane with its name on the left; along it run the wave generator (WG),
  // filter (TVF) and amplifier (TVA), and the two lanes are joined by a junction, a booster (B) or a ring modulator (R), the way the manual describes the ten types.
  interface Props {
    type: number;
    /** Colour of the boxes that belong to a Tone. */
    color?: string;
    /** Drawn width in pixels; the picture keeps its proportions. */
    width?: number;
    /** The two lane names, such as "TONE 1" and "TONE 2", or "TONE 1 (3)" and "TONE 2 (4)" where the picture stands for both pairs. */
    captions?: readonly [string, string];
  }
  let { type, color = 'var(--accent)', width = 260, captions = ['TONE 1', 'TONE 2'] }: Props = $props();

  // The wiring is laid out in a 200 by 64 space (model/structure.ts); the lane names take the first LEFT units and every lane is a rounded outline.
  const LEFT = 62;
  const W = LEFT + 200 + 6;
  const H = 80;
  const DY = 8;

  const s = $derived(structureOf(type));
  const byId = $derived(new Map(s.nodes.map((n) => [n.id, n])));
  const LABEL: Record<string, string> = { wg: 'WG', tvf: 'TVF', tva: 'TVA', booster: 'B', ring: 'R' };
  const X = (n: StructureNode): number => n.x + LEFT;
  const Y = (n: StructureNode): number => n.y + DY;

  /** Half width and height of a box. */
  const half = (n: StructureNode): [number, number] => (n.kind === 'wg' || n.kind === 'tvf' || n.kind === 'tva' ? [14, 7.5] : n.kind === 'sum' ? [2.5, 2.5] : [8, 8]);

  function path(a: StructureNode, b: StructureNode): string {
    const [aw] = half(a);
    const [bw] = half(b);
    const forward = b.x >= a.x;
    const x1 = X(a) + (forward ? aw : -aw);
    const x2 = X(b) + (forward ? -bw : bw);
    const mid = (x1 + x2) / 2;
    // Roland draws a level run, a slanted run to the new height, then a level run again; the same here.
    return a.y === b.y ? `M${x1},${Y(a)} L${x2},${Y(b)}` : `M${x1},${Y(a)} L${mid - 5},${Y(a)} L${mid + 5},${Y(b)} L${x2},${Y(b)}`;
  }

  const LANES = [DY + 14 - 15, DY + 50 - 15] as const;
</script>

<svg viewBox="0 0 {W} {H}" {width} height={(width * H) / W} role="img" aria-label={`Structure type ${type}: ${captions[0]} and ${captions[1]}`} style:--c={color}>
  {#each LANES as top, i (top)}
    <rect class="lane" x="1" y={top} width={W - 2} height="30" rx="8" />
    <line class="split" x1={LEFT - 4} y1={top + 1} x2={LEFT - 4} y2={top + 29} />
    <text class="cap" x="8" y={top + 19}>{captions[i]}</text>
  {/each}
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
      <circle class="sum" cx={X(n)} cy={Y(n)} r="2.8" />
    {:else}
      <rect class="box {n.kind}" x={X(n) - w} y={Y(n) - h} width={w * 2} height={h * 2} rx={n.kind === 'booster' || n.kind === 'ring' ? 2 : 6} />
      <text class="t {n.kind}" x={X(n)} y={Y(n) + 3.4} text-anchor="middle">{LABEL[n.kind]}</text>
    {/if}
  {/each}
</svg>

<style>
  svg {
    display: block;
    overflow: visible;
  }
  .lane {
    fill: var(--body-deep);
    stroke: var(--edge-light);
    stroke-width: 1;
  }
  .split {
    stroke: var(--edge-light);
    stroke-width: 1;
  }
  .cap {
    font: 600 7.5px var(--font-caps);
    letter-spacing: 0.06em;
    fill: var(--text-dim);
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
    font: 600 7.5px var(--font-caps);
    fill: var(--text);
    pointer-events: none;
    letter-spacing: 0.02em;
  }
</style>
