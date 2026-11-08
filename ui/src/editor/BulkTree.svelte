<script lang="ts">
  import { DRAG_TYPE, parsePayload, type DragItem, type DragPayload, type DropTarget } from '../model/bulks.svelte';
  import type { BankService } from '../model/bank.svelte';
  import { panelLabel, patchNumber } from '../model/bankPanel';
  import type { LibraryService } from '../model/library.svelte';
  import { reportInfo } from '../model/messages.svelte';
  import { saveFile } from '../model/saveFile';
  import { say } from '../model/ui.svelte';
  import BulkVersions from './BulkVersions.svelte';

  // The library as bulks: each bank file kept under its name, opened to show its patches in order. Patches are dragged between bulks, to a
  // new bulk and to and from the bank; the place a drop would land is tinted while the pointer moves, before anything changes.
  interface Props {
    bank: BankService;
    lib: LibraryService;
    /** The device ID exported bank files are addressed to. */
    deviceId: number;
    /** After a library patch is put in a bank slot, re-select that slot so the pad and instrument follow it. */
    onput?: (slot: number) => void;
  }

  let { bank, lib, deviceId, onput }: Props = $props();
  const bulks = $derived(lib.bulks);

  /** What a drop would do right now, as a key for the tint and a sentence for the hint line. */
  let guide = $state<{ key: string; text: string } | null>(null);
  let confirmDelete = $state<number | null>(null);
  let versionsOf = $state<number | null>(null);
  let renameText = $state('');

  const rowKey = (bulk: number, position: number): string => `slot:${bulk}:${position}`;
  const positionLabel = (p: number): string => `${panelLabel(p)} · ${patchNumber(p)}`;

  function accepts(e: DragEvent): boolean {
    const t = e.dataTransfer?.types ?? [];
    return t.includes(DRAG_TYPE) || t.includes('text/xp-slot') || t.includes('text/xp-library');
  }

  /** What was dragged, from whichever of our drag types it carries. */
  function payloadOf(e: DragEvent): DragPayload | null {
    const d = e.dataTransfer;
    const own = parsePayload(d?.getData(DRAG_TYPE));
    if (own) return own;
    const slot = d?.getData('text/xp-slot');
    if (slot !== undefined && slot !== '') return { type: 'patches', items: [{ kind: 'bank-slot', slot: Number(slot) }] };
    const id = d?.getData('text/xp-library');
    if (id) return { type: 'patches', items: [{ kind: 'patch', patchId: Number(id) }] };
    return null;
  }

  function itemsFor(bulk: number, position: number, patchId: number): DragItem[] {
    const picked = bulks.selection;
    if (picked.length > 1 && bulks.isSelected(bulk, position)) {
      return [...picked].sort((a, b) => a.bulk - b.bulk || a.position - b.position).map((s) => ({ kind: 'bulk-slot', bulk: s.bulk, position: s.position, patchId: s.patchId }));
    }
    return [{ kind: 'bulk-slot', bulk, position, patchId }];
  }

  function dragPatch(e: DragEvent, bulk: number, position: number, patchId: number): void {
    const items = itemsFor(bulk, position, patchId);
    e.dataTransfer?.setData(DRAG_TYPE, JSON.stringify({ type: 'patches', items } satisfies DragPayload));
    if (items.length === 1) e.dataTransfer?.setData('text/xp-library', String(patchId));
    if (e.dataTransfer) e.dataTransfer.effectAllowed = 'copyMove';
  }

  function dragBulk(e: DragEvent, id: number): void {
    e.dataTransfer?.setData(DRAG_TYPE, JSON.stringify({ type: 'bulk', bulk: id } satisfies DragPayload));
    if (e.dataTransfer) e.dataTransfer.effectAllowed = 'copyMove';
  }

  function over(e: DragEvent, key: string, text: string): void {
    if (!accepts(e)) return;
    e.preventDefault();
    if (e.dataTransfer) e.dataTransfer.dropEffect = e.altKey ? 'copy' : 'move';
    if (guide?.key !== key || guide.text !== text) guide = { key, text };
  }

  function leave(e: DragEvent, key: string): void {
    if (guide?.key === key && !(e.currentTarget as Node).contains(e.relatedTarget as Node | null)) guide = null;
  }

  async function drop(e: DragEvent, target: DropTarget): Promise<void> {
    e.preventDefault();
    guide = null;
    const payload = payloadOf(e);
    if (!payload) return;
    const message = await bulks.drop(bank, target, payload, e.altKey);
    if (message) reportInfo(message);
  }

  async function putInBank(patchId: number): Promise<void> {
    if (bank.locked) return;
    const slot = bank.selected;
    const bytes = await lib.fetchBlocks(patchId);
    if (bytes && bank.assign(slot, bytes, 'library')) onput?.(slot);
  }

  function rowClick(e: MouseEvent, bulk: number, position: number, patchId: number): void {
    if (e.ctrlKey || e.metaKey || e.shiftKey) bulks.toggleSelected(bulk, position, patchId);
    else void putInBank(patchId);
  }

  async function exportBulk(id: number, name: string): Promise<void> {
    const bytes = await bulks.exportBulk(id, deviceId);
    if (bytes) say(await saveFile(`${name}.syx`, bytes));
  }

  function startRename(id: number, name: string): void {
    renameText = name;
    bulks.renaming = id;
  }

  async function finishRename(id: number): Promise<void> {
    if (bulks.renaming !== id) return;
    const ok = await bulks.rename(id, renameText);
    if (!ok) bulks.renaming = id; // keep the box open so the name can be fixed
  }

  const when = (secs: number): string => (secs > 0 ? new Date(secs * 1000).toLocaleString() : '');

  // A bulk made by a drop opens its name box straight away.
  $effect(() => {
    const id = bulks.renaming;
    if (id !== null) renameText = bulks.bulk(id)?.name ?? '';
  });

  async function toggleVersions(id: number): Promise<void> {
    if (versionsOf === id) {
      versionsOf = null;
      return;
    }
    await bulks.loadRevisions(id);
    versionsOf = id;
  }
</script>

{#snippet fileIco()}
  <svg class="file-ico" viewBox="0 0 10 12" width="10" height="12" aria-hidden="true">
    <path d="M1.5 1h4.2L8.5 4.3V11H1.5z" fill="none" stroke="currentColor" stroke-width="1" />
    <path d="M5.7 1v3.3H8.5" fill="none" stroke="currentColor" stroke-width="1" />
  </svg>
{/snippet}

{#snippet folderIco(open: boolean, live: boolean)}
  <svg class="folder-ico" class:live class:open aria-hidden="true" viewBox="0 0 13 11" width="13" height="11">
    {#if open}
      <path d="M1 3.2h3.2l1.1 1.4H12v5.2H1z" fill="currentColor" fill-opacity="0.18" stroke="currentColor" stroke-width="1" />
      <path d="M1 3.2V2.2h2.6l.7 1" fill="none" stroke="currentColor" stroke-width="1" />
    {:else}
      <path d="M1 3.2h3.2l1.1 1.4H12v5.2H1z" fill="none" stroke="currentColor" stroke-width="1" />
      <path d="M1 3.2V2.2h2.6l.7 1" fill="none" stroke="currentColor" stroke-width="1" />
    {/if}
  </svg>
{/snippet}

{#snippet patchRow(bulk: number, position: number, patchId: number, name: string)}
  <li>
    <button
      type="button"
      class="prow"
      class:sel={bulks.isSelected(bulk, position)}
      class:target={guide?.key === rowKey(bulk, position)}
      draggable="true"
      ondragstart={(e) => dragPatch(e, bulk, position, patchId)}
      ondragend={() => (guide = null)}
      ondragenter={(e) => over(e, rowKey(bulk, position), `Drop to put the patch at ${positionLabel(position)} of ${bulks.bulk(bulk)?.name ?? 'the bulk'} (Alt copies).`)}
      ondragover={(e) => over(e, rowKey(bulk, position), `Drop to put the patch at ${positionLabel(position)} of ${bulks.bulk(bulk)?.name ?? 'the bulk'} (Alt copies).`)}
      ondragleave={(e) => leave(e, rowKey(bulk, position))}
      ondrop={(e) => drop(e, { kind: 'bulk-slot', bulk, position })}
      onclick={(e) => rowClick(e, bulk, position, patchId)}
      title="Click puts it in bank slot {patchNumber(bank.selected)}. Ctrl-click picks several to drag together."
    >
      {@render fileIco()}
      <span class="nm">{name.trimEnd() || '(unnamed)'}</span>
      <span class="pos">{positionLabel(position)}</span>
    </button>
  </li>
{/snippet}

<div class="tree">
  {#if bulks.query.trim() !== ''}
    <ul class="hits" aria-label="Search results">
      {#each bulks.hits as h (`${h.bulk_id}:${h.position}`)}
        <li class="hit">
          <button type="button" class="prow" draggable="true" ondragstart={(e) => dragPatch(e, h.bulk_id, h.position, h.patch_id)} onclick={() => putInBank(h.patch_id)}>
            <span class="pos">{h.bulk_name}</span>
            <span class="nm">{h.name.trimEnd()}</span>
            <span class="pos">{positionLabel(h.position)}</span>
          </button>
        </li>
      {:else}
        <li class="empty">No patch in any bulk matches.</li>
      {/each}
    </ul>
  {:else}
    <ul class="bulks" aria-label="Bulks">
      {#each bulks.bulks as b (b.id)}
        <li class="bulk" class:open={bulks.open[b.id]}>
          <div
            class="head"
            class:loaded={bank.origin?.id === b.id}
            class:target={guide?.key === `bulk:${b.id}`}
            role="group"
            aria-label="Bulk {b.name}"
            draggable="true"
            ondragstart={(e) => dragBulk(e, b.id)}
            ondragenter={(e) => over(e, `bulk:${b.id}`, `Drop to add the patch at the end of ${b.name} (Alt copies).`)}
            ondragover={(e) => over(e, `bulk:${b.id}`, `Drop to add the patch at the end of ${b.name} (Alt copies).`)}
            ondragleave={(e) => leave(e, `bulk:${b.id}`)}
            ondrop={(e) => drop(e, { kind: 'bulk', bulk: b.id })}
          >
            <button type="button" class="chev" aria-expanded={!!bulks.open[b.id]} aria-label="{bulks.open[b.id] ? 'Close' : 'Open'} {b.name}" onclick={() => bulks.toggle(b.id)}>{bulks.open[b.id] ? '▾' : '▸'}</button>
            {@render folderIco(!!bulks.open[b.id], bank.origin?.id === b.id)}
            {#if bulks.renaming === b.id}
              <!-- svelte-ignore a11y_autofocus -->
              <input
                class="rename"
                aria-label="Bulk name"
                bind:value={renameText}
                autofocus
                onkeydown={(e) => {
                  if (e.key === 'Enter') void finishRename(b.id);
                  else if (e.key === 'Escape') bulks.renaming = null;
                }}
                onblur={() => void finishRename(b.id)}
              />
            {:else}
              <button type="button" class="name" onclick={() => bulks.toggle(b.id)} title="Drag the bulk to the new-bulk area to copy it">{b.name}</button>
            {/if}
            {#if bank.origin?.id === b.id}<span class="tag">in bank</span>{/if}
            <span class="count">{b.count}</span>
          </div>

          {#if bulks.open[b.id]}
            <div class="tools">
              <button type="button" class="tool" onclick={() => (bulks.pendingLoad = b.id)} disabled={bank.locked}>Load into bank</button>
              <button type="button" class="tool" onclick={() => startRename(b.id, b.name)}>Rename</button>
              <button type="button" class="tool" onclick={() => bulks.duplicate(b.id)}>Copy</button>
              <button type="button" class="tool" onclick={() => exportBulk(b.id, b.name)}>Export .syx</button>
              <button type="button" class="tool" onclick={() => toggleVersions(b.id)} aria-pressed={versionsOf === b.id}>Versions</button>
              <button type="button" class="tool danger" onclick={() => (confirmDelete = b.id)}>Delete</button>
            </div>
            {#if bulks.pendingLoad === b.id}
              <p class="ask" role="alert">
                Replace the bank with {b.name}?{bank.unsaved.length > 0 ? ' Unsaved changes in the bank are lost.' : ''}
                <button type="button" class="go" onclick={async () => { await bulks.loadIntoBank(bank, b.id); bulks.pendingLoad = null; }}>Replace</button>
                <button type="button" onclick={() => (bulks.pendingLoad = null)}>Cancel</button>
              </p>
            {/if}
            {#if confirmDelete === b.id}
              <p class="ask" role="alert">
                Delete {b.name}? Patches that only it holds are deleted with it.
                <button type="button" class="danger" onclick={async () => { confirmDelete = null; await bulks.remove(b.id); }}>Delete</button>
                <button type="button" onclick={() => (confirmDelete = null)}>Keep</button>
              </p>
            {/if}
            {#if versionsOf === b.id}
              <BulkVersions revisions={bulks.revisions[b.id] ?? []} {when} onrestore={(rev) => bulks.restore(b.id, rev)} />
            {/if}
            <ul class="slots" aria-label="Patches of {b.name}">
              {#each bulks.contents[b.id] ?? [] as s (s.position)}
                {@render patchRow(b.id, s.position, s.patch_id, s.name)}
              {:else}
                <li class="empty">Empty. Drop patches on the bulk to fill it.</li>
              {/each}
            </ul>
          {/if}
        </li>
      {:else}
        <li class="empty">No bulks yet. Import a .syx / .mid file above, or save the bank here.</li>
      {/each}
      <li
        class="newzone"
        class:target={guide?.key === 'new'}
        ondragenter={(e) => over(e, 'new', 'Drop to make a new bulk from what you drag (a bulk dropped here is copied).')}
        ondragover={(e) => over(e, 'new', 'Drop to make a new bulk from what you drag (a bulk dropped here is copied).')}
        ondragleave={(e) => leave(e, 'new')}
        ondrop={(e) => drop(e, { kind: 'new' })}
      >
        Drop here to make a new bulk
      </li>
    </ul>
  {/if}
  <p class="hint" aria-live="polite">
    {#if guide}
      {guide.text}
    {:else}
      Drag a patch onto a bank slot, another bulk or the new-bulk area. Alt copies · Ctrl-click multi-select.
    {/if}
  </p>
</div>

<style>
  /*
   * FL Studio Browser tree — measured from live capture 2026-11-07
   * (artifact/docs/xppatchstudio/fl-live + owner screenshot):
   *   well #131c21 · hover #1a2328 · selection wash #242d32
   *   category blue #7a9bb0 · live/current tan #c4a882 · file text #b8c2c8
   *   ~18 px rows, wireframe folder / page glyphs, flat hover (no bevel, glow, pulse).
   */
  .tree {
    --br-bg: #131c21;
    --br-hover: #1a2328;
    --br-sel: #242d32;
    --br-edge: #0d1418;
    --br-folder: #7a9bb0;
    --br-folder-live: #c4a882;
    --br-file: #b8c2c8;
    --br-file-sel: #e8eef2;
    --br-guide: #1e2a30;

    display: flex;
    flex-direction: column;
    gap: 0;
    min-width: 0;
    flex: 1;
    padding: 2px 0 4px;
    background: var(--br-bg);
    color: var(--br-file);
  }
  ul {
    list-style: none;
    margin: 0;
    padding: 0;
  }
  .bulk {
    margin: 0;
  }
  .head {
    display: flex;
    align-items: center;
    gap: 4px;
    min-height: 18px;
    height: 18px;
    padding: 0 6px 0 2px;
    background: transparent;
  }
  .head:hover {
    background: var(--br-hover);
  }
  .head.loaded .name,
  .head.loaded .folder-ico {
    color: var(--br-folder-live);
  }
  .head.target,
  .newzone.target,
  .prow.target {
    background: color-mix(in srgb, var(--accent) 22%, var(--br-sel));
  }
  .chev {
    flex: none;
    width: 12px;
    height: 18px;
    padding: 0;
    border: 0;
    background: transparent;
    color: #5a666e;
    font-size: 8px;
    line-height: 18px;
    text-align: center;
    cursor: pointer;
  }
  .folder-ico,
  .file-ico {
    flex: none;
    display: block;
    margin-right: 1px;
    color: var(--br-folder);
  }
  .folder-ico.live {
    color: var(--br-folder-live);
  }
  .file-ico {
    color: #8a949a;
  }
  .name {
    flex: 1;
    min-width: 0;
    height: 18px;
    padding: 0;
    border: 0;
    background: transparent;
    text-align: left;
    font: 400 11px/18px var(--font);
    color: var(--br-folder);
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
    cursor: pointer;
  }
  .rename {
    flex: 1;
    min-width: 0;
    height: 16px;
    background: #0f161a;
    color: var(--br-file-sel);
    border: 1px solid #2a353b;
    border-radius: 0;
    padding: 0 3px;
    font: 11px var(--font);
  }
  .count {
    flex: none;
    font: 10px var(--font-mono);
    color: #5a666e;
    padding-right: 2px;
  }
  .tag {
    flex: none;
    font: 600 8px var(--font-caps);
    letter-spacing: 0.05em;
    text-transform: uppercase;
    color: var(--br-folder-live);
  }
  .tools {
    display: flex;
    flex-wrap: wrap;
    gap: 2px;
    padding: 1px 4px 3px 28px;
  }
  .tool {
    height: 16px;
    padding: 0 5px;
    border: 1px solid #1e282e;
    border-radius: 1px;
    background: #1a2328;
    color: #9aa4aa;
    font: 400 10px var(--font);
    cursor: pointer;
  }
  .tool:hover:not(:disabled) {
    background: #242d32;
    color: var(--br-file-sel);
  }
  .tool:focus-visible {
    outline: none;
    background-image: var(--focus-fill);
  }
  .tool:disabled {
    opacity: 0.4;
    cursor: default;
  }
  .ask {
    margin: 2px 4px 4px 28px;
    padding: 4px 6px;
    background: color-mix(in srgb, var(--warn) 12%, var(--br-bg));
    border: 1px solid var(--br-edge);
    font-size: 11px;
    color: var(--br-file);
  }
  .ask button {
    height: 16px;
    margin-left: 4px;
    padding: 0 5px;
    border: 1px solid #1e282e;
    border-radius: 1px;
    background: #1a2328;
    color: #9aa4aa;
    font: 400 10px var(--font);
    cursor: pointer;
  }
  .slots,
  .hits {
    margin: 0;
    padding: 0 0 2px;
  }
  .slots {
    padding-left: 12px;
    border-left: 1px solid var(--br-guide);
    margin-left: 7px;
  }
  .danger {
    color: #c88884;
  }
  .go {
    background: color-mix(in srgb, var(--accent) 35%, #1a2328) !important;
    color: var(--br-file-sel) !important;
    border-color: var(--accent-soft) !important;
  }
  .prow {
    display: flex;
    align-items: center;
    gap: 5px;
    width: 100%;
    height: 18px;
    min-height: 18px;
    padding: 0 6px 0 4px;
    border: 0;
    background: transparent;
    text-align: left;
    color: inherit;
    font: 400 11px/18px var(--font);
    cursor: pointer;
  }
  .prow:hover {
    background: var(--br-hover);
  }
  .prow:focus-visible {
    outline: none;
    background-image: var(--focus-fill);
  }
  .prow.sel {
    background: var(--br-sel);
  }
  .prow.sel .nm {
    color: var(--br-file-sel);
  }
  .prow.sel .file-ico {
    color: #c5ced3;
  }
  .pos {
    flex: none;
    font: 9px var(--font-mono);
    color: #5a666e;
  }
  .nm {
    flex: 1;
    min-width: 0;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
    color: var(--br-file);
  }
  .newzone {
    margin: 4px 6px;
    padding: 6px;
    text-align: center;
    color: #5a666e;
    font-size: 10px;
    border: 1px dashed #243038;
    background: transparent;
  }
  .newzone:hover,
  .newzone.target {
    color: var(--br-file);
    border-color: #3a4a52;
  }
  .empty,
  .hint {
    color: #66727a;
    font-size: 10px;
    padding: 4px 6px;
    margin: 0;
  }
  .hit .pos:first-child {
    color: var(--br-folder);
  }
</style>
