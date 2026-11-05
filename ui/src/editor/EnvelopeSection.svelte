<script lang="ts">
  import { EnvelopeGraph, type Stage } from '../kit';
  import type { BlockName } from '../model/catalog';
  import type { EnvelopeSpec } from '../model/layout';
  import { contextOf } from '../model/labels';
  import { useStore } from './context';
  import ParamControl from './ParamControl.svelte';

  interface Props {
    spec: EnvelopeSpec;
    block: BlockName;
    color: string;
  }

  let { spec, block, color }: Props = $props();
  const store = useStore();

  const stages = $derived<Stage[]>(
    spec.times.map((t, i) => ({
      time: store.get(block, t),
      level: i < spec.levels.length ? store.get(block, spec.levels[i] as string) : (spec.endLevel ?? 0),
    })),
  );
  const names = $derived(spec.times.map((_, i) => (i < spec.levels.length ? `Time ${i + 1} / Level ${i + 1}` : `Time ${i + 1} (release)`)));
  const locked = $derived(spec.times.map((_, i) => i >= spec.levels.length));
  const title = $derived(`${spec.title}`);

  function apply(next: Stage[]): void {
    next.forEach((s, i) => {
      const cur = stages[i];
      if (!cur) return;
      if (s.time !== cur.time) store.set(block, spec.times[i] as string, s.time);
      const levelId = spec.levels[i];
      if (levelId && s.level !== cur.level) store.set(block, levelId, s.level);
    });
  }
</script>

<div class="env">
  <EnvelopeGraph
    {stages}
    startLevel={spec.startLevel}
    stageNames={names}
    name={title}
    context={contextOf(block, spec.times[0] as string, store.kind)}
    {color}
    levelLocked={locked}
    width={236}
    height={118}
    onchange={apply}
    onbegin={() => store.begin(`${store.label(block, spec.times[0] as string).split(' · ')[0]} · ${title}`)}
    onend={() => store.end()}
  />
  <div class="extras">
    {#each spec.extras as id (id)}
      <ParamControl {block} {id} size={20} {color} />
    {/each}
  </div>
</div>

<style>
  .env {
    display: flex;
    flex-direction: column;
    gap: 6px;
  }
  .extras {
    display: flex;
    flex-wrap: wrap;
    gap: 4px 6px;
  }
</style>
