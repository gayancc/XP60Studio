<script lang="ts">
  import { onDestroy, onMount, untrack } from 'svelte';
  import { DockPanel, TabPanel, ToggleLed } from '../kit';
  import FactoryPanel from './FactoryPanel.svelte';
  import { asSyx, toMidiFile } from '../model/midiFile';
  import { saveFile } from '../model/saveFile';
  import { say } from '../model/ui.svelte';
  import { buildFolders, findFolder, normaliseTag, pathParts, topOf } from '../model/folders';
  import { preview } from '../model/preview.svelte';
  import { useStore } from './context';
  import { useServices } from './services';

  interface Props {
    /** Called after a stored patch has been loaded into the Patch editor. */
    onopened: () => void;
  }

  let { onopened }: Props = $props();
  const store = useStore();
  const services = useServices();
  const lib = $derived(services?.library);

  // The library as folders: a tag with a path ("Pads/Warm") is a folder inside a folder. The tabs are everything, favourites and
  // the top-level folders; below them come the folders inside the open one.
  const folders = $derived(buildFolders((lib?.tags ?? []).map((t) => t[0])));
  const topFolders = $derived(folders.slice(0, 8));
  const groupTabs = $derived(['All', 'Favourites', ...topFolders.map((f) => f.name)]);
  const openPath = $derived(lib?.tag ?? '');
  const groupIndex = $derived(
    lib?.favouritesOnly ? 1 : openPath ? Math.max(0, 2 + topFolders.findIndex((f) => f.name.toLowerCase() === topOf(openPath).toLowerCase())) : 0,
  );
  const openFolder = $derived(openPath ? findFolder(folders, openPath) : undefined);
  /** The way down to the open folder, each step reachable again: Pads, Pads/Warm. */
  const trail = $derived(
    openFolder
      ? pathParts(openFolder.path).map((name, i, parts) => ({ name, path: parts.slice(0, i + 1).join('/') }))
      : [],
  );
  /** Worth showing only when there is somewhere to go: a way back up, or folders inside. */
  const showTrail = $derived(openFolder !== undefined && (trail.length > 1 || openFolder.children.length > 0));

  function pickGroup(i: number): void {
    if (!lib) return;
    lib.favouritesOnly = i === 1;
    lib.tag = i >= 2 ? (topFolders[i - 2]?.path ?? '') : '';
    void lib.refresh();
  }

  function openFolderPath(path: string): void {
    if (!lib) return;
    lib.favouritesOnly = false;
    lib.tag = path;
    void lib.refresh();
  }

  let search = $state('');
  let newTag = $state('');
  let renameTo = $state('');
  let confirmDelete = $state(false);
  let input: HTMLInputElement | undefined = $state();

  onMount(() => {
    void lib?.refresh();
    void lib?.bulks.refresh();
  });

  // A bank file kept in the library is a bulk; choosing one shows only its patches, in their order there.
  function pickBulk(id: number | null): void {
    if (!lib) return;
    lib.bulk = id;
    void lib.refresh();
  }

  // Leaving the Library while a preview sounds puts the instrument's own sound back.
  onDestroy(() => void preview.stop());

  // Search as you type, after a short pause (the first run is the initial empty search, already done by onMount).
  let firstRun = true;
  $effect(() => {
    const text = search;
    const l = lib;
    if (!l) return;
    if (firstRun) {
      firstRun = false;
      return;
    }
    const timer = setTimeout(() => {
      l.text = text;
      void l.refresh();
    }, 250);
    return () => clearTimeout(timer);
  });

  const selected = $derived(lib?.selected ?? null);

  $effect(() => {
    // Reset the little editors when another patch is picked.
    const id = selected?.id;
    // Only a different patch resets the little editors; a refresh of the same one must not discard typing.
    untrack(() => {
      newTag = '';
      renameTo = selected?.name.trimEnd() ?? '';
      confirmDelete = false;
    });
    void id;
  });

  async function pickFile(e: Event & { currentTarget: HTMLInputElement }): Promise<void> {
    const file = e.currentTarget.files?.[0];
    e.currentTarget.value = '';
    if (!file || !lib) return;
    try {
      lib.stage(file.name, new Uint8Array(await file.arrayBuffer()));
    } catch (e) {
      lib.error = e instanceof Error ? e.message : String(e);
    }
  }

  async function open(id: number, name: string): Promise<void> {
    const bytes = await lib?.fetchBlocks(id);
    if (!bytes) return;
    store.openBlocks(bytes, `Library: ${name.trimEnd()}`);
    onopened();
  }

  async function exportVisibleMid(): Promise<void> {
    const bytes = await lib?.exportVisible(store.deviceId);
    if (!bytes) return;
    try {
      say(await saveFile('xppatch-library.mid', toMidiFile(bytes)));
    } catch (e) {
      if (lib) lib.error = e instanceof Error ? e.message : String(e);
    }
  }

  async function exportVisible(): Promise<void> {
    const bytes = await lib?.exportVisible(store.deviceId);
    if (!bytes) return;
    say(await saveFile('xppatch-library-bank.syx', bytes));
  }

  async function addCurrent(): Promise<void> {
    await lib?.addPatch(store.snapshot(), store.origin);
  }
</script>

{#if !lib}
  <div class="unavailable" role="status">
    <h2>Library</h2>
    <p>The library stores patches in a database on this computer and needs the desktop app. It is not available in the browser preview.</p>
  </div>
{:else}
  <div class="work">
    <section class="list">
      <TabPanel tabs={groupTabs} selected={groupIndex} name="Library groups" meta="{lib.rows.length} of {lib.total}" onchange={pickGroup}>
       {#snippet children()}
       <div class="listbody">
        {#if lib.bulks.bulks.length > 0}
          <div class="crumbs" role="group" aria-label="Bulks">
            <span class="sep">Bulk</span>
            <button type="button" class:on={lib.bulk === null} onclick={() => pickBulk(null)}>All patches</button>
            {#each lib.bulks.bulks as b (b.id)}
              <button type="button" class:on={lib.bulk === b.id} onclick={() => pickBulk(b.id)} title="{b.count} patches">{b.name}</button>
            {/each}
          </div>
        {/if}
        {#if openFolder && showTrail}
          <div class="crumbs" role="group" aria-label="Folders in {openFolder.name}">
            {#each trail as t, i (t.path)}
              <button type="button" class:on={t.path === openFolder.path} onclick={() => openFolderPath(t.path)}>{t.name}</button>
              {#if i < trail.length - 1}<span class="sep" aria-hidden="true">›</span>{/if}
            {/each}
            {#if openFolder.children.length > 0}
              <span class="sep" aria-hidden="true">|</span>
              {#each openFolder.children as f (f.path)}
                <button type="button" class="child" onclick={() => openFolderPath(f.path)}>{f.name}{f.children.length > 0 ? ' ▸' : ''}</button>
              {/each}
            {/if}
          </div>
        {/if}
        <div class="bar">
          <input
            type="search"
            placeholder="Search names"
            aria-label="Search names"
            bind:value={search}
          />
          {#if preview.available}
            <ToggleLed
              checked={preview.enabled}
              label="Hear on hover"
              name="Hear a patch on the instrument while the pointer rests on it"
              context="Library"
              onchange={(on) => preview.setEnabled(on)}
            />
          {/if}
          <button type="button" onclick={() => input?.click()} disabled={lib.busy}>Import .syx / .mid…</button>
          <input bind:this={input} type="file" accept=".syx,.mid,.midi" hidden onchange={pickFile} aria-label="Import a .syx or .mid file" />
          <button type="button" onclick={exportVisible} disabled={lib.busy || lib.rows.length === 0}>Export shown</button>
          <button type="button" onclick={exportVisibleMid} disabled={lib.busy || lib.rows.length === 0} title="The same patches as a Standard MIDI File">Export .mid</button>
          <button type="button" onclick={addCurrent} disabled={lib.busy} title="Store the patch open in the editor">Add current patch</button>
        </div>
        {#if lib.staged}
          <div class="staged" role="group" aria-label="File to import">
            <p>
              <b>{lib.staged.name}</b> holds {lib.staged.names.length} patch{lib.staged.names.length === 1 ? '' : 'es'}:
              {lib.staged.names.slice(0, 8).map(([, n]) => n.trimEnd() || '(unnamed)').join(', ')}{lib.staged.names.length > 8 ? ', …' : ''}
            </p>
            <div class="actions">
              <button type="button" class="primary" disabled={lib.busy} onclick={() => lib.importStaged()}>Import {lib.staged.names.length}</button>
              <button type="button" onclick={() => lib.cancelStaged()}>Cancel</button>
            </div>
          </div>
        {/if}
        <ul class="rows" aria-label="Stored patches">
          {#each lib.rows as r (r.id)}
            <li class:sel={lib.selectedId === r.id} onpointerenter={() => preview.hover(r.id)} onpointerleave={() => preview.unhover()}>
              <button
                type="button"
                class="star"
                aria-label={r.favourite ? 'Remove from favourites' : 'Add to favourites'}
                aria-pressed={r.favourite}
                disabled={lib.busy}
                onclick={() => lib.favourite(r.id, !r.favourite)}>{r.favourite ? '★' : '☆'}</button>
              <button type="button" class="name" onclick={() => (lib.selectedId = r.id)} ondblclick={() => open(r.id, r.name)}>
                <span class="n">{r.name.trimEnd() || '(unnamed)'}</span>
                <span class="tags">{#each r.tags as t (t)}<i>{t}</i>{/each}</span>
                <span class="src">{r.source}</span>
              </button>
            </li>
          {:else}
            <li class="empty">{lib.total === 0 && !lib.text && !lib.tag ? 'The library is empty. Import a .syx bank to start.' : 'No patches match.'}</li>
          {/each}
        </ul>
       </div>
       {/snippet}
      </TabPanel>
    </section>

    <aside class="detail">
      <DockPanel title={selected ? selected.name.trimEnd() || '(unnamed)' : 'Patch'} meta="details" focused={false}>
        {#if selected}
          <div class="actions">
            <button type="button" class="primary" onclick={() => open(selected.id, selected.name)}>Open in editor</button>
            {#if confirmDelete}
              <button type="button" class="danger" disabled={lib.busy} onclick={() => lib.remove(selected.id)}>Delete for good</button>
              <button type="button" onclick={() => (confirmDelete = false)}>Keep</button>
            {:else}
              <button type="button" onclick={() => (confirmDelete = true)}>Delete…</button>
            {/if}
          </div>
          <label class="field">
            <span class="caps">Rename</span>
            <input
              type="text"
              maxlength="12"
              value={renameTo}
              oninput={(e) => (renameTo = e.currentTarget.value)}
              onkeydown={(e) => {
                if (e.key === 'Enter' && renameTo.trim()) void lib.rename(selected.id, renameTo.trim());
              }}
              aria-label="Rename patch"
            />
          </label>
          <div class="field">
            <span class="caps">Tags</span>
            <div class="chips">
              {#each selected.tags as t (t)}
                <span class="chip">{t}<button type="button" aria-label="Remove tag {t}" onclick={() => lib.removeTag(selected.id, t)}>×</button></span>
              {:else}
                <span class="dim">none</span>
              {/each}
            </div>
            <input
              type="text"
              placeholder="Add a tag or folder (Pads/Warm), then Enter"
              value={newTag}
              oninput={(e) => (newTag = e.currentTarget.value)}
              onkeydown={(e) => {
                if (e.key === 'Enter' && normaliseTag(newTag) !== '') {
                  void lib.addTag(selected.id, normaliseTag(newTag)).then((ok) => {
                    if (ok) newTag = '';
                  });
                }
              }}
              aria-label="Add a tag"
            />
          </div>
          <p class="dim">From {selected.source}</p>
        {:else}
          <p class="dim">Select a patch to see its tags and open it. Double-click a row to open it in the editor.</p>
        {/if}
      </DockPanel>
      <DockPanel title="Factory lists" meta="names only" focused={false} fill>
        <FactoryPanel />
      </DockPanel>
    </aside>
  </div>
{/if}

<style>
  .work {
    display: grid;
    grid-template-columns: 1fr minmax(300px, 360px);
    gap: 10px;
    padding: 10px;
    min-height: 0;
    /* one row as tall as the window leaves, so the lists scroll inside it instead of pushing the page past the window */
    grid-template-rows: minmax(0, 1fr);
  }
  .list {
    display: flex;
    flex-direction: column;
  }
  .list > :global(.tp) {
    flex: 1;
    min-height: 0;
  }
  .listbody {
    display: flex;
    flex-direction: column;
    height: 100%;
    min-height: 0;
  }
  .work > :global(*) {
    min-height: 0;
  }
  /* The details column: the lists at the bottom take whatever height is left, so nothing is pushed past the window. */
  .detail {
    display: flex;
    flex-direction: column;
    gap: 8px;
    min-height: 0;
  }
  .unavailable {
    padding: 24px;
    max-width: 560px;
  }
  .bar {
    display: flex;
    gap: 8px;
    align-items: flex-end;
    flex-wrap: wrap;
    margin-bottom: 8px;
  }
  input[type='search'],
  input[type='text'] {
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
  .bar > button,
  .actions button,
  .staged button {
    height: var(--work-h);
    padding: 0 12px;
    border-radius: 2px;
    border: 1px solid var(--face-rim);
    background: var(--face-off);
    color: var(--face-ink);
    cursor: pointer;
    font: inherit;
  }
  button:focus-visible {
    outline: none;
    background-image: var(--focus-fill);
  }
  button:disabled {
    opacity: 0.4;
    cursor: default;
  }
  .primary {
    color: var(--face-ink);
  }
  .danger {
    background: var(--bad);
    color: #1c0808;
  }
  .rows {
    list-style: none;
    margin: 0;
    padding: 0;
    flex: 1;
    min-height: 120px;
    overflow: auto;
    display: flex;
    flex-direction: column;
    gap: 1px;
  }
  .rows li {
    display: flex;
    gap: 4px;
  }
  .rows li.sel .name {
    /* Selected browser row, shot 23: fill about #2f3037, label about #b6b7b5. Not an accent wash. */
    background: #2f3037;
    color: #b6b7b5;
  }
  .rows .star {
    background: transparent;
    border: 0;
    width: 26px;
    padding: 0;
    color: var(--warn);
  }
  .name {
    flex: 1;
    display: flex;
    gap: 10px;
    align-items: baseline;
    text-align: left;
    background: transparent;
    border: 0;
    border-radius: 0;
    color: #899093;
  }
  .n {
    min-width: 130px;
    font-family: var(--font-mono);
  }
  .tags {
    display: flex;
    gap: 4px;
    flex: 1;
  }
  .tags i {
    font-style: normal;
    font-size: 10px;
    padding: 0 6px;
    border: 1px solid var(--edge-light);
    border-radius: 8px;
    color: var(--text-dim);
  }
  .src {
    color: var(--text-faint);
    font-size: 10.5px;
    max-width: 160px;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }
  li.sel .src,
  li.sel .tags i {
    color: #b6b7b5;
    border-color: #b6b7b5;
  }
  .empty {
    padding: 10px;
    color: var(--text-dim);
  }
  .actions {
    display: flex;
    gap: 6px;
    margin-bottom: 10px;
  }
  .field {
    display: flex;
    flex-direction: column;
    gap: 3px;
    margin-bottom: 10px;
  }
  .chips {
    display: flex;
    gap: 4px;
    flex-wrap: wrap;
  }
  .chip {
    display: inline-flex;
    align-items: center;
    gap: 2px;
    padding: 0 2px 0 8px;
    border: 1px solid var(--edge-light);
    border-radius: 9px;
  }
  .chip button {
    background: transparent;
    border: 0;
    padding: 0 4px;
  }
  .dim {
    color: var(--text-dim);
  }
  .staged {
    padding: 6px 8px;
    margin: 4px 0;
    border: 1px solid var(--accent);
    border-radius: var(--radius);
    background: var(--body-deep);
  }
  .staged p {
    margin: 0 0 6px;
  }
  .crumbs {
    display: flex;
    flex-wrap: wrap;
    gap: 4px;
    margin-bottom: 6px;
  }
  .crumbs button {
    /* Same category text as the effect-picker tabs: idle #76747a, selected white, no raised face. */
    padding: 4px 10px 5px;
    background: transparent;
    border: 0;
    border-radius: 0;
    cursor: pointer;
    color: #76747a;
    font: inherit;
    font-size: 11px;
  }
  .crumbs button:hover {
    background: transparent;
    color: #76747a;
  }
  .crumbs .sep {
    color: var(--text-faint);
    align-self: center;
  }
  .crumbs button.on,
  .crumbs button.on:hover {
    background: transparent;
    color: #ffffff;
    font-weight: 600;
  }
  .crumbs button:focus-visible {
    outline: none;
    background-image: var(--focus-fill);
  }
</style>
