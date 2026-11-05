<script lang="ts">
  import type { Routing, NodeId, Edge } from '../model/routing';
  import { clearHint, showHint } from './hint.svelte';

  // The signal path of a Patch's effects, drawn from its settings. Instead of one line per Tone per destination, the four Tones share a single bus on the left; one
  // lane leaves it for each destination (EFX, chorus, reverb, and the two outputs along the top and bottom), carrying small coloured dots for the Tones that really feed
  // it. The stages send on to each other down the middle and to the outputs through a shared bus on the right. A lane or stage nothing feeds is not drawn or is dimmed.
  interface Props {
    routing: Routing;
    /** The text under each effect stage's name (for example the EFX type). */
    labels: { efx: string; chorus: string; reverb: string };
    onjump?: (node: 'efx' | 'chorus' | 'reverb') => void;
  }
  let { routing, labels, onjump }: Props = $props();

  const W = 640;
  const H = 236;
  const TONES: readonly NodeId[] = ['tone1', 'tone2', 'tone3', 'tone4'];
  const TONE_Y = [10, 60, 110, 160]; // top of each Tone chip, 36 high
  const COLOR: Record<string, string> = { tone1: 'var(--tone-1)', tone2: 'var(--tone-2)', tone3: 'var(--tone-3)', tone4: 'var(--tone-4)' };
  const STAGES = [
    { id: 'efx', name: 'EFX', y: 30 },
    { id: 'chorus', name: 'CHORUS', y: 100 },
    { id: 'reverb', name: 'REVERB', y: 170 },
  ] as const;
  const STAGE_X = 250;
  const STAGE_W = 140;
  const STAGE_H = 44;
  const RAIL = 112;
  const LANE: Record<string, number> = { mix: 14, efx: 52, chorus: 122, reverb: 192, dir: 226 };
  const NAMES: Record<NodeId, string> = { tone1: 'TONE 1', tone2: 'TONE 2', tone3: 'TONE 3', tone4: 'TONE 4', efx: 'EFX', chorus: 'CHORUS', reverb: 'REVERB', mix: 'MIX OUTPUT', dir: 'DIRECT OUT' };

  const has = (from: NodeId, to: NodeId): Edge | undefined => routing.edges.find((e) => e.from === from && e.to === to);
  /** The Tones that feed a destination. */
  const feeders = (to: NodeId): NodeId[] => TONES.filter((t) => has(t, to));
  const lanes = $derived((['mix', 'efx', 'chorus', 'reverb', 'dir'] as const).filter((d) => feeders(d).length > 0));
  const feeding = $derived(TONES.filter((t) => routing.edges.some((e) => e.from === t)));
  const stub = (t: NodeId): number => (TONE_Y[TONES.indexOf(t)] ?? 0) + 18;
  /** The shared bus spans from the highest to the lowest thing joined to it. */
  const rail = $derived.by(() => {
    const ys = [...lanes.map((d) => LANE[d] ?? 0), ...feeding.map(stub)];
    return ys.length ? { y1: Math.min(...ys), y2: Math.max(...ys) } : null;
  });
  const laneColor = (d: NodeId): string => {
    const f = feeders(d);
    return f.length === 1 ? (COLOR[f[0] as string] ?? 'var(--accent)') : 'var(--text-dim)';
  };
  const lanePath = (d: string): string => {
    const y = LANE[d] ?? 0;
    if (d === 'mix') return `M${RAIL} ${y} H582 V30`;
    if (d === 'dir') return `M${RAIL} ${y} H582 V214`;
    return `M${RAIL} ${y} H${STAGE_X}`;
  };

  const stageEdge = (from: NodeId, to: NodeId): boolean => !!has(from, to);
  const toMix = $derived(stageEdge('efx', 'mix') || stageEdge('chorus', 'mix') || stageEdge('reverb', 'mix'));
  const active = (n: NodeId): boolean => routing.used.has(n);

  function explain(n: NodeId): void {
    const why = routing.ignoredTones.has(n)
      ? 'Ignored: with Structure Type 2 to 10 this Tone is combined into the next one, and its output settings are not used'
      : n === 'efx' || n === 'chorus' || n === 'reverb'
        ? active(n)
          ? 'In use: something is sending sound to it. Click to open the Effects tab'
          : 'Not in use: nothing sends sound to it with the current settings. Click to open the Effects tab'
        : '';
    showHint({ context: 'Routing', name: NAMES[n], value: why });
  }
  function explainLane(d: NodeId): void {
    const names = feeders(d).map((t) => NAMES[t]).join(', ');
    showHint({ context: 'Routing', name: `To ${NAMES[d]}`, value: `Fed by ${names}` });
  }
</script>

<svg viewBox="0 0 {W} {H}" width={W} height={H} role="img" aria-label="Effects routing of this patch, drawn from its settings">
  <defs>
    <marker id="rt-arrow" viewBox="0 0 8 8" refX="7" refY="4" markerWidth="7" markerHeight="7" orient="auto"><path d="M0 0 L8 4 L0 8 z" class="head" /></marker>
  </defs>

  <!-- the shared Tone bus -->
  {#if rail}<line x1={RAIL} x2={RAIL} y1={rail.y1} y2={rail.y2} class="bus" />{/if}
  {#each feeding as t (t)}<path d="M90 {stub(t)} H{RAIL}" class="stub" style:stroke={COLOR[t]} />{/each}

  <!-- one lane per destination, with a dot for each Tone that feeds it -->
  {#each lanes as d (d)}
    <g class="lane" role="img" aria-label="Lane to {NAMES[d]}" onpointerenter={() => explainLane(d)} onpointerleave={clearHint}>
      <path d={lanePath(d)} class="edge" style:stroke={laneColor(d)} marker-end="url(#rt-arrow)" />
      {#each feeders(d) as t, k (t)}<circle cx={RAIL + 16 + k * 13} cy={LANE[d]} r="4.5" style:fill={COLOR[t]} class="chip" />{/each}
    </g>
  {/each}

  <!-- the stages send on to each other and to the outputs -->
  {#if stageEdge('efx', 'chorus')}<path d="M320 74 V100" class="edge acc" marker-end="url(#rt-arrow)" />{/if}
  {#if stageEdge('chorus', 'reverb')}<path d="M320 144 V170" class="edge acc" marker-end="url(#rt-arrow)" />{/if}
  {#if stageEdge('efx', 'reverb')}<path d="M390 62 H414 V200 H390" class="edge acc" marker-end="url(#rt-arrow)" />{/if}
  {#if stageEdge('efx', 'mix')}<path d="M390 52 H450" class="edge acc" />{/if}
  {#if stageEdge('chorus', 'mix')}<path d="M390 122 H450 V52" class="edge acc" />{/if}
  {#if stageEdge('reverb', 'mix')}<path d="M390 192 H450 V52" class="edge acc" />{/if}
  {#if toMix}<path d="M450 52 H528" class="edge acc" marker-end="url(#rt-arrow)" />{/if}
  {#if stageEdge('efx', 'dir')}<path d="M390 42 H474 V192 H528" class="edge acc" marker-end="url(#rt-arrow)" />{/if}

  <!-- nodes -->
  {#each TONES as t, i (t)}
    <g class="node" class:dim={routing.ignoredTones.has(t)} role="img" aria-label={NAMES[t]} onpointerenter={() => explain(t)} onpointerleave={clearHint}>
      <rect x="6" y={TONE_Y[i]} width="84" height="36" rx="3" class="box" />
      <rect x="6" y={TONE_Y[i]} width="4" height="36" rx="1" style:fill={COLOR[t]} />
      <text x="50" y={(TONE_Y[i] ?? 0) + 22} text-anchor="middle" class="name">{NAMES[t]}</text>
    </g>
  {/each}
  {#each STAGES as s (s.id)}
    <g
      class="node go"
      class:dim={!active(s.id)}
      role="button"
      tabindex="0"
      aria-label="{s.name}: open the Effects tab"
      onpointerenter={() => explain(s.id)}
      onpointerleave={clearHint}
      onclick={() => onjump?.(s.id)}
      onkeydown={(e) => (e.key === 'Enter' || e.key === ' ') && onjump?.(s.id)}
    >
      <rect x={STAGE_X} y={s.y} width={STAGE_W} height={STAGE_H} rx="3" class="box" />
      <text x={STAGE_X + STAGE_W / 2} y={s.y + 19} text-anchor="middle" class="name">{s.name}</text>
      <text x={STAGE_X + STAGE_W / 2} y={s.y + 34} text-anchor="middle" class="sub">{labels[s.id]}</text>
    </g>
  {/each}
  <g class="node" class:dim={!active('mix')} role="img" aria-label="MIX OUTPUT">
    <rect x="532" y="30" width="100" height="44" rx="3" class="box" />
    <text x="582" y="57" text-anchor="middle" class="name">MIX OUTPUT</text>
  </g>
  <g class="node" class:dim={!active('dir')} role="img" aria-label="DIRECT OUT">
    <rect x="532" y="170" width="100" height="44" rx="3" class="box" />
    <text x="582" y="197" text-anchor="middle" class="name">DIRECT OUT</text>
  </g>
</svg>

<style>
  svg {
    display: block;
    max-width: 100%;
    height: auto;
  }
  .box {
    fill: var(--caption);
  }
  .bus {
    stroke: var(--text-dim);
    stroke-width: 2;
    opacity: 0.6;
  }
  .stub {
    fill: none;
    stroke-width: 2;
  }
  .edge {
    fill: none;
    stroke-width: 2;
    stroke-linecap: round;
    stroke-linejoin: round;
  }
  .acc {
    stroke: var(--accent);
  }
  .head {
    fill: var(--text-dim);
  }
  .chip {
    stroke: var(--body);
    stroke-width: 1.5;
  }
  .name {
    font: 600 11px var(--font-caps);
    letter-spacing: 0.08em;
    fill: var(--text);
  }
  .sub {
    font: 10px var(--font-mono);
    fill: var(--text-dim);
  }
  .dim {
    opacity: 0.42;
  }
  .go {
    cursor: pointer;
  }
  .go:hover .box,
  .go:focus-visible .box {
    fill: var(--well);
  }
  .node:focus-visible {
    outline: none;
  }
</style>
