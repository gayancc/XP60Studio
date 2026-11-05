<script lang="ts">
  import type { Snippet } from 'svelte';
  import Tabs from './Tabs.svelte';

  interface Props {
    tabs: readonly string[];
    selected: number;
    /** Accent for the selected tab; a function lets each tab carry its own colour. */
    color?: string | ((index: number) => string);
    name?: string;
    onchange: (index: number) => void;
    /** Small text at the right of the tab strip. */
    meta?: string;
    children: Snippet<[number]>;
  }

  let { tabs, selected, color = 'var(--accent)', name = 'Sections', onchange, meta = '', children }: Props = $props();
  const accent = $derived(typeof color === 'function' ? color(selected) : color);
</script>

<section class="tp" aria-label={name}>
  <div class="strip">
    <Tabs {tabs} {selected} color={accent} {name} {onchange} />
    {#if meta}<span class="meta">{meta}</span>{/if}
  </div>
  <div class="body" role="tabpanel">{@render children(selected)}</div>
</section>

<style>
  .tp {
    display: flex;
    flex-direction: column;
    min-height: 0;
    min-width: 0;
  }
  .strip {
    display: flex;
    align-items: flex-end;
    gap: 8px;
    padding: 0 4px;
    border-bottom: 1px solid var(--edge);
  }
  .meta {
    margin-left: auto;
    padding-bottom: 3px;
    color: var(--text-faint);
    font-size: 10px;
  }
  .body {
    flex: 1;
    min-height: 0;
    padding: 8px;
    background: var(--body);
    border: 1px solid var(--edge);
    border-top: 0;
    border-radius: 0 0 var(--radius-lg) var(--radius-lg);
    overflow: hidden;
  }
</style>
