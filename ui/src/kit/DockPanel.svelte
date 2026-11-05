<script lang="ts">
  import type { Snippet } from 'svelte';

  interface Props {
    title: string;
    focused?: boolean;
    collapsed?: boolean;
    /** Small text at the right of the caption, for example the sound name. */
    meta?: string;
    /** Take the room left in a column of panels, with the content filling it (a list that scrolls inside). */
    fill?: boolean;
    onfocus?: () => void;
    children: Snippet;
  }

  let { title, focused = false, collapsed = $bindable(false), meta = '', fill = false, onfocus, children }: Props = $props();
</script>

<section class="panel" class:focused class:fill={fill && !collapsed} role="group" aria-label={title} onpointerdown={() => onfocus?.()} onfocusin={() => onfocus?.()}>
  <header>
    <button
      type="button"
      class="caption"
      aria-expanded={!collapsed}
      onclick={() => (collapsed = !collapsed)}
    >
      <span class="chev" class:open={!collapsed}>▸</span>
      <span class="title">{title}</span>
      {#if meta}<span class="meta">{meta}</span>{/if}
    </button>
  </header>
  {#if !collapsed}
    <div class="content">{@render children()}</div>
  {/if}
</section>

<style>
  .panel {
    background: var(--body);
    border: 1px solid var(--edge);
    border-radius: var(--radius-lg);
    overflow: hidden;
  }
  .panel.fill {
    flex: 1 1 0;
    min-height: 120px;
    display: flex;
    flex-direction: column;
  }
  .panel.fill > .content {
    flex: 1;
    min-height: 0;
    display: flex;
    flex-direction: column;
    overflow-x: hidden;
  }
  .caption {
    display: flex;
    align-items: center;
    gap: 6px;
    width: 100%;
    padding: 3px 8px;
    background: var(--caption);
    border: 0;
    border-bottom: 1px solid var(--edge);
    font-family: var(--font-caps);
    font-size: 11px;
    letter-spacing: 0.03em;
    color: var(--text-dim);
    cursor: pointer;
    text-align: left;
  }
  .focused .caption {
    color: var(--accent);
    box-shadow: inset 0 -2px 0 var(--accent);
  }
  .caption:focus-visible {
    outline: none;
    box-shadow: inset var(--focus);
  }
  .chev {
    display: inline-block;
    font-size: 9px;
    transition: transform 100ms;
  }
  .chev.open {
    transform: rotate(90deg);
  }
  .meta {
    margin-left: auto;
    color: var(--text-faint);
    font-size: 10px;
  }
  .content {
    padding: 8px;
    overflow-x: auto;
  }
</style>
