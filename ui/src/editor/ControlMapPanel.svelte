<script lang="ts">
  import { paramInfo } from '../model/catalog';
  import { describe } from '../model/controlMap';
  import { controls } from '../model/controls.svelte';
  import { blockTitle } from '../model/labels';
  import { useStore } from './context';

  const store = useStore();

  /** "Tone 1 · Cutoff Frequency" for a binding (the catalogue knows the parameter's name). */
  const nameOf = (b: (typeof controls.bindings)[number]): string => {
    const info = paramInfo(store.catalog, b.kind, b.block, b.id);
    return `${blockTitle(b.block, b.kind)} · ${info?.name ?? b.id}`;
  };
</script>

<div class="map">
  <span class="caps">MIDI controller map</span>
  {#if controls.learning}
    <p class="learn"><button type="button" onclick={() => controls.cancelLearn()}>Cancel linking</button></p>
  {/if}
  {#if controls.bindings.length === 0}
    <p class="note">Nothing is linked yet. Right-click a knob and choose Link to MIDI CC, then move a controller on your keyboard or on the XP-60.</p>
  {:else}
    <ul aria-label="Linked controllers">
      {#each controls.bindings as b (b.kind + b.block + b.id)}
        <li>
          <span class="cc">{describe(b)}</span>
          <span class="what">{nameOf(b)}</span>
          <button type="button" aria-label="Unlink {nameOf(b)}" onclick={() => controls.unlink(b)}>×</button>
        </li>
      {/each}
    </ul>
    <button type="button" class="clear" onclick={() => controls.clearAll()}>Clear all</button>
  {/if}
  <p class="note">A linked controller sets the parameter from its smallest to its largest value. One sweep is one undo step. Controller 0 and 32 (Bank Select) are never linked.</p>
</div>

<style>
  .map {
    display: flex;
    flex-direction: column;
    gap: 6px;
    width: 360px;
  }
  ul {
    list-style: none;
    margin: 0;
    padding: 0;
    max-height: 220px;
    overflow: auto;
    display: flex;
    flex-direction: column;
    gap: 2px;
  }
  li {
    display: flex;
    gap: 8px;
    align-items: center;
    padding: 2px 4px;
    background: var(--body-deep);
    border-radius: 2px;
  }
  .cc {
    font-family: var(--font-mono);
    color: var(--accent);
    min-width: 76px;
  }
  .what {
    flex: 1;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }
  button {
    background: var(--well-deep);
    border: 1px solid var(--edge);
    border-radius: var(--radius);
    padding: 2px 8px;
    cursor: pointer;
    color: inherit;
    font: inherit;
  }
  button:hover {
    background: var(--well);
  }
  .clear {
    align-self: flex-start;
  }
  .learn {
    margin: 0;
    padding: 4px 6px;
    border: 1px solid var(--accent);
    border-radius: var(--radius);
    display: flex;
    gap: 8px;
    align-items: center;
  }
  .note {
    margin: 0;
    color: var(--text-dim);
    font-size: 11px;
  }
</style>
