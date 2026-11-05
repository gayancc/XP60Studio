<script lang="ts">
  import { tick } from 'svelte';
  import { closeMenu, menu, type MenuItem } from './menu.svelte';

  let el: HTMLDivElement | undefined = $state();
  let left = $state(0);
  let top = $state(0);
  let invoker: HTMLElement | null = null;

  // Keep the menu inside the window once it has a size.
  $effect(() => {
    if (!menu.open) return;
    const x = menu.x;
    const y = menu.y;
    left = x;
    top = y;
    invoker = document.activeElement instanceof HTMLElement ? document.activeElement : null;
    void tick().then(() => {
      if (!el) return;
      const r = el.getBoundingClientRect();
      left = Math.max(4, Math.min(x, window.innerWidth - r.width - 4));
      top = Math.max(4, Math.min(y, window.innerHeight - r.height - 4));
      const first = el.querySelector<HTMLElement>('input, button:not(:disabled)');
      first?.focus();
      if (first instanceof HTMLInputElement) first.select();
    });
  });

  function close(): void {
    closeMenu();
    invoker?.focus();
    invoker = null;
  }

  function run(item: MenuItem): void {
    if (item.disabled) return;
    close();
    item.action?.();
  }

  function submit(item: MenuItem, e: KeyboardEvent & { currentTarget: HTMLInputElement }): void {
    if (e.key !== 'Enter') return;
    e.preventDefault();
    const text = e.currentTarget.value;
    close();
    item.input?.onSubmit(text);
  }

  function key(e: KeyboardEvent): void {
    if (e.key === 'Escape' || e.key === 'Tab') {
      e.preventDefault();
      close();
      return;
    }
    if (e.key !== 'ArrowDown' && e.key !== 'ArrowUp') return;
    e.preventDefault();
    const focusables = [...(el?.querySelectorAll<HTMLElement>('input, button:not(:disabled)') ?? [])];
    if (focusables.length === 0) return;
    const i = focusables.indexOf(document.activeElement as HTMLElement);
    const step = e.key === 'ArrowDown' ? 1 : -1;
    const next = focusables[i < 0 ? (step > 0 ? 0 : focusables.length - 1) : (i + step + focusables.length) % focusables.length];
    next?.focus();
  }
</script>

<svelte:window onpointerdown={(e) => menu.open && !el?.contains(e.target as Node) && closeMenu()} />

{#if menu.open}
  <div
    class="menu"
    bind:this={el}
    style:left="{left}px"
    style:top="{top}px"
    role="menu"
    aria-label="Control menu"
    tabindex="-1"
    onkeydown={key}
  >
    {#each menu.items as item, i (i)}
      {#if item.separator}
        <hr />
      {:else if item.input}
        <label class="row input">
          <span>{item.label}</span>
          <input
            type="text"
            inputmode="decimal"
            value={item.input.initial}
            onkeydown={(e) => submit(item, e)}
            aria-label={item.label}
          />
        </label>
      {:else}
        <button type="button" role="menuitem" class="row" disabled={item.disabled} onclick={() => run(item)}>
          {item.label}
        </button>
      {/if}
    {/each}
  </div>
{/if}

<style>
  .menu {
    position: fixed;
    z-index: 1000;
    min-width: 190px;
    padding: 0;
    background: var(--pop-bg);
    color: var(--pop-text);
    border: 2px solid var(--pop-border);
  }
  .row {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 10px;
    width: 100%;
    height: var(--pop-row);
    padding: 0 12px;
    background: transparent;
    color: var(--pop-text);
    border: 0;
    border-radius: 0;
    text-align: left;
    cursor: pointer;
  }
  button.row:hover:not(:disabled),
  button.row:focus-visible {
    background: var(--pop-hover-bg);
    color: var(--pop-hover-text);
    outline: none;
  }
  button.row:disabled {
    color: var(--pop-dim);
    cursor: default;
  }
  .input input {
    width: 64px;
    background: var(--track);
    color: var(--text);
    border: 1px solid var(--edge);
    border-radius: 2px;
    padding: 1px 4px;
    font-family: var(--font-mono);
  }
  .input input:focus {
    outline: none;
    border-color: var(--accent);
  }
  hr {
    border: 0;
    border-top: 1px solid var(--pop-head-bg);
    margin: 3px 2px;
  }
</style>
