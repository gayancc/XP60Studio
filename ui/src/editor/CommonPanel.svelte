<script lang="ts">
  import { GroupFrame } from '../kit';
  import { commonParams, type CommonSection } from '../model/layout';
  import { commonSectionTitle } from '../model/labels';
  import { useStore } from './context';
  import ParamControl from './ParamControl.svelte';
  import StructureControl from './StructureControl.svelte';
  import EfxParam from './EfxParam.svelte';
  import EfxTypePick from './EfxTypePick.svelte';

  // The two Structure types are drawn as wiring diagrams with their booster beside them, so the boosters are not listed again on their own.
  const STRUCTURE_PAIRS: Record<string, { title: string; booster: string; tones: readonly [string, string] }> = {
    structure_type_1_2: { title: 'Tones 1 and 2', booster: 'booster_1_2', tones: ['tone1', 'tone2'] },
    structure_type_3_4: { title: 'Tones 3 and 4', booster: 'booster_3_4', tones: ['tone3', 'tone4'] },
  };
  const STRUCTURE_BOOSTERS = new Set(Object.values(STRUCTURE_PAIRS).map((p) => p.booster));

  interface Props {
    sections: readonly CommonSection[];
    /** Show the name field above the groups. */
    showName?: boolean;
  }

  let { sections, showName = false }: Props = $props();
  const store = useStore();

  const idsOf = (s: CommonSection): string[] => commonParams(store.catalog.common, s);

  let draft = $state<string | null>(null);
  const shown = $derived(draft ?? store.name.trimEnd());

  function commit(): void {
    if (draft !== null) store.setName(draft);
    draft = null;
  }
</script>

<div class="common">
  {#if showName}
  <label class="name">
    <span class="caps">Patch name</span>
    <input
      type="text"
      maxlength="12"
      spellcheck="false"
      value={shown}
      oninput={(e) => (draft = e.currentTarget.value)}
      onblur={commit}
      onkeydown={(e) => {
        if (e.key === 'Enter') e.currentTarget.blur();
        if (e.key === 'Escape') {
          draft = null;
          e.currentTarget.blur();
        }
      }}
      aria-label="Patch name"
    />
  </label>
  {/if}

  {#each sections as s (s)}
    <GroupFrame title={commonSectionTitle(s)}>
      {#each idsOf(s) as id (id)}
        {#if s === 'structure' && STRUCTURE_PAIRS[id]}
          <StructureControl title={STRUCTURE_PAIRS[id].title} typeId={id} boosterId={STRUCTURE_PAIRS[id].booster} tones={STRUCTURE_PAIRS[id].tones} />
        {:else if id === 'efx_type'}
          <EfxTypePick block="common" {id} />
        {:else if id.startsWith('efx_parameter_')}
          <EfxParam block="common" {id} />
        {:else if !(s === 'structure' && STRUCTURE_BOOSTERS.has(id))}
          <ParamControl block="common" {id} />
        {/if}
      {/each}
    </GroupFrame>
  {/each}
</div>

<style>
  .common {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
    gap: 10px;
    align-items: start;
  }
  .name {
    grid-column: 1 / -1;
  }
  .name {
    display: flex;
    flex-direction: column;
    gap: 2px;
    width: 160px;
  }
  input {
    background: var(--track);
    color: var(--text);
    border: 1px solid var(--edge);
    border-radius: var(--radius);
    padding: 4px 6px;
    font-family: var(--font-mono);
    font-size: 13px;
    letter-spacing: 0.05em;
  }
  input:focus {
    outline: none;
    border-color: var(--accent);
  }
</style>
