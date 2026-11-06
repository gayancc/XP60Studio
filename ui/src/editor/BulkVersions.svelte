<script lang="ts">
  import type { BulkRevision } from '../model/backend';

  // The earlier contents a bulk kept when it was saved over or edited, newest first. Restoring one keeps the current contents as a version, so a
  // restore can be undone too.
  interface Props {
    revisions: readonly BulkRevision[];
    when: (secs: number) => string;
    onrestore: (revision: number) => void;
  }

  let { revisions, when, onrestore }: Props = $props();
</script>

<ul class="versions" aria-label="Earlier versions">
  {#each revisions as r (r.id)}
    <li>
      <span>{when(r.saved_at) || 'earlier'} · {r.count} patch{r.count === 1 ? '' : 'es'}</span>
      <button type="button" onclick={() => onrestore(r.id)}>Restore</button>
    </li>
  {:else}
    <li class="none">No earlier versions yet. One is kept each time the bulk is saved over or edited.</li>
  {/each}
</ul>

<style>
  .versions {
    list-style: none;
    margin: 2px 0 4px 24px;
    padding: 4px 6px;
    background: var(--track);
    border-radius: var(--radius);
    font-size: 11px;
  }
  li {
    display: flex;
    justify-content: space-between;
    align-items: center;
    gap: 8px;
    padding: 1px 0;
  }
  .none {
    color: var(--text-dim);
  }
  button {
    background: var(--well-deep);
    border: 1px solid var(--edge);
    border-radius: var(--radius);
    padding: 1px 8px;
    cursor: pointer;
    color: inherit;
    font: inherit;
  }
  button:hover {
    background: var(--well);
  }
  button:focus-visible {
    outline: none;
    background-image: var(--focus-fill);
  }
</style>
