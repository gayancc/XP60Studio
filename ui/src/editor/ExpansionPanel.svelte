<script lang="ts">
  import { expansionBoards } from '../model/factory';
  import { expansions, MAX_BOARDS } from '../model/expansions.svelte';

  // Which Wave Expansion boards are in this keyboard. The wave picker offers the instrument's own waves and the waves of the boards
  // ticked here. Expansion boards are read-only and only sound while installed, so this must match what is physically in the slots.
  const main = expansionBoards.filter((b) => b.number <= 19);
  const other = expansionBoards.filter((b) => b.number > 19);
</script>

{#snippet board(b: (typeof expansionBoards)[number])}
  {@const on = expansions.has(b.number)}
  <button
    type="button"
    class="b"
    class:on
    role="switch"
    aria-checked={on}
    disabled={!on && expansions.full}
    onclick={() => expansions.set(b.number, !on)}
    title={b.waves ? `${b.waves.length} wave names are held for this board` : 'Roland publishes no waveform list for this board: its waves are chosen by number'}
  >
    <i></i>
    <span class="n">SR-JV80-{String(b.number).padStart(2, '0')}</span>
    <span class="t">{b.name}</span>
    {#if b.waves}<span class="c">{b.waves.length} waves</span>{/if}
  </button>
{/snippet}

<div class="ex">
  <header>
    <h3>Installed expansion boards</h3>
    <span class="count" class:full={expansions.full}>{expansions.installed.length} of {MAX_BOARDS} slots</span>
  </header>
  <p class="dim">Tick the boards that are physically in your keyboard (EXP-A to EXP-D). They are read-only and only sound while installed, so the wave picker offers their waves only when ticked here.</p>
  <div class="grid">{#each main as b (b.number)}{@render board(b)}{/each}</div>
  <h4>Promotional and regional boards</h4>
  <div class="grid">{#each other as b (b.number)}{@render board(b)}{/each}</div>
</div>

<style>
  .ex {
    display: flex;
    flex-direction: column;
    gap: 6px;
    width: 520px;
    max-width: 100%;
  }
  header {
    display: flex;
    align-items: baseline;
    justify-content: space-between;
  }
  h3,
  h4 {
    margin: 0;
    font: 600 11px var(--font-caps);
    letter-spacing: 0.08em;
    text-transform: uppercase;
    color: var(--text);
  }
  h4 {
    margin-top: 4px;
    color: var(--text-dim);
  }
  .count {
    font: 600 10.5px var(--font-caps);
    color: var(--text-dim);
  }
  .count.full {
    color: var(--accent);
  }
  .dim {
    margin: 0;
    color: var(--text-dim);
    font-size: 10.5px;
    line-height: 1.35;
  }
  .grid {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 4px;
  }
  .b {
    display: grid;
    grid-template-columns: 10px auto 1fr auto;
    align-items: center;
    column-gap: 7px;
    padding: 4px 8px;
    text-align: left;
    background: var(--body-deep);
    color: var(--text-dim);
    border: 1px solid var(--edge);
    border-radius: var(--radius);
    cursor: pointer;
    font: inherit;
    font-size: 11px;
  }
  .b:hover:not(:disabled) {
    background: var(--well-deep);
    color: var(--text);
  }
  .b:disabled {
    opacity: 0.4;
    cursor: default;
  }
  .b:focus-visible {
    outline: none;
    background-image: var(--focus-fill);
  }
  .b.on {
    background: color-mix(in srgb, var(--accent) 14%, var(--body-deep));
    color: var(--text);
  }
  i {
    width: 8px;
    height: 8px;
    border-radius: 50%;
    background: var(--edge-light);
    box-shadow: inset 0 0 0 1px rgba(0, 0, 0, 0.4);
  }
  .b.on i {
    background: var(--accent);
    box-shadow: 0 0 5px color-mix(in srgb, var(--accent) 70%, transparent);
  }
  .n {
    font: 10px var(--font-mono);
    color: var(--text-faint);
  }
  .b.on .n {
    color: var(--text-dim);
  }
  .t {
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }
  .c {
    font-size: 9.5px;
    color: var(--text-faint);
  }
</style>
