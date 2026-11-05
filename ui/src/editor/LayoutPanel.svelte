<script lang="ts">
  import { layouts, MAX_NAME } from '../model/layouts.svelte';

  let name = $state('');

  function save(): void {
    if (layouts.saveAs(name)) name = '';
  }
</script>

<div class="layouts">
  <span class="caps">Layouts</span>
  <ul aria-label="Saved layouts">
    {#each layouts.all as l (l.name)}
      <li>
        <button type="button" class="apply" onclick={() => layouts.apply(l.name)} title="Open this arrangement of tabs">{l.name}</button>
        {#if layouts.isBuiltIn(l.name)}
          <span class="dim">built in</span>
        {:else}
          <button type="button" class="x" aria-label="Delete layout {l.name}" onclick={() => layouts.remove(l.name)}>×</button>
        {/if}
      </li>
    {/each}
  </ul>
  <div class="row">
    <input
      type="text"
      maxlength={MAX_NAME}
      placeholder="Name for what is on screen"
      aria-label="Name for the layout"
      bind:value={name}
      onkeydown={(e) => e.key === 'Enter' && save()}
    />
    <button type="button" disabled={name.trim() === ''} onclick={save}>Save</button>
  </div>
  <div class="row">
    <button type="button" onclick={() => layouts.reset()}>Reset layout</button>
  </div>
  {#if layouts.message}<p class="note" role="status">{layouts.message}</p>{/if}
  <p class="note">A layout remembers the workspace and the tab shown in each one. The window opens the way you left it.</p>
</div>

<style>
  .layouts {
    display: flex;
    flex-direction: column;
    gap: 6px;
    width: 300px;
  }
  ul {
    list-style: none;
    margin: 0;
    padding: 0;
    display: flex;
    flex-direction: column;
    gap: 2px;
  }
  li {
    display: flex;
    gap: 8px;
    align-items: center;
  }
  .apply {
    flex: 1;
    text-align: left;
  }
  .row {
    display: flex;
    gap: 6px;
  }
  input {
    flex: 1;
    background: var(--track);
    color: var(--text);
    border: 1px solid var(--edge);
    border-radius: var(--radius);
    padding: 4px 6px;
  }
  input:focus {
    outline: none;
    border-color: var(--accent);
  }
  button {
    background: var(--well-deep);
    border: 1px solid var(--edge);
    border-radius: var(--radius);
    padding: 3px 10px;
    cursor: pointer;
    color: inherit;
    font: inherit;
  }
  button:hover:not(:disabled) {
    background: var(--well);
  }
  button:disabled {
    opacity: 0.4;
    cursor: default;
  }
  .x {
    padding: 0 7px;
  }
  .dim,
  .note {
    margin: 0;
    color: var(--text-dim);
    font-size: 11px;
  }
</style>
