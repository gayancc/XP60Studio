<script lang="ts">
  import { onDestroy, onMount } from 'svelte';
  import { Meter, TabPanel } from '../kit';
  import { SLOTS } from '../model/bank.svelte';
  import { PatchStore } from '../model/store.svelte';
  import { tauriBackend, type BackupFile } from '../model/backend';
  import { arrangement, tabIn } from '../model/layouts.svelte';
  import { asSyx, toMidiFile } from '../model/midiFile';
  import { errorText, reportError } from '../model/messages.svelte';
  import { saveFile } from '../model/saveFile';
  import { say } from '../model/ui.svelte';
  import { useStore } from './context';
  import { useServices } from './services';

  interface Props {
    /** Called after a slot has been opened in the Patch editor. */
    onopened: () => void;
  }

  let { onopened }: Props = $props();
  const store = useStore();
  const services = useServices();
  const bank = $derived(services?.bank);
  const lib = $derived(services?.library);

  let search = $state('');
  let fileInput: HTMLInputElement | undefined = $state();
  let dragFrom = $state<number | null>(null);
  let firstSearch = true;

  onMount(() => {
    void lib?.refresh();
  });

  // Leaving this workspace with an armed write must not leave it armed.
  onDestroy(() => {
    void bank?.cancelArming();
  });

  $effect(() => {
    const text = search;
    const l = lib;
    if (!l) return;
    if (firstSearch) {
      firstSearch = false;
      return;
    }
    const timer = setTimeout(() => {
      l.text = text;
      void l.refresh();
    }, 250);
    return () => clearTimeout(timer);
  });

  const changed = $derived(new Set(bank?.changed ?? []));
  const confirmed = $derived(new Set(bank?.confirmedSlots ?? []));
  const selectedSlot = $derived(bank ? bank.slots[bank.selected] : undefined);
  const slotLabel = (i: number): string => String(i + 1).padStart(3, '0');
  const percent = $derived(bank?.progress && bank.progress.total > 0 ? bank.progress.done / bank.progress.total : 0);

  async function pickFile(e: Event & { currentTarget: HTMLInputElement }): Promise<void> {
    const file = e.currentTarget.files?.[0];
    e.currentTarget.value = '';
    if (!file || !bank) return;
    try {
      const bytes = asSyx(new Uint8Array(await file.arrayBuffer()));
      bank.loadFile(bytes, file.name, PatchStore.listSyx(bytes));
    } catch (err) {
      bank.error = err instanceof Error ? err.message : String(err);
    }
  }

  async function exportBank(): Promise<void> {
    const bytes = await bank?.toSyx(store.deviceId);
    if (!bytes) return;
    say(await saveFile('xppatch-user-bank.syx', bytes));
  }

  async function exportBankMid(): Promise<void> {
    const bytes = await bank?.toSyx(store.deviceId);
    if (!bytes) return;
    try {
      say(await saveFile('xppatch-user-bank.mid', toMidiFile(bytes)));
    } catch (e) {
      if (bank) bank.error = e instanceof Error ? e.message : String(e);
    }
  }

  async function addToLibrary(): Promise<void> {
    await bank?.addAllToLibrary(`Synth/${new Date().toISOString().slice(0, 10)}`);
    await lib?.refresh();
  }

  async function backupEverything(): Promise<void> {
    const bytes = await bank?.backupEverything();
    if (bytes) say(await saveFile('xppatch-user-memory.syx', bytes));
  }

  async function putLibraryPatch(id: number, slot: number): Promise<void> {
    const bytes = await lib?.fetchBlocks(id);
    if (bytes) bank?.assign(slot, bytes, 'library');
  }

  function drop(e: DragEvent, slot: number): void {
    e.preventDefault();
    const lib = e.dataTransfer?.getData('text/xp-library');
    const from = e.dataTransfer?.getData('text/xp-slot');
    if (bank?.locked) return;
    if (lib) void putLibraryPatch(Number(lib), slot);
    else if (from !== undefined && from !== '') bank?.swap(Number(from), slot);
    dragFrom = null;
  }

  async function openSlot(slot: number): Promise<void> {
    const bytes = bank?.slots[slot]?.bytes;
    if (!bytes) return;
    store.openBlocks(bytes, `Bank slot ${slotLabel(slot)}`);
    onopened();
  }
  const page = $derived(tabIn(arrangement.bankPage, 4));
  const pageNames = ['001–032', '033–064', '065–096', '097–128'];
  const rightTabs = ['Slot', 'Library', 'Backups', 'Write'];
  const rtab = $derived(tabIn(arrangement.bankTab, 4));
  let backups = $state<BackupFile[]>([]);

  async function loadBackups(): Promise<void> {
    try {
      backups = await tauriBackend.backupsList();
    } catch (e) {
      reportError(errorText(e));
    }
  }

  async function openBackup(name: string): Promise<void> {
    if (!bank || bank.locked) return;
    try {
      const bytes = asSyx(await tauriBackend.backupRead(name));
      bank.loadFile(bytes, name, PatchStore.listSyx(bytes));
    } catch (e) {
      reportError(errorText(e));
    }
  }

  const when = (secs: number): string => (secs > 0 ? new Date(secs * 1000).toLocaleString() : 'unknown time');

  // The list is read when the tab is opened, so it always shows what is on disk now.
  $effect(() => {
    if (rtab === 2) void loadBackups();
  });
  // The confirmation must be seen: arming switches to the Write tab.
  $effect(() => {
    if (bank?.phase === 'armed' || bank?.phase === 'arming' || bank?.phase === 'writing') arrangement.bankTab = 3;
  });
</script>

{#if !bank || !lib}
  <div class="unavailable" role="status">
    <h2>Bank</h2>
    <p>The bank workspace reads and writes the instrument's User memory and needs the desktop app.</p>
  </div>
{:else}
  <div class="work">
    <section class="left">
      <div class="bar">
          <button type="button" onclick={() => bank.readFromSynth()} disabled={bank.working}>Read from synth</button>
          <button type="button" onclick={() => fileInput?.click()} disabled={bank.locked}>Load .syx / .mid…</button>
          <input bind:this={fileInput} type="file" accept=".syx,.mid,.midi" hidden onchange={pickFile} aria-label="Load a .syx or .mid bank file" />
          <button type="button" onclick={exportBank} disabled={bank.working}>Export .syx</button>
          <button type="button" onclick={exportBankMid} disabled={bank.working}>Export .mid</button>
          <button type="button" onclick={addToLibrary} disabled={bank.working || bank.slots.every((s) => s.bytes === null)} title="File every patch in the grid into the Library, in a folder named Synth and today's date">Add all to library</button>
          <button type="button" onclick={backupEverything} disabled={bank.working} title="Read all patches, performances and rhythm sets from the instrument into one file. Nothing is written to the instrument.">Back up all memory</button>
          {#if bank.working}
            <button type="button" onclick={() => bank.cancel()}>Cancel</button>
            <span class="prog" role="status">
              {bank.phase === 'reading' ? 'Reading' : bank.phase === 'arming' ? 'Backing up' : 'Writing'}
              {bank.progress ? `${bank.progress.done} / ${bank.progress.total}` : '…'}
              <Meter level={percent} length={120} label="Progress" />
            </span>
          {/if}
        <span class="dim">{bank.source} · {bank.changed.length} changed</span>
      </div>
      <TabPanel tabs={pageNames} selected={page} name="User bank pages" onchange={(n) => (arrangement.bankPage = n)}>
        {#snippet children(pg)}
        <div class="grid" role="group" aria-label="User patch slots">
          {#each bank.slots as s, i (i)}
            {#if i >= pg * 32 && i < pg * 32 + 32}
            <button
              type="button"
              class="slot"
              class:sel={bank.selected === i}
              class:changed={changed.has(i) || confirmed.has(i)}
              class:empty={s.bytes === null}
              class:dragging={dragFrom === i}
              draggable={!bank.locked && s.bytes !== null}
              ondragstart={(e) => {
                dragFrom = i;
                e.dataTransfer?.setData('text/xp-slot', String(i));
              }}
              ondragend={() => (dragFrom = null)}
              ondragover={(e) => {
                const t = e.dataTransfer?.types ?? [];
                if (!bank.locked && (t.includes('text/xp-slot') || t.includes('text/xp-library'))) e.preventDefault();
              }}
              ondrop={(e) => drop(e, i)}
              onclick={() => (bank.selected = i)}
              ondblclick={() => openSlot(i)}
              aria-label="Slot {slotLabel(i)}: {s.name.trimEnd() || 'empty'}{changed.has(i) ? ', changed' : ''}"
              aria-pressed={bank.selected === i}
            >
              <span class="n">{slotLabel(i)}</span>
              <span class="nm">{s.bytes ? s.name.trimEnd() || '(unnamed)' : '—'}</span>
            </button>
            {/if}
          {/each}
        </div>
        <p class="hint">Drag a slot onto another to swap them, or drag a library patch onto a slot. Double-click opens a slot in the editor. Nothing reaches the synth until you write the bank.</p>
        {/snippet}
      </TabPanel>
    </section>

    <aside class="right">
     <TabPanel tabs={rightTabs} selected={rtab} name="Bank tools" meta={bank.phase === 'armed' ? 'armed' : ''} onchange={(n) => (arrangement.bankTab = n)}>
      {#snippet children(n)}
      {#if n === 3}
        <p class="warn">Writing replaces patches inside the XP-60 for good. All 128 current patches are saved to a backup file first, and each slot is read back after it is written.</p>
        {#if bank.phase === 'idle' || bank.phase === 'failed' || bank.phase === 'done'}
          <button type="button" onclick={() => bank.arm()} disabled={bank.changed.length === 0}>
            1. Back up all 128 and prepare ({bank.changed.length} slot{bank.changed.length === 1 ? '' : 's'} to write)
          </button>
        {/if}
        {#if bank.phase === 'armed' && bank.armed}
          <div class="confirm" role="alert">
            <p>Backup saved: <span class="dim">{bank.armed.backup_path}</span></p>
            <p>Replace <b>{bank.confirmedSlots.length}</b> slot{bank.confirmedSlots.length === 1 ? '' : 's'}: {bank.confirmedSlots.slice(0, 12).map((i) => slotLabel(i)).join(', ')}{bank.confirmedSlots.length > 12 ? ', …' : ''}? The slots are locked until you write or cancel.</p>
            <div class="row">
              <button type="button" onclick={() => bank.cancelArming()}>Cancel</button>
              <button type="button" class="danger" onclick={() => bank.commit()}>2. Write {bank.confirmedSlots.length} slot{bank.confirmedSlots.length === 1 ? '' : 's'}</button>
            </div>
          </div>
        {/if}
      {:else if n === 2}
        <p class="dim">Every save into the instrument first writes what it replaces to a file here. Open one to look at it in the bank grid; sending it back goes through the normal write steps.</p>
        <ul class="rows" aria-label="Backup files">
          {#each backups as f (f.name)}
            <li>
              <button type="button" class="lrow" disabled={bank.locked} onclick={() => openBackup(f.name)} title="Open {f.name} in the bank grid">
                <span>{f.name}</span>
                <span class="dim">{when(f.modified)} · {Math.round(f.bytes / 1024)} KB</span>
              </button>
            </li>
          {:else}
            <li class="dim">No backup files yet.</li>
          {/each}
        </ul>
      {:else if n === 0}
        <p class="big">{selectedSlot?.bytes ? selectedSlot.name.trimEnd() || '(unnamed)' : 'Nothing known about this slot yet.'}</p>
        <div class="row">
          <button type="button" onclick={() => openSlot(bank.selected)} disabled={!selectedSlot?.bytes}>Open in editor</button>
          <button type="button" onclick={() => bank.assign(bank.selected, store.snapshot(), 'editor')} disabled={bank.locked}>Put the editor's patch here</button>
        </div>
        <div class="row">
          <button type="button" onclick={() => { bank.swap(bank.selected, bank.selected - 1); bank.selected = Math.max(0, bank.selected - 1); }} disabled={bank.locked || bank.selected === 0}>Move up</button>
          <button type="button" onclick={() => { bank.swap(bank.selected, bank.selected + 1); bank.selected = Math.min(SLOTS - 1, bank.selected + 1); }} disabled={bank.locked || bank.selected === SLOTS - 1}>Move down</button>
        </div>
      {:else}
        <input type="search" placeholder="Search names" aria-label="Search the library" bind:value={search} />
        <ul class="rows" aria-label="Library patches">
          {#each lib.rows as r (r.id)}
            <li>
              <button
                type="button"
                class="lrow"
                draggable="true"
                ondragstart={(e) => e.dataTransfer?.setData('text/xp-library', String(r.id))}
                onclick={() => putLibraryPatch(r.id, bank.selected)}
                disabled={bank.locked}
                title="Put in slot {slotLabel(bank.selected)}"
              >
                <span>{r.name.trimEnd() || '(unnamed)'}</span>
                <span class="dim">→ {slotLabel(bank.selected)}</span>
              </button>
            </li>
          {:else}
            <li class="dim">No library patches match.</li>
          {/each}
        </ul>
      {/if}
      {/snippet}
     </TabPanel>
    </aside>
  </div>
{/if}

<style>
  .work {
    display: grid;
    grid-template-columns: 1fr minmax(320px, 380px);
    gap: 10px;
    padding: 10px;
    min-height: 0;
  }
  .left > :global(.tp),
  .right > :global(.tp) {
    flex: 1;
  }
  .left,
  .right {
    display: flex;
    flex-direction: column;
    gap: 10px;
    min-width: 0;
  }
  .unavailable {
    padding: 24px;
    max-width: 560px;
  }
  .bar {
    display: flex;
    gap: 8px;
    align-items: center;
    flex-wrap: wrap;
    margin-bottom: 8px;
  }
  .prog {
    display: inline-flex;
    gap: 8px;
    align-items: center;
  }
  button {
    background: var(--well-deep);
    border: 1px solid var(--edge);
    border-radius: var(--radius);
    padding: 3px 10px;
    cursor: pointer;
    color: inherit;
    font: inherit;
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
  .danger {
    background: var(--bad);
    color: #120404;
    border-color: var(--bad);
    font-weight: 600;
  }
  .grid {
    display: grid;
    grid-template-columns: repeat(8, 1fr);
    gap: 4px;
  }
  .slot {
    display: flex;
    flex-direction: column;
    align-items: flex-start;
    gap: 1px;
    padding: 6px 6px;
    min-width: 0;
    text-align: left;
    background: var(--well-deep);
  }
  .slot .n {
    font-family: var(--font-mono);
    font-size: 9.5px;
    color: var(--text-faint);
  }
  .slot .nm {
    max-width: 100%;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
    font-size: 11px;
  }
  .slot.empty {
    opacity: 0.5;
  }
  .slot.changed {
    border-color: var(--warn);
    box-shadow: inset 0 0 0 1px var(--warn);
  }
  .slot.sel {
    background: var(--accent);
    color: var(--accent-ink);
    border-color: var(--accent-soft);
  }
  .slot.sel .n {
    color: var(--accent-ink);
  }
  .slot.dragging {
    opacity: 0.4;
  }
  .hint,
  .dim {
    color: var(--text-dim);
    font-size: 11px;
  }
  .warn {
    color: var(--warn);
    margin: 0 0 8px;
  }
  .confirm p {
    margin: 4px 0;
  }
  .row {
    display: flex;
    gap: 8px;
    margin: 8px 0;
  }
  .big {
    font-family: var(--font-mono);
    font-size: 14px;
    margin: 0 0 8px;
  }
  input[type='search'] {
    width: 100%;
    background: var(--track);
    color: var(--text);
    border: 1px solid var(--edge);
    border-radius: var(--radius);
    padding: 4px 6px;
    margin-bottom: 6px;
  }
  .rows {
    list-style: none;
    margin: 0;
    padding: 0;
    max-height: 330px;
    overflow: auto;
  }
  .lrow {
    display: flex;
    justify-content: space-between;
    width: 100%;
    background: transparent;
    border: 0;
    text-align: left;
  }
</style>
