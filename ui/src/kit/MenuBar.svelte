<script lang="ts">
  import type { Snippet } from 'svelte';

  export interface MenuItem {
    label?: string;
    /** Shortcut text shown at the right. */
    hint?: string;
    checked?: boolean;
    disabled?: boolean;
    separator?: boolean;
    run?: () => void;
  }

  export interface Menu {
    title: string;
    items?: readonly MenuItem[];
    /** A free-form panel instead of a list of commands (the connection settings, for example). */
    panel?: Snippet;
  }

  interface Props {
    menus: readonly Menu[];
    name?: string;
  }

  let { menus, name = 'Menu' }: Props = $props();
  let open = $state<number | null>(null);
  let root: HTMLElement | undefined = $state();

  function pick(item: MenuItem): void {
    if (item.disabled || item.separator) return;
    open = null;
    item.run?.();
  }

  function outside(e: PointerEvent): void {
    if (open !== null && root && !root.contains(e.target as Node)) open = null;
  }

  function key(e: KeyboardEvent): void {
    if (e.key === 'Escape' && open !== null) {
      e.preventDefault();
      e.stopPropagation();
      open = null;
    }
  }
</script>

<svelte:window onpointerdown={outside} onkeydown={key} />

<nav class="bar" bind:this={root} aria-label={name}>
  {#each menus as m, i (m.title)}
    <div class="menu">
      <button
        type="button"
        class="title"
        class:open={open === i}
        aria-haspopup="true"
        aria-expanded={open === i}
        onclick={() => (open = open === i ? null : i)}
        onpointerenter={() => {
          if (open !== null) open = i;
        }}>{m.title}</button>
      {#if open === i}
        {#if m.panel}
          <div class="drop panel">{@render m.panel()}</div>
        {:else}
          <ul class="drop" role="menu" aria-label={m.title}>
            {#each m.items ?? [] as it, j (j)}
              {#if it.separator}
                <li class="sep" role="separator"></li>
              {:else}
                <li role="none">
                  <button
                    type="button"
                    role={it.checked === undefined ? 'menuitem' : 'menuitemcheckbox'}
                    aria-checked={it.checked}
                    disabled={it.disabled}
                    onclick={() => pick(it)}>
                    <span class="tick" aria-hidden="true">{it.checked ? '✓' : ''}</span>
                    <span class="lab">{it.label}</span>
                    {#if it.hint}<span class="hint">{it.hint}</span>{/if}
                  </button>
                </li>
              {/if}
            {/each}
          </ul>
        {/if}
      {/if}
    </div>
  {/each}
</nav>

<style>
  .bar {
    display: flex;
    gap: 1px;
    position: relative;
    z-index: 900;
  }
  .menu {
    position: relative;
  }
  .title {
    background: transparent;
    border: 0;
    border-radius: var(--radius);
    padding: 3px 10px;
    cursor: pointer;
    color: var(--text);
  }
  .title:hover,
  .title.open {
    background: var(--well);
  }
  .title:focus-visible {
    outline: none;
    box-shadow: var(--focus);
  }
  .drop {
    position: absolute;
    top: calc(100% + 1px);
    left: 0;
    min-width: 220px;
    margin: 0;
    padding: 3px;
    list-style: none;
    background: var(--caption);
    border: 1px solid var(--edge-light);
    border-radius: var(--radius-lg);
    box-shadow: 0 8px 24px rgba(0, 0, 0, 0.5);
  }
  .drop.panel {
    padding: 10px;
    min-width: 280px;
  }
  .drop button {
    display: flex;
    align-items: center;
    gap: 8px;
    width: 100%;
    padding: 4px 8px;
    background: transparent;
    border: 0;
    border-radius: 2px;
    text-align: left;
    cursor: pointer;
    color: var(--text);
  }
  .drop button:hover:not(:disabled),
  .drop button:focus-visible {
    background: var(--accent);
    color: var(--accent-ink);
    outline: none;
  }
  .drop button:disabled {
    color: var(--text-faint);
    cursor: default;
  }
  .tick {
    width: 12px;
    color: var(--accent);
  }
  button:hover:not(:disabled) .tick {
    color: inherit;
  }
  .lab {
    flex: 1;
  }
  .hint {
    color: var(--text-dim);
    font-family: var(--font-mono);
    font-size: 10.5px;
  }
  button:hover:not(:disabled) .hint {
    color: inherit;
  }
  .sep {
    height: 1px;
    margin: 3px 4px;
    background: var(--edge);
  }
</style>
