<script lang="ts">
  import type { Snippet } from 'svelte';

  interface Props {
    title: string;
    color?: string;
    children: Snippet;
  }

  let { title, color = 'var(--accent)', children }: Props = $props();
</script>

<fieldset class="group" style:--tab={color}>
  <legend><span>{title}</span></legend>
  <div class="body">{@render children()}</div>
</fieldset>

<style>
  .group {
    margin: 0;
    padding: 0 8px 8px;
    border: 1px solid var(--edge);
    border-radius: var(--radius-lg);
    background: var(--body);
    min-inline-size: 0;
  }
  legend {
    padding: 0;
    margin-left: -1px;
  }
  /* Slanted-edge tab: the right edge leans, as on the native FL group titles. */
  legend span {
    display: inline-block;
    padding: 1px 16px 1px 8px;
    background: var(--caption);
    border: 1px solid var(--edge);
    border-bottom: 0;
    border-left-color: var(--tab);
    border-left-width: 2px;
    clip-path: polygon(0 0, 100% 0, calc(100% - 9px) 100%, 0 100%);
    font-family: var(--font-caps);
    font-size: 10.5px;
    letter-spacing: 0.03em;
    color: var(--text);
  }
  .body {
    display: flex;
    flex-wrap: wrap;
    align-items: flex-start;
    gap: var(--gap) 10px;
    padding-top: 8px;
  }
</style>
