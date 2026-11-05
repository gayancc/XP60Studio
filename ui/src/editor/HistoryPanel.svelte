<script lang="ts">
  import { useStore } from './context';

  const store = useStore();
</script>

<ol class="history" aria-label="Undo history">
  <li class:current={store.position === 0}>
    <button type="button" onclick={() => store.jumpTo(0)}>
      <span class="n">0</span><span class="l">Opened: {store.origin}</span>
    </button>
  </li>
  {#each store.items as item, i (i)}
    <li class:current={store.position === i + 1} class:undone={i + 1 > store.position}>
      <button type="button" onclick={() => store.jumpTo(i + 1)}>
        <span class="n">{i + 1}</span><span class="l">{item.label}</span>
      </button>
    </li>
  {/each}
</ol>

<style>
  .history {
    list-style: none;
    margin: 0;
    padding: 0;
    max-height: 190px;
    overflow: auto;
    display: flex;
    flex-direction: column-reverse;
    gap: 1px;
  }
  button {
    display: flex;
    gap: 8px;
    width: 100%;
    padding: 2px 6px;
    text-align: left;
    background: transparent;
    border: 0;
    border-radius: 2px;
    cursor: pointer;
    color: var(--text);
  }
  button:hover {
    background: var(--caption);
  }
  button:focus-visible {
    outline: none;
    background-image: var(--focus-fill);
  }
  .current button {
    background: var(--accent);
    color: var(--accent-ink);
  }
  .undone button {
    color: var(--text-faint);
    font-style: italic;
  }
  .n {
    width: 24px;
    color: var(--text-dim);
    font-family: var(--font-mono);
    font-size: 11px;
  }
  .current .n {
    color: var(--accent-ink);
  }
  .l {
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }
</style>
