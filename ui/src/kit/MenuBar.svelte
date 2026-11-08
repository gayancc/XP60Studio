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
  /** Where the open menu hangs: against the window, because the bar sits in a row that clips anything below it. */
  let at = $state({ left: 0, top: 0 });

  function openAt(i: number, title: HTMLElement): void {
    const r = title.getBoundingClientRect();
    at = { left: Math.max(4, Math.min(r.left, window.innerWidth - 300)), top: r.bottom + 1 };
    open = i;
  }
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
        onclick={(e) => (open === i ? (open = null) : openAt(i, e.currentTarget))}
        onpointerenter={(e) => {
          if (open !== null) openAt(i, e.currentTarget);
        }}>{m.title}</button>
      {#if open === i}
        {#if m.panel}
          <div class="drop panel" style:left="{at.left}px" style:top="{at.top}px">{@render m.panel()}</div>
        {:else}
          <ul class="drop" role="menu" aria-label={m.title} style:left="{at.left}px" style:top="{at.top}px">
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
    background-image: var(--focus-fill);
  }
  .drop {
    position: fixed;
    z-index: 950;
    max-height: calc(100vh - 40px);
    overflow-y: auto;
    min-width: 220px;
    margin: 0;
    padding: 0;
    list-style: none;
    background: var(--pop-bg);
    color: var(--pop-text);
    border: 2px solid var(--pop-border);
  }
  /* Settings-style panels use FL's MIDI-settings surface: dark fill, 1 px rim
     (40,49,55). The 10 px inset is the panel's own padding. The Synth card
     cancels it, because that card already pads itself and must stay flush
     with the rim. */
  .drop.panel {
    padding: 10px;
    min-width: 0;
    background: #353e44;
    color: var(--text);
    border: 1px solid #283137;
  }
  .drop:not(.panel) button {
    display: flex;
    align-items: center;
    gap: 8px;
    width: 100%;
    height: var(--pop-row);
    padding: 0 12px 0 0;
    background: transparent;
    border: 0;
    border-radius: 0;
    text-align: left;
    cursor: default;
    color: var(--pop-text);
  }
  .drop:not(.panel) button:hover:not(:disabled),
  .drop:not(.panel) button:focus-visible {
    background: var(--pop-hover-bg);
    color: var(--pop-hover-text);
    outline: none;
  }
  .drop:not(.panel) button:disabled {
    color: var(--pop-dim);
    cursor: default;
  }
  .tick {
    width: 18px;
    text-align: center;
    color: inherit;
  }
  button:hover:not(:disabled) .tick {
    color: inherit;
  }
  .lab {
    flex: 1;
  }
  .hint {
    color: var(--pop-dim);
    font-size: 11px;
  }
  button:hover:not(:disabled) .hint {
    color: var(--pop-hover-dim);
  }
  .sep {
    height: 1px;
    margin: 2px 0;
    background: var(--pop-head-bg);
  }
</style>
