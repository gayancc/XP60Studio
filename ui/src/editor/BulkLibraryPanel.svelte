<script lang="ts">
  import { onMount } from 'svelte';
  import { Segmented } from '../kit';
  import { DRAG_TYPE, type DragPayload } from '../model/bulks.svelte';
  import type { BankService } from '../model/bank.svelte';
  import { patchNumber } from '../model/bankPanel';
  import type { LibraryService } from '../model/library.svelte';
  import BulkSaveBar from './BulkSaveBar.svelte';
  import BulkTree from './BulkTree.svelte';

  // The Bank workspace's Library tab: the bank's place in the library (saving it), then the library itself, either as bulks (each bank file
  // under its name, opened to its patches) or as one flat list of every patch.
  interface Props {
    bank: BankService;
    lib: LibraryService;
    deviceId: number;
  }

  let { bank, lib, deviceId }: Props = $props();

  let view = $state(0);
  let query = $state('');
  let first = true;

  onMount(() => {
    void lib.refresh();
    void lib.bulks.refresh();
  });

  // The search runs a moment after typing stops, in whichever view is showing.
  $effect(() => {
    const text = query;
    const v = view;
    if (first) {
      first = false;
      return;
    }
    const timer = setTimeout(() => {
      if (v === 0) void lib.bulks.search(text);
      else {
        lib.text = text;
        void lib.refresh();
      }
    }, 250);
    return () => clearTimeout(timer);
  });

  async function putInBank(id: number): Promise<void> {
    if (bank.locked) return;
    const bytes = await lib.fetchBlocks(id);
    if (bytes) bank.assign(bank.selected, bytes, 'library');
  }

  function dragFlat(e: DragEvent, id: number): void {
    e.dataTransfer?.setData(DRAG_TYPE, JSON.stringify({ type: 'patches', items: [{ kind: 'patch', patchId: id }] } satisfies DragPayload));
    e.dataTransfer?.setData('text/xp-library', String(id));
    if (e.dataTransfer) e.dataTransfer.effectAllowed = 'copyMove';
  }
</script>

<div class="panel">
  <BulkSaveBar {bank} {lib} />
  <Segmented options={['Bulks', 'All patches']} bind:selected={view} name="Library view" context="Library" />
  <input type="search" placeholder={view === 0 ? 'Search patches in all bulks' : 'Search names'} aria-label="Search the library" bind:value={query} />
  {#if view === 0}
    <BulkTree {bank} {lib} {deviceId} />
  {:else}
    <ul class="rows" aria-label="Library patches">
      {#each lib.rows as r (r.id)}
        <li>
          <button
            type="button"
            class="lrow"
            draggable="true"
            ondragstart={(e) => dragFlat(e, r.id)}
            onclick={() => putInBank(r.id)}
            disabled={bank.locked}
            title="Put in slot {patchNumber(bank.selected)}"
          >
            <span>{r.name.trimEnd() || '(unnamed)'}</span>
            <span class="dim">→ {patchNumber(bank.selected)}</span>
          </button>
        </li>
      {:else}
        <li class="dim">No library patches match.</li>
      {/each}
    </ul>
  {/if}
</div>

<style>
  .panel {
    display: flex;
    flex-direction: column;
    gap: 6px;
    min-width: 0;
  }
  input[type='search'] {
    width: 100%;
    background: var(--track);
    color: var(--text);
    border: 1px solid var(--edge);
    border-radius: var(--radius);
    padding: 4px 6px;
    /* the tab body scrolls; the search stays in view above the list */
    position: sticky;
    top: 0;
    z-index: 1;
  }
  .rows {
    list-style: none;
    margin: 0;
    padding: 0;
  }
  .lrow {
    display: flex;
    justify-content: space-between;
    width: 100%;
    background: transparent;
    border: 0;
    text-align: left;
    color: inherit;
    font: inherit;
    cursor: pointer;
    padding: 2px 6px;
  }
  .lrow:hover:not(:disabled) {
    background: var(--well);
  }
  .lrow:focus-visible {
    outline: none;
    background-image: var(--focus-fill);
  }
  .dim {
    color: var(--text-dim);
    font-size: 11px;
  }
</style>
