<script lang="ts">
  import { bulkNameFrom } from '../model/bulks.svelte';
  import type { BankService } from '../model/bank.svelte';
  import type { LibraryService } from '../model/library.svelte';

  // Where the bank stands in the library: which bulk it was loaded from or saved as, whether it has changed since, and the two ways to keep it:
  // save it as a new bulk, or save it over the bulk it came from (whose old contents are kept as a version). Asking is done here, in place.
  interface Props {
    bank: BankService;
    lib: LibraryService;
  }

  let { bank, lib }: Props = $props();
  const bulks = $derived(lib.bulks);

  let mode = $state<'idle' | 'over' | 'new'>('idle');
  let name = $state('');

  const origin = $derived(bank.origin);
  const unsaved = $derived(bank.unsaved.length);
  const hasPatches = $derived(bank.slots.some((s) => s.bytes !== null));
  const originGone = $derived(origin !== null && !bulks.bulk(origin.id));

  function askNew(): void {
    name = bulks.freeName(origin ? `${origin.name} 2` : bulkNameFrom(bank.source));
    mode = 'new';
  }

  async function saveNew(): Promise<void> {
    if (await bulks.saveBank(bank, name.trim(), null)) mode = 'idle';
  }

  async function saveOver(): Promise<void> {
    if (!origin) return;
    if (await bulks.saveBank(bank, bulks.bulk(origin.id)?.name ?? origin.name, origin.id)) mode = 'idle';
  }
</script>

<div class="bar" aria-label="The bank in the library">
  <div class="line">
    <span class="k">Bank</span>
    <span class="v">{origin ? origin.name : 'not in the library'}</span>
    {#if origin && unsaved > 0}
      <span class="chg" title="Slots that differ from the saved bulk">modified · {unsaved}</span>
    {:else if !origin && hasPatches}
      <span class="chg">unsaved</span>
    {/if}
  </div>
  <div class="actions">
    <button type="button" disabled={!origin || originGone || unsaved === 0 || bank.locked} onclick={() => (mode = 'over')} title="Save the bank over the bulk it came from">
      Save
    </button>
    <button type="button" disabled={!hasPatches} onclick={askNew} title="Keep the bank as a new bulk in the library">Save as new bulk…</button>
  </div>
  {#if mode === 'over' && origin}
    <p class="ask" role="alert">
      Save over <b>{origin.name}</b>? {unsaved} slot{unsaved === 1 ? '' : 's'} changed. The previous contents are kept as a version you can restore.
      <button type="button" class="go" onclick={saveOver} disabled={bulks.busy}>Save over</button>
      <button type="button" onclick={() => (mode = 'idle')}>Cancel</button>
    </p>
  {:else if mode === 'new'}
    <p class="ask">
      <label>
        New bulk name
        <!-- svelte-ignore a11y_autofocus -->
        <input
          bind:value={name}
          maxlength="60"
          autofocus
          onkeydown={(e) => {
            if (e.key === 'Enter' && name.trim() !== '' && !bulks.busy) void saveNew();
            else if (e.key === 'Escape') mode = 'idle';
          }}
        />
      </label>
      <button type="button" class="go" onclick={saveNew} disabled={bulks.busy || name.trim() === ''}>Save</button>
      <button type="button" onclick={() => (mode = 'idle')}>Cancel</button>
    </p>
  {/if}
</div>

<style>
  .bar {
    display: flex;
    flex-direction: column;
    gap: 4px;
    padding: 6px;
    background: var(--well-deep);
    border-radius: var(--radius);
  }
  .line {
    display: flex;
    align-items: baseline;
    gap: 8px;
  }
  .k {
    font: 600 11px var(--font-caps);
    letter-spacing: 0.08em;
    text-transform: uppercase;
    color: var(--text-dim);
  }
  .v {
    flex: 1;
    min-width: 0;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
    font: 600 13px var(--font);
  }
  .chg {
    font: 600 11px var(--font-caps);
    letter-spacing: 0.06em;
    text-transform: uppercase;
    color: var(--warn);
  }
  .actions {
    display: flex;
    gap: 6px;
    flex-wrap: wrap;
  }
  .ask {
    margin: 0;
    padding: 4px 6px;
    background: color-mix(in srgb, var(--warn) 14%, var(--track));
    border-radius: var(--radius);
    font-size: 13px;
    display: flex;
    flex-wrap: wrap;
    gap: 6px;
    align-items: center;
  }
  label {
    display: flex;
    align-items: center;
    gap: 6px;
    flex: 1;
  }
  input {
    flex: 1;
    min-width: 0;
    background: var(--track);
    color: var(--text);
    border: 1px solid var(--edge);
    border-radius: var(--radius);
    padding: 2px 6px;
    font: inherit;
  }
  button {
    background: var(--body-deep);
    border: 1px solid var(--edge);
    border-radius: var(--radius);
    padding: 3px 10px;
    cursor: pointer;
    color: inherit;
    font: 400 12px var(--font);
  }
  button:hover:not(:disabled) {
    background: var(--well);
  }
  button:focus-visible {
    outline: none;
    background-image: var(--focus-fill);
  }
  button:disabled {
    opacity: 0.4;
    cursor: default;
  }
  .go {
    background: var(--accent);
    color: var(--accent-ink);
    border-color: var(--accent-soft);
  }
</style>
