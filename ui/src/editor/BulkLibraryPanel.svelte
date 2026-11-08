<script lang="ts">
  import { onMount } from 'svelte';
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
    /** After a library patch is put in a bank slot, re-select that slot so the pad and instrument follow it. */
    onput?: (slot: number) => void;
  }

  let { bank, lib, deviceId, onput }: Props = $props();

  let view = $state(0);
  let query = $state('');
  let first = true;
  let fileInput: HTMLInputElement | undefined = $state();

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

  async function pickFile(e: Event & { currentTarget: HTMLInputElement }): Promise<void> {
    const file = e.currentTarget.files?.[0];
    e.currentTarget.value = '';
    if (!file) return;
    try {
      lib.stage(file.name, new Uint8Array(await file.arrayBuffer()));
    } catch (err) {
      lib.error = err instanceof Error ? err.message : String(err);
    }
  }

  async function putInBank(id: number): Promise<void> {
    if (bank.locked) return;
    const slot = bank.selected;
    const bytes = await lib.fetchBlocks(id);
    if (bytes && bank.assign(slot, bytes, 'library')) onput?.(slot);
  }

  function dragFlat(e: DragEvent, id: number): void {
    e.dataTransfer?.setData(DRAG_TYPE, JSON.stringify({ type: 'patches', items: [{ kind: 'patch', patchId: id }] } satisfies DragPayload));
    e.dataTransfer?.setData('text/xp-library', String(id));
    if (e.dataTransfer) e.dataTransfer.effectAllowed = 'copyMove';
  }
</script>

<div class="panel">
  <BulkSaveBar {bank} {lib} />
  <!-- FL Browser shell: caption #2c353a, deep well #131c21, flat tree (measured 2026-11-07). -->
  <div class="browser">
    <div class="chrome">
      <span class="title">Browser</span>
      <div class="tabs" role="tablist" aria-label="Library view">
        <button type="button" class="tab" class:on={view === 0} role="tab" aria-selected={view === 0} onclick={() => (view = 0)}>Bulks</button>
        <button type="button" class="tab" class:on={view === 1} role="tab" aria-selected={view === 1} onclick={() => (view = 1)}>All</button>
      </div>
      <button type="button" class="import" onclick={() => fileInput?.click()} disabled={lib.busy} title="Import a .syx or .mid bank file into the library as a bulk">
        Import…
      </button>
      <input bind:this={fileInput} type="file" accept=".syx,.mid,.midi" hidden onchange={pickFile} aria-label="Import a .syx or .mid bulk file" />
    </div>
    {#if lib.staged}
      <div class="staged" role="group" aria-label="File to import">
        <p>
          <b>{lib.staged.name}</b> holds {lib.staged.names.length} patch{lib.staged.names.length === 1 ? '' : 'es'}:
          {lib.staged.names
            .slice(0, 8)
            .map(([, n]) => n.trimEnd() || '(unnamed)')
            .join(', ')}{lib.staged.names.length > 8 ? ', …' : ''}
        </p>
        <div class="actions">
          <button type="button" class="primary" disabled={lib.busy} onclick={() => lib.importStaged()}>
            Import {lib.staged.names.length} as bulk
          </button>
          <button type="button" onclick={() => lib.cancelStaged()}>Cancel</button>
        </div>
      </div>
    {/if}
    {#if lib.error}
      <p class="err" role="alert">{lib.error}</p>
    {:else if lib.notice}
      <p class="ok" role="status">{lib.notice}</p>
    {/if}
    <input type="search" placeholder={view === 0 ? 'Search patches in all bulks' : 'Search names'} aria-label="Search the library" bind:value={query} />
    {#if view === 0}
      <BulkTree {bank} {lib} {deviceId} {onput} />
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
              <svg class="file-ico" viewBox="0 0 10 12" width="10" height="12" aria-hidden="true">
                <path d="M1.5 1h4.2L8.5 4.3V11H1.5z" fill="none" stroke="currentColor" stroke-width="1" />
                <path d="M5.7 1v3.3H8.5" fill="none" stroke="currentColor" stroke-width="1" />
              </svg>
              <span class="nm">{r.name.trimEnd() || '(unnamed)'}</span>
              <span class="dim">→ {patchNumber(bank.selected)}</span>
            </button>
          </li>
        {:else}
          <li class="empty">No library patches match.</li>
        {/each}
      </ul>
    {/if}
  </div>
</div>

<style>
  .panel {
    display: flex;
    flex-direction: column;
    gap: 6px;
    min-width: 0;
    min-height: 0;
    flex: 1;
  }
  .browser {
    --br-bg: #131c21;
    --br-cap: #2c353a;
    --br-hover: #1a2328;
    --br-sel: #242d32;
    --br-edge: #0d1418;
    --br-file: #b8c2c8;

    display: flex;
    flex-direction: column;
    min-width: 0;
    min-height: 0;
    flex: 1;
    background: var(--br-bg);
    border: 1px solid var(--br-edge);
    overflow: hidden;
  }
  .chrome {
    display: flex;
    align-items: center;
    gap: 6px;
    height: 26px;
    padding: 0 8px;
    background: var(--br-cap);
    border-bottom: 1px solid var(--br-edge);
    flex: none;
  }
  .title {
    font: 600 13px var(--font);
    color: #a8b2b8;
    letter-spacing: 0.02em;
  }
  .tabs {
    display: flex;
    gap: 1px;
    margin-left: 4px;
  }
  .tab {
    height: 20px;
    padding: 0 8px;
    border: 1px solid transparent;
    border-radius: 1px;
    background: transparent;
    color: #7a868e;
    font: 400 12px var(--font);
    cursor: pointer;
  }
  .tab:hover {
    color: var(--br-file);
    background: #243038;
  }
  .tab.on {
    color: var(--accent);
    border-color: color-mix(in srgb, var(--accent) 55%, #2c353a);
    background: color-mix(in srgb, var(--accent) 12%, #2c353a);
  }
  .import {
    margin-left: auto;
    height: 20px;
    padding: 0 8px;
    border: 1px solid #1e282e;
    border-radius: 1px;
    background: #1a2328;
    color: #9aa4aa;
    font: 400 12px var(--font);
    cursor: pointer;
  }
  .import:hover:not(:disabled) {
    background: #242d32;
    color: #e8eef2;
  }
  .import:focus-visible {
    outline: none;
    background-image: var(--focus-fill);
  }
  .import:disabled {
    opacity: 0.4;
    cursor: default;
  }
  .actions button {
    height: 22px;
    padding: 0 9px;
    border: 1px solid #1e282e;
    border-radius: 1px;
    background: #1a2328;
    color: #9aa4aa;
    font: 400 12px var(--font);
    cursor: pointer;
  }
  .actions button:hover:not(:disabled) {
    background: #242d32;
    color: #e8eef2;
  }
  .actions button:focus-visible {
    outline: none;
    background-image: var(--focus-fill);
  }
  .actions button:disabled {
    opacity: 0.4;
    cursor: default;
  }
  .primary {
    background: color-mix(in srgb, var(--accent) 35%, #1a2328) !important;
    color: #e8eef2 !important;
    border-color: var(--accent-soft) !important;
  }
  .staged {
    padding: 6px 8px;
    border-bottom: 1px solid var(--br-edge);
    background: #151e23;
  }
  .staged p {
    margin: 0 0 6px;
    font-size: 13px;
    color: #8b969c;
  }
  .staged b {
    color: var(--br-file);
  }
  .actions {
    display: flex;
    gap: 4px;
  }
  .err {
    margin: 0;
    padding: 4px 8px;
    color: var(--bad);
    font-size: 13px;
  }
  .ok {
    margin: 0;
    padding: 4px 8px;
    color: var(--ok);
    font-size: 13px;
  }
  input[type='search'] {
    width: 100%;
    height: 26px;
    background: #0f161a;
    color: var(--br-file);
    border: 0;
    border-bottom: 1px solid var(--br-edge);
    border-radius: 0;
    padding: 0 8px;
    font: 13px var(--font);
    flex: none;
    position: sticky;
    top: 0;
    z-index: 1;
  }
  input[type='search']::placeholder {
    color: #5a666e;
  }
  .rows {
    list-style: none;
    margin: 0;
    padding: 2px 0;
    flex: 1;
    min-height: 0;
    overflow: auto;
  }
  .lrow {
    display: flex;
    align-items: center;
    gap: 5px;
    width: 100%;
    height: 22px;
    background: transparent;
    border: 0;
    text-align: left;
    color: var(--br-file);
    font: 400 13px/22px var(--font);
    cursor: pointer;
    padding: 0 6px 0 8px;
  }
  .lrow:hover:not(:disabled) {
    background: var(--br-hover);
  }
  .lrow:focus-visible {
    outline: none;
    background-image: var(--focus-fill);
  }
  .lrow:disabled {
    opacity: 0.4;
    cursor: default;
  }
  .file-ico {
    flex: none;
    color: #8a949a;
  }
  .nm {
    flex: 1;
    min-width: 0;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }
  .dim {
    flex: none;
    color: #5a666e;
    font: 11px var(--font-mono);
  }
  .empty {
    color: #66727a;
    font-size: 12px;
    padding: 5px 8px;
  }
</style>
