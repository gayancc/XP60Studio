<script lang="ts">
  import { onDestroy, onMount, untrack } from 'svelte';
  import { DockPanel, TabPanel, ToggleLed } from '../kit';
  import FactoryPanel from './FactoryPanel.svelte';
  import { asSyx, download, toMidiFile } from '../model/midiFile';
  import { buildFolders, findFolder, normaliseTag, pathParts, topOf } from '../model/folders';
  import { planRename, summarise } from '../model/nameReplace';
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
  let findText = $state('');
  let replaceText = $state('');
  let matchCase = $state(false);
  const plans = $derived(lib ? planRename(lib.rows, findText, replaceText, matchCase) : []);
  const doable = $derived(plans.filter((p) => p.skip === null).length);

  async function replaceAll(): Promise<void> {
    if (!lib || doable === 0) return;
    await lib.renameMany(plans);
    findText = '';
  }
  let newTag = $state('');
  let renameTo = $state('');
  let confirmDelete = $state(false);
  let input: HTMLInputElement | undefined = $state();

  onMount(() => {
    void lib?.refresh();
  });

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
      download('xppatch-library.mid', toMidiFile(bytes));
    } catch (e) {
      if (lib) lib.error = e instanceof Error ? e.message : String(e);
    }
  }

  async function exportVisible(): Promise<void> {
    const bytes = await lib?.exportVisible(store.deviceId);
    if (!bytes) return;
    const url = URL.createObjectURL(new Blob([bytes as BlobPart], { type: 'application/octet-stream' }));
    const a = document.createElement('a');
    a.href = url;
    a.download = 'xppatch-library-bank.syx';
    a.click();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
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
        {#if preview.enabled}<p class="notice" role="status">{preview.playing !== null ? 'Sounding on the temporary patch. Leaving the list puts the previous sound back.' : 'Rest the pointer on a patch to hear it on the instrument (temporary patch only).'}</p>{/if}
        {#if preview.error}<p class="error" role="alert">{preview.error}</p>{/if}
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
        {#if lib.notice}<p class="notice" role="status">{lib.notice}</p>{/if}
        {#if lib.error}<p class="error" role="alert">{lib.error}</p>{/if}
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
      <DockPanel title="Find and replace" meta="names shown" focused={false}>
        <div class="fr">
          <input type="text" placeholder="Find in names" aria-label="Find in names" bind:value={findText} />
          <input type="text" placeholder="Replace with" aria-label="Replace with" maxlength="12" bind:value={replaceText} />
          <ToggleLed checked={matchCase} label="Match case" name="Match upper and lower case" context="Library" onchange={(on) => (matchCase = on)} />
          {#if findText !== ''}
            <p class="dim" role="status">{summarise(plans)}</p>
            <ul class="preview" aria-label="Names that would change">
              {#each plans.slice(0, 6) as p (p.id)}
                <li class:skipped={p.skip !== null}>{p.from} → {p.to}{p.skip ? ` (${p.skip})` : ''}</li>
              {/each}
              {#if plans.length > 6}<li class="dim">… and {plans.length - 6} more</li>{/if}
            </ul>
          {/if}
          <button type="button" disabled={lib.busy || doable === 0} onclick={replaceAll}>Replace in {doable} name{doable === 1 ? '' : 's'}</button>
        </div>
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
  button {
    background: var(--well-deep);
    border: 1px solid var(--edge);
    border-radius: var(--radius);
    padding: 3px 10px;
    cursor: pointer;
    color: inherit;
  }
  button:hover:not(:disabled) {
    background: var(--well);
  }
  button:focus-visible {
    outline: none;
    box-shadow: var(--focus);
  }
  button:disabled {
    opacity: 0.4;
    cursor: default;
  }
  .primary {
    background: var(--accent);
    color: var(--accent-ink);
    border-color: var(--accent-soft);
  }
  .danger {
    background: var(--bad);
    color: #1c0808;
  }
  .rows {
    list-style: none;
    margin: 0;
    padding: 0;
    max-height: calc(100vh - 270px);
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
    background: var(--accent);
    color: var(--accent-ink);
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
    border-radius: 2px;
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
    color: var(--accent-ink);
    border-color: var(--accent-ink);
  }
  .empty {
    padding: 10px;
    color: var(--text-dim);
  }
  .notice {
    color: var(--ok);
    margin: 4px 0;
  }
  .error {
    color: var(--bad);
    margin: 4px 0;
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
  .fr {
    display: flex;
    flex-direction: column;
    gap: 6px;
  }
  .fr input {
    background: var(--track);
    color: var(--text);
    border: 1px solid var(--edge);
    border-radius: var(--radius);
    padding: 4px 6px;
  }
  .preview {
    list-style: none;
    margin: 0;
    padding: 0;
    font-family: var(--font-mono);
    font-size: 11px;
  }
  .preview .skipped {
    color: var(--text-faint);
    text-decoration: line-through;
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
    padding: 2px 8px;
    background: var(--well-deep);
    border: 1px solid var(--edge);
    border-radius: 10px;
    cursor: pointer;
    color: inherit;
    font: inherit;
    font-size: 11px;
  }
  .crumbs .sep {
    color: var(--text-faint);
    align-self: center;
  }
  .crumbs button.on {
    background: var(--accent);
    color: var(--accent-ink);
  }
  .crumbs button:focus-visible {
    outline: none;
    box-shadow: var(--focus);
  }
</style>
