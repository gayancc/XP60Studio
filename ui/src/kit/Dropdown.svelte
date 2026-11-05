<script lang="ts">
  import { clearHint, showHint } from './hint.svelte';

  interface Props {
    options: readonly string[];
    selected: number;
    label?: string;
    name?: string;
    context?: string;
    disabled?: boolean;
    width?: number;
    onchange?: (index: number) => void;
  }

  let {
    options,
    selected = $bindable(),
    label = '',
    name = label,
    context = '',
    disabled = false,
    width = 130,
    onchange,
  }: Props = $props();

  const publish = (): void => showHint({ context, name: name || label, value: options[selected] ?? '' });

  function change(e: Event & { currentTarget: HTMLSelectElement }): void {
    selected = e.currentTarget.selectedIndex;
    onchange?.(selected);
    publish();
  }
</script>

<label class="dd" style:width="{width}px">
  {#if label}<span class="caps">{label}</span>{/if}
  <select
    {disabled}
    value={selected}
    aria-label={name || label}
    onchange={change}
    onpointerenter={publish}
    onpointerleave={clearHint}
    onfocus={publish}
    onblur={clearHint}
  >
    {#each options as opt, i (i)}
      <option value={i}>{opt}</option>
    {/each}
  </select>
</label>

<style>
  .dd {
    display: inline-flex;
    flex-direction: column;
    gap: 2px;
  }
  select {
    appearance: none;
    background: var(--track)
      url("data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg' width='8' height='5'><path d='M0 0h8L4 5z' fill='%238b959b'/></svg>")
      no-repeat right 6px center;
    color: var(--text);
    border: 1px solid var(--edge);
    border-radius: var(--radius);
    padding: 3px 18px 3px 6px;
    font: inherit;
    cursor: pointer;
  }
  select:hover:not(:disabled) {
    border-color: var(--edge-light);
  }
  select:focus-visible {
    outline: none;
    box-shadow: var(--focus);
  }
  select:disabled {
    opacity: 0.4;
  }
</style>
