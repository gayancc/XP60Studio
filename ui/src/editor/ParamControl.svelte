<script lang="ts">
  import { Dropdown, Knob, Segmented, Stepper, ToggleLed } from '../kit';
  import type { BlockName } from '../model/catalog';
  import { paramInfo } from '../model/catalog';
  import { controlFor } from '../model/controlKind';
  import { controls } from '../model/controls.svelte';
  import { contextOf, shortLabel } from '../model/labels';
  import { isUnlisted, offered } from '../model/unlisted';
  import { useStore } from './context';

  interface Props {
    block: BlockName;
    id: string;
    /** Overrides the automatic short label. */
    label?: string;
    size?: 20 | 30 | 38 | 64;
    color?: string;
    width?: number;
    /** Why the parameter has no effect right now (from the manual); shown in the hint bar and the control is dimmed. */
    inert?: string;
  }

  let { block, id, label, size = 30, color = 'var(--accent)', width = 112, inert }: Props = $props();

  const store = useStore();
  const info = $derived(paramInfo(store.catalog, store.kind, block, id));
  const choice = $derived(info ? controlFor(info) : null);
  const value = $derived(store.get(block, id));
  const short = $derived(label ?? (info ? shortLabel(info.name) : id));
  const ctx = $derived(contextOf(block, id, store.kind));
  const options = $derived(info?.display.kind === 'options' ? info.display.labels : []);
  const base = $derived(info?.display.kind === 'options' ? info.display.min : 0);
  const history = $derived(store.label(block, id));
  /** The list entry that means "none" (0), so a double-click puts a stepped value back to it. */
  const neutral = $derived(options.findIndex((o) => /^[+\-\u2212]?0+(\.0+)?$/.test(o.trim())));

  const put = (v: number): void => store.set(block, id, v);
  /** The entries offered for a list: those the manual lists, plus one the patch already holds. */
  const shownIdx = $derived(offered(id, base, options.length, value));
  const shownOptions = $derived(shownIdx.map((i) => (isUnlisted(id, base + i) ? `${options[i]} (not in manual)` : (options[i] ?? ''))));
  const shownSelected = $derived(shownIdx.indexOf(value - base));
  const putShown = (i: number): void => put(base + (shownIdx[i] ?? 0));
  const hintName = $derived(inert && info ? `${info.name} (no effect: ${inert})` : (info?.name ?? ''));

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
  <span class="pc" class:inert={!!inert} data-param="{block}:{id}">
  {#if choice.type === 'knob'}
    <Knob
      {value}
      min={info.min}
      max={info.max}
      origin={choice.origin}
      {size}
      {color}
      label={short}
      name={hintName}
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
      <Segmented options={shownOptions} selected={shownSelected} name={hintName} context={ctx} onchange={putShown} />
    </div>
  {:else if choice.type === 'stepper'}
    <Stepper
      value={value - base}
      min={0}
      max={options.length - 1}
      defaultValue={neutral >= 0 ? neutral : undefined}
      label={short}
      name={info.name}
      context={ctx}
      format={(i) => options[i] ?? String(i)}
      onchange={(i) => put(base + i)}
      onbegin={() => store.begin(history)}
      onend={() => store.end()}
    />
  {:else}
    <Dropdown options={shownOptions} selected={shownSelected} label={short} name={hintName} context={ctx} {width} onchange={putShown} />
  {/if}
  </span>
{/if}

<style>
  .pc {
    display: inline-flex;
  }
  .pc.inert {
    opacity: 0.4;
  }
  .field {
    display: inline-flex;
    flex-direction: column;
    gap: 2px;
  }
</style>
