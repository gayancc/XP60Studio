<script lang="ts">
  import { GroupFrame } from '../kit';
  import type { BlockName } from '../model/catalog';
  import { ENVELOPES, envelopeParamIds, toneParams, type EnvelopeSpec, type ToneSection } from '../model/layout';
  import { useStore } from './context';
  import LfoSection from './LfoSection.svelte';
  import EnvelopeSection from './EnvelopeSection.svelte';
  import ControllerMatrix from './ControllerMatrix.svelte';
  import ParamControl from './ParamControl.svelte';

  /** One group of a Tone's sound design. */
  export type TonePart = 'wave' | 'pitch' | 'tvf' | 'tva' | 'lfo' | 'output';
  /**
   * What a tab shows, in signal-flow order: the sound source (wave, then its pitch), the two shaping stages side by side (filter and amplifier), and what moves
   * the sound (both LFOs, then the controller routing). Each is sized to fit the smallest window without scrolling.
   */
  export type ToneGroup = 'wavepitch' | 'filteramp' | 'lfomod' | TonePart;

  interface Props {
    block: BlockName;
    color: string;
    part: ToneGroup;
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
    <LfoSection {block} lfo={1} {color} />
    <LfoSection {block} lfo={2} {color} />
  {:else}
    <GroupFrame title="Output" {color}>
      {#each outputIds as id (id)}<ParamControl {block} {id} {color} />{/each}
    </GroupFrame>
  {/if}
{/snippet}

<div class="tone" class:stack={part === 'wavepitch' || part === 'lfomod'} style:--tone={color}>
  {#if part === 'wavepitch'}
    {@render section('wave')}
    {@render section('pitch')}
  {:else if part === 'filteramp'}
    {@render section('tvf')}
    {@render section('tva')}
  {:else if part === 'lfomod'}
    <div class="pair">{@render section('lfo')}</div>
    <ControllerMatrix {block} {color} />
  {:else}
    {@render section(part)}
  {/if}
</div>

<style>
  .tone.stack {
    display: flex;
    flex-direction: column;
  }
  /* The stack aligns its children to the start, which shrank this row to its content and stacked the two LFOs; the row takes the full width so they sit side by side. */
  .pair {
    align-self: stretch;
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
    gap: 10px;
    align-items: start;
  }
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
