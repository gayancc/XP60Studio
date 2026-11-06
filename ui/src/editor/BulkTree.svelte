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
  }

  let { bank, lib, deviceId }: Props = $props();
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
    const bytes = await lib.fetchBlocks(patchId);
    if (bytes) bank.assign(bank.selected, bytes, 'library');
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
      <span class="pos">{positionLabel(position)}</span>
      <span class="nm">{name.trimEnd() || '(unnamed)'}</span>
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
              <button type="button" onclick={() => (bulks.pendingLoad = b.id)} disabled={bank.locked}>Load into bank</button>
              <button type="button" onclick={() => startRename(b.id, b.name)}>Rename</button>
              <button type="button" onclick={() => bulks.duplicate(b.id)}>Copy</button>
              <button type="button" onclick={() => exportBulk(b.id, b.name)}>Export .syx</button>
              <button type="button" onclick={() => toggleVersions(b.id)} aria-pressed={versionsOf === b.id}>Versions</button>
              <button type="button" class="danger" onclick={() => (confirmDelete = b.id)}>Delete</button>
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
        <li class="empty">No bulks yet. Import a .syx bank in the Library workspace, or save the bank here.</li>
      {/each}
      <li
        class="newzone"
        class:target={guide?.key === 'new'}
        ondragenter={(e) => over(e, 'new', 'Drop to make a new bulk from what you drag (a bulk dropped here is copied).')}
        ondragover={(e) => over(e, 'new', 'Drop to make a new bulk from what you drag (a bulk dropped here is copied).')}
        ondragleave={(e) => leave(e, 'new')}
        ondrop={(e) => drop(e, { kind: 'new' })}
      >
        Drop patches here to make a new bulk
      </li>
    </ul>
  {/if}
  <p class="hint" aria-live="polite">
    {#if guide}
      {guide.text}
    {:else}
      Drag a patch onto a bank slot, another bulk or the new-bulk area. Alt copies instead of moving. Ctrl-click picks several patches.
    {/if}
  </p>
</div>

<style>
  .tree {
    display: flex;
    flex-direction: column;
    gap: 6px;
    min-width: 0;
  }
  ul {
    list-style: none;
    margin: 0;
    padding: 0;
  }
  .bulk {
    margin-bottom: 4px;
  }
  .head {
    display: flex;
    align-items: center;
    gap: 4px;
    padding: 2px 4px;
    background: var(--well-deep);
    border-radius: var(--radius);
  }
  .head.loaded {
    background: color-mix(in srgb, var(--accent) 16%, var(--well-deep));
  }
  .head.target,
  .newzone.target,
  .prow.target {
    background: color-mix(in srgb, var(--accent) 50%, var(--well-deep));
    color: var(--text);
  }
  .chev {
    width: 20px;
    padding: 2px 0;
    text-align: center;
  }
  .name {
    flex: 1;
    min-width: 0;
    text-align: left;
    font-weight: 600;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }
  .rename {
    flex: 1;
    min-width: 0;
    background: var(--track);
    color: var(--text);
    border: 1px solid var(--edge);
    border-radius: var(--radius);
    padding: 2px 6px;
    font: inherit;
  }
  .count {
    font: 10px var(--font-mono);
    color: var(--text-dim);
  }
  .tag {
    font: 600 9px var(--font-caps);
    letter-spacing: 0.06em;
    text-transform: uppercase;
    color: var(--accent);
  }
  .tools {
    display: flex;
    flex-wrap: wrap;
    gap: 4px;
    padding: 4px 0 4px 24px;
  }
  .ask {
    margin: 2px 0 4px 24px;
    padding: 4px 6px;
    background: color-mix(in srgb, var(--warn) 14%, var(--well-deep));
    border-radius: var(--radius);
    font-size: 11px;
  }
  .slots {
    margin-left: 16px;
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
  .head button,
  .prow {
    background: transparent;
    border-color: transparent;
  }
  button:hover:not(:disabled) {
    background-color: var(--well);
  }
  button:focus-visible {
    outline: none;
    background-image: var(--focus-fill);
  }
  button:disabled {
    opacity: 0.4;
    cursor: default;
  }
  .danger {
    color: var(--bad);
  }
  .go {
    background: var(--accent);
    color: var(--accent-ink);
    border-color: var(--accent-soft);
  }
  .prow {
    display: flex;
    gap: 8px;
    align-items: baseline;
    width: 100%;
    padding: 2px 6px;
    text-align: left;
  }
  .prow.sel {
    background: color-mix(in srgb, var(--accent) 26%, transparent);
  }
  .pos {
    flex: none;
    font: 10px var(--font-mono);
    color: var(--text-faint);
  }
  .nm {
    flex: 1;
    min-width: 0;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }
  .newzone {
    margin-top: 6px;
    padding: 10px 6px;
    text-align: center;
    color: var(--text-dim);
    font-size: 11px;
    background: var(--hint);
    border-radius: var(--radius);
  }
  .empty,
  .hint {
    color: var(--text-dim);
    font-size: 11px;
    padding: 4px 6px;
    margin: 0;
  }
</style>
