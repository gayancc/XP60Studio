<script lang="ts">
  import { TONE_BLOCKS } from '../model/catalog';
  import { waveLabel } from '../model/factory';
  import { blockTitle } from '../model/labels';
  import { useStore } from './context';
  import TonePanel, { type ToneGroup } from './TonePanel.svelte';

  // One sound section for all four Tones at once, so the same parameters can be compared and set across Tones without switching between them. A Tone that is off is
  // dimmed but still editable. Clicking a Tone's heading selects it, as the Tone cards do.
  interface Props {
    part: ToneGroup;
    colors: readonly string[];
    selected: number;
    onselect: (index: number) => void;
  }
  let { part, colors, selected, onselect }: Props = $props();
  const store = useStore();

  const tones = $derived(
    TONE_BLOCKS.map((block, i) => ({
      block,
      i,
      color: colors[i] ?? 'var(--accent)',
      on: store.get(block, 'tone_switch') === 1,
      wave: waveLabel(store.get(block, 'wave_group_type'), store.get(block, 'wave_group_id'), store.get(block, 'wave_number') + 1),
    })),
  );
</script>

<div class="all">
  {#each tones as t (t.block)}
    <section class="col" class:off={!t.on} class:sel={selected === t.i} style:--tone={t.color} aria-label={blockTitle(t.block)}>
      <button type="button" class="head" onclick={() => onselect(t.i)} aria-pressed={selected === t.i}>
        <i></i><b>{blockTitle(t.block)}</b><span>{t.wave}</span>
      </button>
      <div class="body"><TonePanel block={t.block} color={t.color} {part} /></div>
    </section>
  {/each}
</div>

<style>
  /* Four Tones of one section are more than a small window shows at once; this view scrolls inside itself instead of cutting them off. */
  .all {
    flex: 1;
    min-height: 0;
    overflow-y: auto;
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
    gap: 8px;
    align-items: start;
    align-content: start;
    scrollbar-width: thin;
    scrollbar-color: var(--edge-light) transparent;
  }
  .col {
    min-width: 0;
    border-top: 3px solid var(--tone);
    border-radius: var(--radius-lg);
    background: var(--body-deep);
  }
  .col.sel {
    background: color-mix(in srgb, var(--tone) 10%, var(--body-deep));
  }
  .col.off {
    border-top-color: var(--edge-light);
  }
  .col.off .body {
    opacity: 0.5;
    filter: grayscale(0.8);
  }
  .head {
    display: flex;
    align-items: center;
    gap: 8px;
    width: 100%;
    padding: 4px 8px;
    background: transparent;
    border: 0;
    cursor: pointer;
    color: var(--text);
    text-align: left;
    font: inherit;
  }
  .head:hover {
    background: var(--caption);
  }
  .head:focus-visible {
    outline: none;
    background-image: var(--focus-fill);
  }
  .head i {
    width: 9px;
    height: 9px;
    border-radius: 50%;
    background: var(--tone);
    flex: none;
  }
  .col.off .head i {
    background: transparent;
    box-shadow: inset 0 0 0 2px var(--edge-light);
  }
  .head b {
    font: 600 12px var(--font-caps);
    letter-spacing: 0.04em;
  }
  .head span {
    margin-left: auto;
    min-width: 0;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
    color: var(--text-dim);
    font-size: 11px;
  }
  .body {
    padding: 0 6px 8px;
  }
  .body :global(.tone) {
    grid-template-columns: 1fr;
  }
</style>
