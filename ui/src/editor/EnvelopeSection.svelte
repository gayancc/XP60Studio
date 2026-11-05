<script lang="ts">
  import { EnvelopeGraph, type Stage } from '../kit';
  import { paramInfo, type BlockName } from '../model/catalog';
  import { controlFor } from '../model/controlKind';
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

  // The extra controls fall in two groups, each aligned on its own: the small knobs in one row, and the drop-downs (velocity times, time key follow) in a grid of equal columns.
  const isChoice = (id: string): boolean => {
    const info = paramInfo(store.catalog, store.kind, block, id);
    return info ? controlFor(info).type !== 'knob' : false;
  };
  const knobs = $derived(spec.extras.filter((id) => !isChoice(id)));
  const choices = $derived(spec.extras.filter((id) => isChoice(id)));

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
    {#if knobs.length > 0}
      <div class="knobs">
        {#each knobs as id (id)}<ParamControl {block} {id} size={20} {color} />{/each}
      </div>
    {/if}
    {#if choices.length > 0}
      <div class="choices">
        {#each choices as id (id)}<ParamControl {block} {id} size={20} {color} width={112} />{/each}
      </div>
    {/if}
  </div>
</div>

<style>
  /* The graph and its extra controls sit side by side when the frame is wide, and one under the other when it is narrow, so a wide frame has no blank area beside the graph. */
  .env {
    flex: 1 1 100%;
    display: flex;
    flex-direction: row;
    flex-wrap: wrap;
    align-items: flex-start;
    gap: 6px 14px;
  }
  .extras {
    flex: 1 1 200px;
    min-width: 0;
    display: flex;
    flex-direction: column;
    gap: 8px;
  }
  .knobs {
    display: flex;
    align-items: flex-start;
    gap: 6px 22px;
  }
  .choices {
    display: grid;
    grid-template-columns: repeat(auto-fill, 112px);
    gap: 6px 10px;
    align-items: start;
  }
</style>
