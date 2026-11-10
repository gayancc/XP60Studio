<script lang="ts">
  import { tick, type Snippet } from 'svelte';
  import { clearHint, showHint } from './hint.svelte';

  // A list picker drawn the FL Studio way: a flat recessed readout that opens a dark flat list, no native widget. Groups are heading bars inside the one list (as in
  // FL's Windows menu), the active entry carries a check mark. Click or press Enter/Space/Down to open; Up and Down move, Enter picks, Esc closes.
  export interface DropdownGroup {
    label: string;
    /** Indices into `options` that belong to the group, in the order shown. */
    indices: readonly number[];
  }

  interface Props {
    options: readonly string[];
    selected: number;
    /** Optional grouping: the list opens as groups with cascading sub-menus. */
    groups?: readonly DropdownGroup[];
    label?: string;
    name?: string;
    context?: string;
    disabled?: boolean;
    width?: number;
    /** Optional small drawing shown before an entry's text, in the list and on the readout (receives the index into `options`). */
    glyph?: Snippet<[number]>;
    onchange?: (index: number) => void;
    /** The readout has no face of its own (a host such as PatchSelector draws it) and fills its parent. */
    bare?: boolean;
    /** Open the list under this element (its full width) instead of under the readout. */
    anchor?: HTMLElement;
  }

  let { options, selected = $bindable(), groups, label = '', name = label, context = '', disabled = false, width = 130, glyph, onchange, bare = false, anchor }: Props = $props();

  const uid = $props.id();
  let trigger: HTMLButtonElement | undefined = $state();
  let menu: HTMLDivElement | undefined = $state();
  let open = $state(false);
  let left = $state(0);
  let top = $state(0);
  let minWidth = $state(0);
  let maxHeight = $state(400);
  /** The list opens above the readout (not enough room below), so it is placed by its measured height. */
  let up = false;
  /** Keyboard cursor: an index into `options`. */
  let cursor = $state(0);

  const text = $derived(options[selected] ?? '');
  /** What the list shows, top to bottom: group headers and entries. Without groups it is just the entries. */
  const lines = $derived<readonly ({ header: string } | { option: number })[]>(
    groups ? groups.flatMap((g) => [{ header: g.label }, ...g.indices.map((option) => ({ option }))]) : options.map((_, option) => ({ option })),
  );
  const order = $derived(lines.flatMap((l) => ('option' in l ? [l.option] : [])));

  const publish = (): void => showHint({ context, name: name || label, value: text, shortcut: 'Click to open · Up or Down to step' });

  function choose(i: number): void {
    close();
    if (i === selected) return;
    selected = i;
    onchange?.(i);
    publish();
  }

  function place(): void {
    if (!trigger) return;
    const r = (anchor ?? trigger).getBoundingClientRect();
    minWidth = r.width;
    const below = window.innerHeight - r.bottom - 8;
    const above = r.top - 8;
    left = r.left;
    up = false;
    if (below >= 220 || below >= above) {
      top = r.bottom + 2;
      maxHeight = Math.max(120, below);
    } else {
      up = true;
      top = Math.max(4, r.top - 2 - Math.min(above, order.length * 20 + 40));
      maxHeight = Math.max(120, above);
    }
  }

  async function show(): Promise<void> {
    if (disabled || open) return;
    place();
    cursor = selected;
    open = true;
    await tick();
    if (menu) {
      const r = menu.getBoundingClientRect();
      left = Math.max(4, Math.min(left, window.innerWidth - r.width - 4));
      if (up && trigger) top = Math.max(4, (anchor ?? trigger).getBoundingClientRect().top - 2 - r.height);
      menu.focus();
      menu.querySelector<HTMLElement>('.row.cur')?.scrollIntoView?.({ block: 'center' });
    }
  }

  function close(focusTrigger = true): void {
    if (!open) return;
    open = false;
    if (focusTrigger) trigger?.focus();
  }

  function triggerKey(e: KeyboardEvent): void {
    if (disabled) return;
    if (e.key === 'Enter' || e.key === ' ') {
      e.preventDefault();
      void show();
    } else if (e.key === 'ArrowDown' || e.key === 'ArrowUp') {
      e.preventDefault();
      const next = Math.min(options.length - 1, Math.max(0, selected + (e.key === 'ArrowDown' ? 1 : -1)));
      if (next !== selected) {
        selected = next;
        onchange?.(next);
        publish();
      }
    }
  }

  function menuKey(e: KeyboardEvent): void {
    if (e.key === 'Escape' || e.key === 'Tab') {
      e.preventDefault();
      close();
    } else if (e.key === 'ArrowDown' || e.key === 'ArrowUp') {
      e.preventDefault();
      const at = order.indexOf(cursor);
      cursor = order[(at + (e.key === 'ArrowDown' ? 1 : -1) + order.length) % order.length] ?? cursor;
      void tick().then(() => menu?.querySelector<HTMLElement>('.row.kb')?.scrollIntoView?.({ block: 'nearest' }));
    } else if (e.key === 'Enter' || e.key === ' ') {
      e.preventDefault();
      choose(cursor);
    }
  }

  function outside(e: PointerEvent): void {
    if (!open) return;
    const t = e.target as Node;
    if (menu?.contains(t) || trigger?.contains(t)) return;
    close(false);
  }
</script>

<svelte:window onpointerdown={outside} onresize={() => close(false)} />

<div class="dd" class:bare style:width={bare ? undefined : `${width}px`}>
  {#if label}<span class="caps">{label}</span>{/if}
  <button
    bind:this={trigger}
    type="button"
    class="trigger"
    class:open
    role="combobox"
    aria-haspopup="listbox"
    aria-expanded={open}
    aria-controls="{uid}-list"
    aria-label={name || label}
    {disabled}
    onclick={() => (open ? close() : void show())}
    onkeydown={triggerKey}
    onpointerenter={publish}
    onpointerleave={clearHint}
    onfocus={publish}
    onblur={clearHint}
  >
    {#if glyph}{@render glyph(selected)}{/if}<span class="text">{text}</span>
    <svg width="8" height="5" viewBox="0 0 8 5" aria-hidden="true"><path d="M0 0h8L4 5z" /></svg>
  </button>
</div>

{#if open}
  <div id="{uid}-list" class="list" class:combo={!groups} bind:this={menu} style:left="{left}px" style:top="{top}px" style:min-width="{minWidth}px" style:max-height="{maxHeight}px" role="listbox" aria-label={name || label} tabindex="-1" onkeydown={menuKey}>
    {#each lines as l, k (k)}
      {#if 'header' in l}
        <div class="header" role="presentation">{l.header}</div>
      {:else}
        <div
          class="row"
          class:cur={l.option === selected}
          class:kb={l.option === cursor}
          role="option"
          aria-selected={l.option === selected}
          tabindex="-1"
          onpointerenter={() => (cursor = l.option)}
          onclick={() => choose(l.option)}
          onkeydown={() => {}}
        >
          {#if groups}<span class="tick" aria-hidden="true">{l.option === selected ? '✓' : ''}</span>{/if}{#if glyph}{@render glyph(l.option)}{/if}{options[l.option]}
        </div>
      {/if}
    {/each}
  </div>
{/if}

<style>
  .dd {
    display: inline-flex;
    flex-direction: column;
    gap: 2px;
  }
  .dd.bare {
    display: flex;
    flex-direction: row;
    width: 100%;
    height: 100%;
  }
  .bare .trigger {
    height: 100%;
    border-radius: 0;
    background: transparent;
  }
  .bare .trigger:hover:not(:disabled),
  .bare .trigger.open {
    background: transparent;
  }
  .caps {
    font: 600 10px var(--font-caps);
    letter-spacing: 0.08em;
    text-transform: uppercase;
    color: var(--text-dim);
  }
  /* Closed FL "Line" combo, measured 2026-11-08: 23 px, top-light dark face, light label, no separate border. Hover was not captured. */
  .trigger {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 6px;
    width: 100%;
    height: 23px;
    padding: 0 7px;
    border: 0;
    border-radius: 2px;
    background: linear-gradient(180deg, #4e5357 0%, #2f3438 42%, #272c30 100%);
    color: #bbc3bd;
    font: inherit;
    text-align: left;
    cursor: pointer;
  }
  .trigger:hover:not(:disabled),
  .trigger.open {
    background: linear-gradient(180deg, #4e5357 0%, #2f3438 42%, #272c30 100%);
    color: #bbc3bd;
  }
  .trigger:focus-visible {
    outline: none;
    background-image: var(--focus-fill);
  }
  .trigger:disabled {
    opacity: 0.4;
    cursor: default;
  }
  .text {
    flex: 1;
    min-width: 0;
    overflow: hidden;
    white-space: nowrap;
    text-overflow: ellipsis;
  }
  svg {
    flex: none;
    fill: #697177;
  }
  /* The list is FL Studio's menu, from measured pixels of its VIEW menu (tokens --pop-*): flat light body, 2 px dark border, 16 px heading bars, 19 px entries. */
  .list {
    position: fixed;
    z-index: 1100;
    box-sizing: border-box;
    overflow-y: auto;
    padding: 0;
    background: var(--pop-bg);
    border: 2px solid var(--pop-border);
    color: var(--pop-text);
    outline: none;
  }
  .header {
    height: var(--pop-head);
    padding: 0 4px;
    background: var(--pop-head-bg);
    color: var(--pop-head-text);
    font-size: 11px;
    line-height: var(--pop-head);
  }
  .row {
    display: flex;
    align-items: center;
    height: var(--pop-row);
    gap: 8px;
    padding: 0 14px 0 0;
    color: var(--pop-text);
    white-space: nowrap;
    cursor: default;
    user-select: none;
  }
  .tick {
    flex: none;
    width: 18px;
    text-align: center;
  }
  .row.kb {
    background: var(--pop-hover-bg);
    color: var(--pop-hover-text);
  }
  /* A plain list is FL's combo list: dark body, 2 px border, light text, a pale hover fill with dark text, no check column. */
  .combo {
    background: var(--combo-bg);
    border-color: var(--combo-border);
    color: var(--combo-text);
  }
  .combo .row {
    padding-left: 6px;
    color: var(--combo-text);
  }
  .combo .row.kb {
    background: var(--combo-hover-bg);
    color: var(--combo-hover-text);
  }
</style>
