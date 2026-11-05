<script lang="ts">
  import { tick } from 'svelte';
  import { clearHint, showHint } from './hint.svelte';

  interface Props {
    options: readonly string[];
    selected: number;
    name?: string;
    context?: string;
    disabled?: boolean;
    onchange?: (index: number) => void;
  }

  let { options, selected = $bindable(), name = '', context = '', disabled = false, onchange }: Props = $props();

  let group: HTMLDivElement | undefined = $state();

  const publish = (): void => showHint({ context, name, value: options[selected] ?? '' });

  function choose(i: number): void {
    if (disabled || i === selected) return;
    selected = i;
    onchange?.(i);
    publish();
    void tick().then(() => group?.querySelectorAll<HTMLElement>('[role=radio]')[i]?.focus());
  }

  function key(e: KeyboardEvent): void {
    const dir = e.key === 'ArrowRight' || e.key === 'ArrowDown' ? 1 : e.key === 'ArrowLeft' || e.key === 'ArrowUp' ? -1 : 0;
    if (dir === 0) return;
    e.preventDefault();
    choose(Math.min(options.length - 1, Math.max(0, selected + dir)));
  }
</script>

<div
  class="seg"
  bind:this={group}
  role="radiogroup"
  aria-label={name || 'Options'}
  tabindex="-1"
  onkeydown={key}
  onpointerenter={publish}
  onpointerleave={clearHint}
>
  {#each options as opt, i (i)}
    <button
      type="button"
      role="radio"
      aria-checked={i === selected}
      tabindex={i === selected || (selected < 0 && i === 0) ? 0 : -1}
      class:active={i === selected}
      {disabled}
      onclick={() => choose(i)}
    >
      {opt}
    </button>
  {/each}
</div>

<style>
  .seg {
    display: inline-flex;
    border: 1px solid var(--edge);
    border-radius: var(--radius);
    overflow: hidden;
    background: var(--track);
  }
  button {
    background: transparent;
    border: 0;
    padding: 3px 9px;
    font-family: var(--font-caps);
    text-transform: uppercase;
    letter-spacing: 0.05em;
    font-size: 10px;
    color: var(--text-dim);
    cursor: pointer;
    border-right: 1px solid var(--edge);
  }
  button:last-child {
    border-right: 0;
  }
  button:hover:not(:disabled):not(.active) {
    background: var(--caption);
    color: var(--text);
  }
  button.active {
    background: var(--accent);
    color: var(--accent-ink);
  }
  button:focus-visible {
    outline: none;
    background-image: var(--focus-fill);
  }
  button:disabled {
    opacity: 0.4;
    cursor: default;
  }
</style>
