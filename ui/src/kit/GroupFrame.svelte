<script lang="ts">
  import type { Snippet } from 'svelte';

  interface Props {
    title: string;
    color?: string;
    /** The moment (Date.now()) this group was pointed out; the border lights up briefly when it is recent. 0 or omitted: never. */
    flash?: number;
    children: Snippet;
  }

  let { title, color = 'var(--accent)', flash = 0, children }: Props = $props();

  // Pointing a group out: after a jump from elsewhere the group's border takes the accent colour for a moment, then fades back, and the group scrolls into view.
  const SHOWN_FOR = 1800;
  let lit = $state(false);
  let el: HTMLFieldSetElement | undefined = $state();
  $effect(() => {
    if (!flash || Date.now() - flash > 2500) return;
    lit = true;
    el?.scrollIntoView?.({ block: 'nearest', behavior: 'smooth' });
    const t = setTimeout(() => (lit = false), SHOWN_FOR);
    return () => clearTimeout(t);
  });
</script>

<fieldset class="group" class:lit bind:this={el} style:--tab={color}>
  <legend><span>{title}</span></legend>
  <div class="body">{@render children()}</div>
</fieldset>

<style>
  .group {
    margin: 0;
    padding: 0 8px 8px;
    /* Fruity Limiter groups, measured 2026-11-08: 1 px #8e9496 rule, title inside, no filled tab. */
    border: 1px solid #8e9496;
    border-radius: 0;
    background: var(--body);
    min-inline-size: 0;
    transition: border-color 0.7s ease, box-shadow 0.7s ease;
  }
  .group.lit {
    border-color: var(--tab);
    box-shadow: 0 0 0 1px color-mix(in srgb, var(--tab) 35%, transparent);
  }
  legend {
    display: block;
    width: 100%;
    margin: 0;
    padding: 3px 0 0;
    text-align: center;
  }
  legend span {
    display: inline-block;
    padding: 0;
    background: transparent;
    border: 0;
    font-family: var(--font-caps);
    font-size: 10.5px;
    letter-spacing: 0.08em;
    text-transform: uppercase;
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
