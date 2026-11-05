<script lang="ts">
  import { Dropdown, Knob, Segmented, ToggleLed } from '../kit';
  import type { BlockName } from '../model/catalog';
  import { paramInfo } from '../model/catalog';
  import { controlFor } from '../model/controlKind';
  import { controls } from '../model/controls.svelte';
  import { contextOf, shortLabel } from '../model/labels';
  import { useStore } from './context';

  interface Props {
    block: BlockName;
    id: string;
    /** Overrides the automatic short label. */
    label?: string;
    size?: 20 | 30 | 38 | 64;
    color?: string;
    width?: number;
  }

  let { block, id, label, size = 30, color = 'var(--accent)', width = 112 }: Props = $props();

  const store = useStore();
  const info = $derived(paramInfo(store.catalog, store.kind, block, id));
  const choice = $derived(info ? controlFor(info) : null);
  const value = $derived(store.get(block, id));
  const short = $derived(label ?? (info ? shortLabel(info.name) : id));
  const ctx = $derived(contextOf(block, id, store.kind));
  const options = $derived(info?.display.kind === 'options' ? info.display.labels : []);
  const base = $derived(info?.display.kind === 'options' ? info.display.min : 0);
  const history = $derived(store.label(block, id));

  const put = (v: number): void => store.set(block, id, v);

  /** The right-click "Link to MIDI CC" for this parameter, offered only where there is an instrument to listen on. */
  const midi = $derived.by(() => {
    if (!controls.available || !info) return undefined;
    const target = { kind: store.kind, block, id };
    return {
      linked: controls.linked(target),
      learn: () => controls.learn({ ...target, name: info.name }),
      unlink: () => controls.unlink(target),
    };
  });
</script>

{#if info && choice}
  <span class="pc" data-param="{block}:{id}">
  {#if choice.type === 'knob'}
    <Knob
      {value}
      min={info.min}
      max={info.max}
      origin={choice.origin}
      {size}
      {color}
      label={short}
      name={info.name}
      context={ctx}
      defaultValue={choice.origin === null ? info.min : Math.round(info.min + choice.origin * (info.max - info.min))}
      format={(v) => store.formatValue(block, id, v)}
      onchange={put}
      onbegin={() => store.begin(history)}
      onend={() => store.end()}
      {midi}
    />
  {:else if choice.type === 'toggle'}
    <ToggleLed checked={value === 1} label={short} name={info.name} context={ctx} {color} onchange={(on) => put(on ? 1 : 0)} />
  {:else if choice.type === 'segmented'}
    <div class="field">
      <span class="caps">{short}</span>
      <Segmented {options} selected={value - base} name={info.name} context={ctx} onchange={(i) => put(base + i)} />
    </div>
  {:else}
    <Dropdown {options} selected={value - base} label={short} name={info.name} context={ctx} {width} onchange={(i) => put(base + i)} />
  {/if}
  </span>
{/if}

<style>
  .pc {
    display: inline-flex;
  }
  .field {
    display: inline-flex;
    flex-direction: column;
    gap: 2px;
  }
</style>
