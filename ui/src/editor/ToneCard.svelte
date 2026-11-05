<script lang="ts">
  import { clearHint, showHint } from '../kit/hint.svelte';
  import { Meter } from '../kit';
  import type { BlockName } from '../model/catalog';
  import { blockTitle } from '../model/labels';
  import { useStore } from './context';
  import ParamControl from './ParamControl.svelte';
  import WavePicker from './WavePicker.svelte';

  interface Props {
    block: BlockName;
    color: string;
    selected: boolean;
    onselect: () => void;
  }

  let { block, color, selected, onselect }: Props = $props();
  const store = useStore();

  const on = $derived(store.get(block, 'tone_switch') === 1);
  const level = $derived(store.get(block, 'tone_level') / 127);
  const code = $derived(`${store.format(block, 'wave_group_type')} ${store.format(block, 'wave_number')}`);

  const announce = (): void =>
    showHint({ context: blockTitle(block), name: 'Tone on', value: on ? 'On: this Tone sounds' : 'Off: this Tone is silent', shortcut: 'Click to switch the Tone on or off' });
  function toggle(): void {
    store.set(block, 'tone_switch', on ? 0 : 1);
    announce();
  }
</script>

<div class="card" class:selected class:off={!on} style:--tone={color}>
  <div class="head">
    <button type="button" class="sw" role="switch" aria-checked={on} aria-label="{blockTitle(block)} on" onclick={toggle} onpointerenter={announce} onpointerleave={clearHint} onfocus={announce} onblur={clearHint}>
      <i></i>
    </button>
    <div class="title"><WavePicker {block} {color} onopen={onselect} /></div>
  </div>
  <button type="button" class="tag" onclick={onselect} aria-pressed={selected} aria-label="Edit {blockTitle(block)}">
    <b>{blockTitle(block)}</b><span>{code}</span>
  </button>
  <div class="dim">
    <div class="body">
      <ParamControl {block} id="tone_level" label="Level" size={38} {color} />
      <ParamControl {block} id="tone_pan" label="Pan" size={38} {color} />
      <Meter level={on ? level : 0} orientation="vertical" length={52} label="{blockTitle(block)} level" />
    </div>
  </div>
</div>

<style>
  .card {
    border: 1px solid var(--edge);
    border-top: 3px solid var(--tone);
    border-radius: var(--radius-lg);
    background: var(--body-deep);
    min-width: 150px;
  }
  .card.selected {
    background: color-mix(in srgb, var(--tone) 14%, var(--body-deep));
  }
  /* A Tone that is switched off is greyed out, including its colour bar. */
  .card.off {
    border-top-color: var(--edge-light);
  }
  .card.off .dim {
    opacity: 0.42;
    filter: grayscale(1);
  }
  .card.off .title,
  .card.off .tag {
    color: var(--text-faint);
  }
  .card.off .title :global(.trig:hover) {
    color: var(--text-dim);
  }
  .head {
    display: flex;
    align-items: center;
    gap: 4px;
    padding: 3px 8px 0 4px;
  }
  .sw {
    flex: none;
    display: grid;
    place-items: center;
    width: 22px;
    height: 22px;
    padding: 0;
    background: transparent;
    border: 0;
    border-radius: 50%;
    cursor: pointer;
  }
  .sw:focus-visible {
    outline: none;
  }
  .sw:focus-visible i {
    filter: brightness(1.5);
    transform: scale(1.2);
  }
  .sw i {
    width: 11px;
    height: 11px;
    border-radius: 50%;
    background: var(--tone);
    box-shadow:
      0 0 6px color-mix(in srgb, var(--tone) 65%, transparent),
      inset 0 1px 1px rgba(255, 255, 255, 0.35);
    transition: background 0.1s;
  }
  .sw:hover i {
    filter: brightness(1.15);
  }
  .card.off .sw i {
    background: transparent;
    box-shadow: inset 0 0 0 2px var(--edge-light);
  }
  .title {
    flex: 1;
    min-width: 0;
    color: var(--text);
  }
  /* Which Tone this is, and where its wave lives; clicking it selects the Tone for the tabs on the right. */
  .tag {
    display: flex;
    align-items: baseline;
    gap: 8px;
    margin: 0 8px 0 30px;
    padding: 0 4px;
    background: transparent;
    border: 0;
    border-radius: var(--radius);
    cursor: pointer;
    color: var(--text-dim);
    line-height: 1.2;
  }
  .tag b {
    font: 600 9.5px var(--font-caps);
    letter-spacing: 0.1em;
    text-transform: uppercase;
  }
  .tag span {
    font: 10px var(--font-mono);
    color: var(--text-faint);
  }
  .tag:hover {
    background: color-mix(in srgb, var(--text) 8%, transparent);
  }
  .tag:focus-visible {
    outline: none;
    background-image: var(--focus-fill);
  }
  .body {
    display: flex;
    align-items: flex-end;
    justify-content: space-between;
    gap: 8px;
    padding: 6px 12px 8px;
  }
</style>
