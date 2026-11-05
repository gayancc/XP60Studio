<script lang="ts">
  import { GroupFrame } from '../kit';
  import type { BlockName } from '../model/catalog';
  import { ENVELOPES, envelopeParamIds, toneParams, type EnvelopeSpec, type ToneSection } from '../model/layout';
  import { useStore } from './context';
  import EnvelopeSection from './EnvelopeSection.svelte';
  import ParamControl from './ParamControl.svelte';

  /** One tab of a Tone's sound design. */
  export type TonePart = 'wave' | 'pitch' | 'tvf' | 'tva' | 'lfo' | 'output';
  /** The Tone page shows every part of one Tone at once. */
  const ALL_PARTS: readonly TonePart[] = ['wave', 'pitch', 'tvf', 'tva', 'lfo', 'output'];

  interface Props {
    block: BlockName;
    color: string;
    part: TonePart | 'all';
  }

  let { block, color, part }: Props = $props();
  const store = useStore();

  const specOf = (key: EnvelopeSpec['key']): EnvelopeSpec => ENVELOPES.find((e) => e.key === key) as EnvelopeSpec;

  /** The section's parameters that are not part of its envelope graph. */
  function plain(section: ToneSection, env?: EnvelopeSpec): string[] {
    const drawn = new Set(env ? envelopeParamIds(env) : []);
    return toneParams(store.catalog.tone, section).filter((id) => !drawn.has(id));
  }

  const waveIds = $derived(plain('wave'));
  const outputIds = $derived(plain('output'));
  const lfo1 = $derived(plain('lfo1'));
  const lfo2 = $derived(plain('lfo2'));
  const pitch = $derived(plain('pitch', specOf('pitch')));
  const tvf = $derived(plain('tvf', specOf('tvf')));
  const tva = $derived(plain('tva', specOf('tva')));
</script>

{#snippet section(p: TonePart)}
  {#if p === 'wave'}
    <GroupFrame title="Wave" {color}>
      {#each waveIds as id (id)}<ParamControl {block} {id} {color} />{/each}
    </GroupFrame>
  {:else if p === 'pitch'}
    <GroupFrame title="Pitch" {color}>
      {#each pitch as id (id)}<ParamControl {block} {id} {color} />{/each}
      <EnvelopeSection spec={specOf('pitch')} {block} {color} />
    </GroupFrame>
  {:else if p === 'tvf'}
    <GroupFrame title="TVF (filter)" {color}>
      {#each tvf as id (id)}<ParamControl {block} {id} {color} />{/each}
      <EnvelopeSection spec={specOf('tvf')} {block} {color} />
    </GroupFrame>
  {:else if p === 'tva'}
    <GroupFrame title="TVA (level and pan)" {color}>
      {#each tva as id (id)}<ParamControl {block} {id} {color} />{/each}
      <EnvelopeSection spec={specOf('tva')} {block} {color} />
    </GroupFrame>
  {:else if p === 'lfo'}
    <GroupFrame title="LFO 1" {color}>
      {#each lfo1 as id (id)}<ParamControl {block} {id} {color} />{/each}
    </GroupFrame>
    <GroupFrame title="LFO 2" {color}>
      {#each lfo2 as id (id)}<ParamControl {block} {id} {color} />{/each}
    </GroupFrame>
  {:else}
    <GroupFrame title="Output" {color}>
      {#each outputIds as id (id)}<ParamControl {block} {id} {color} />{/each}
    </GroupFrame>
  {/if}
{/snippet}

<div class="tone" style:--tone={color}>
  {#if part === 'all'}
    {#each ALL_PARTS as p (p)}{@render section(p)}{/each}
  {:else}
    {@render section(part)}
  {/if}
</div>

<style>
  .tone {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
    gap: 10px;
    align-items: start;
    align-content: start;
    /* the whole-Tone page is taller than a small window: it scrolls inside itself */
    flex: 1;
    min-height: 0;
    overflow-y: auto;
    scrollbar-width: thin;
    scrollbar-color: var(--edge-light) transparent;
  }
</style>
