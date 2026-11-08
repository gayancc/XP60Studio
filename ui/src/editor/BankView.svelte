<script lang="ts">
  import { onDestroy, onMount } from 'svelte';
  import { Meter, TabPanel } from '../kit';
  import { SLOTS } from '../model/bank.svelte';
  import { PATCH_COUNT, PER_BANK, panelLabel, patchNumber, posOf, slotOf, soundSelect, SUBGROUPS } from '../model/bankPanel';
  import { PatchStore } from '../model/store.svelte';
  import { tauriBackend, type BackupFile } from '../model/backend';
  import { arrangement, tabIn } from '../model/layouts.svelte';
  import { asSyx, toMidiFile } from '../model/midiFile';
  import { errorText, reportError } from '../model/messages.svelte';
  import { saveFile } from '../model/saveFile';
  import { say } from '../model/ui.svelte';
  import type { SynthService } from '../model/synth.svelte';
  import { useStore } from './context';
  import { parsePayload, DRAG_TYPE } from '../model/bulks.svelte';
  import BulkLibraryPanel from './BulkLibraryPanel.svelte';
  import { useServices } from './services';

  interface Props {
    /** Called after a slot has been opened in the Patch editor. */
    onopened: () => void;
    /** The connection to the instrument; when it is connected the patch buttons can select the sound there too. */
    synth?: SynthService;
  }

  let { onopened, synth }: Props = $props();
  const store = useStore();
  const services = useServices();
  const bank = $derived(services?.bank);
  const lib = $derived(services?.library);

  let dragFrom = $state<number | null>(null);
  /** The slot a patch being dragged is over right now. While it is set, the front-panel buttons that pick that slot are lit as a guide. */
  let dropTarget = $state<number | null>(null);
  /**
   * While a drag is held over a SUB a/b pad, show that sub-group's 64 pads so a patch from b can be dropped on a (and the other way).
   * Cleared when the drag ends. Does not change the selected patch by itself.
   */
  let dragPreviewSub = $state<0 | 1 | null>(null);
  const dropPos = $derived(dropTarget === null ? null : posOf(dropTarget));

  onMount(() => {
    void lib?.refresh();
  });

  // Leaving this workspace with an armed write must not leave it armed.
  onDestroy(() => {
    void bank?.cancelArming();
  });

  const changed = $derived(new Set(bank?.changed ?? []));
  const confirmed = $derived(new Set(bank?.confirmedSlots ?? []));
  const selectedSlot = $derived(bank ? bank.slots[bank.selected] : undefined);
  const slotLabel = (i: number): string => String(i + 1).padStart(3, '0');
  const percent = $derived(bank?.progress && bank.progress.total > 0 ? bank.progress.done / bank.progress.total : 0);

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
    const day = new Date().toISOString().slice(0, 10);
    const filed = await bank?.addAllToLibrary(`Synth/${day}`);
    // The patches are also kept as a bulk, so they show under a name in the Library tab (a bank that already came from a bulk stays that bulk).
    if (filed && bank && lib && !bank.origin) {
      await lib.bulks.refresh();
      await lib.bulks.saveBank(bank, lib.bulks.freeName(`Synth ${day}`), null);
    }
    await lib?.refresh();
  }

  async function backupEverything(): Promise<void> {
    const bytes = await bank?.backupEverything();
    if (bytes) say(await saveFile('xppatch-user-memory.syx', bytes));
  }

  async function putLibraryPatch(id: number, slot: number, follow = true): Promise<void> {
    const bytes = await lib?.fetchBlocks(id);
    if (bytes && bank?.assign(slot, bytes, 'library') && follow) choose(slot);
  }

  /** A slot or a library patch is being dragged and this bank can take it. */
  function acceptsDrag(e: DragEvent): boolean {
    const t = e.dataTransfer?.types ?? [];
    return !bank?.locked && (t.includes('text/xp-slot') || t.includes('text/xp-library') || t.includes(DRAG_TYPE));
  }

  function guide(e: DragEvent, slot: number): void {
    if (!acceptsDrag(e)) return;
    e.preventDefault();
    dropTarget = slot;
  }

  function leaveSlot(e: DragEvent, slot: number): void {
    if (dropTarget === slot && !(e.currentTarget as Node).contains(e.relatedTarget as Node | null)) dropTarget = null;
  }

  async function drop(e: DragEvent, slot: number): Promise<void> {
    e.preventDefault();
    if (bank?.locked) return;
    const payload = parsePayload(e.dataTransfer?.getData(DRAG_TYPE));
    const libData = e.dataTransfer?.getData('text/xp-library');
    const from = e.dataTransfer?.getData('text/xp-slot');
    if (payload?.type === 'bulk') {
      // A whole bulk is asked about first: it replaces every slot. The question is on the Library tab.
      if (services?.library) services.library.bulks.pendingLoad = payload.bulk;
      arrangement.bankTab = 1;
    } else if (payload) {
      // One patch goes in the slot; several go into the slots that follow it. Await so choose hears the new bytes.
      const jobs = payload.items.flatMap((item, i) =>
        item.kind !== 'bank-slot' && slot + i < PATCH_COUNT
          ? [putLibraryPatch(item.patchId, slot + i, false)]
          : [],
      );
      if (jobs.length > 0) {
        await Promise.all(jobs);
        choose(slot);
      } else choose(slot);
    } else if (libData) await putLibraryPatch(Number(libData), slot);
    else if (from !== undefined && from !== '') {
      bank?.swap(Number(from), slot);
      choose(slot);
    } else choose(slot);
    endDrag();
  }

  async function openSlot(slot: number): Promise<void> {
    const bytes = bank?.slots[slot]?.bytes;
    if (!bytes) return;
    store.openBlocks(bytes, `Bank slot ${slotLabel(slot)}`);
    onopened();
  }
  // The front panel's patch buttons: sub-group a or b, then BANK 1-8 and NUMBER 1-8 (Owner's Manual p.26). This screen is for arranging the User bank, so there is no
  // patch-group choice: the presets and expansion boards are fixed and are not rearranged here.
  const ONE_TO_EIGHT = [1, 2, 3, 4, 5, 6, 7, 8];
  let sub = $state<0 | 1>(0);
  const current = $derived(bank ? bank.selected : 0);
  const pos = $derived(posOf(current));
  /** Which sub-group's pads are on screen: drag preview over SUB a/b, else the selected patch's sub. */
  const viewSub = $derived(dragPreviewSub ?? sub);

  // Choosing a patch from anywhere (a click, an arrow key, the slot list) brings its sub-group up.
  $effect(() => {
    sub = posOf(current).sub;
  });

  function previewSub(e: DragEvent, t: 0 | 1): void {
    if (!acceptsDrag(e)) return;
    e.preventDefault();
    if (e.dataTransfer) e.dataTransfer.dropEffect = 'move';
    dragPreviewSub = t;
    dropTarget = null;
  }

  function endDrag(): void {
    dragFrom = null;
    dropTarget = null;
    dragPreviewSub = null;
  }

  // Pressing a patch also sounds it on the instrument when connected. No channel picker on this page —
  // the receive channel lives in System > MIDI.
  const canFollow = $derived(!!synth?.connected);

  function choose(slot: number): void {
    const next = Math.min(PATCH_COUNT - 1, Math.max(0, slot));
    if (bank) bank.selected = next;
    if (!canFollow) return;
    const held = bank?.slots[next];
    // A bulk / file / editor copy lives only in the app until Write. Program Change would still play the
    // old User memory on the keyboard — send this pad's bytes to temporary memory so the keys match the name.
    if (held?.bytes && held.origin !== 'synth') {
      store.auditionBlocks(held.bytes);
      return;
    }
    // Slot was read from the instrument (or is still empty): select that User number on the panel.
    const sel = soundSelect('USER', next);
    if (sel) void synth?.selectSound(0, sel.msb, sel.lsb, sel.program);
  }

  const chooseBank = (b: number): void => choose(slotOf({ sub, bank: b, num: pos.num }));
  const chooseNumber = (n: number): void => choose(slotOf({ sub, bank: pos.bank, num: n }));
  const chooseSub = (t: 0 | 1): void => choose(slotOf({ sub: t, bank: pos.bank, num: pos.num }));

  function panelKey(e: KeyboardEvent): void {
    const step = e.key === 'ArrowRight' ? 1 : e.key === 'ArrowLeft' ? -1 : e.key === 'ArrowDown' ? PER_BANK : e.key === 'ArrowUp' ? -PER_BANK : 0;
    if (step === 0) return;
    e.preventDefault();
    choose(current + step);
  }

  const cellName = (i: number): string => {
    const s = bank?.slots[i];
    return s?.bytes ? s.name.trimEnd() || '(unnamed)' : '—';
  };
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
          <button type="button" onclick={exportBank} disabled={bank.working}>Export .syx</button>
          <button type="button" onclick={exportBankMid} disabled={bank.working}>Export .mid</button>
          <button type="button" onclick={addToLibrary} disabled={bank.working || bank.slots.every((s) => s.bytes === null)} title="File every patch in the grid into the Library: as a bulk named Synth and today's date, and in a folder of the same name">Add all to library</button>
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
      <!-- svelte-ignore a11y_no_noninteractive_element_interactions -->
      <div class="panel" role="group" aria-label="Patch selection" tabindex="-1" onkeydown={panelKey}>
        <div class="console">
          <div class="lcd" aria-live="polite">
            <div class="lcd-meta">
              <span class="lcd-group">USER</span>
              <span class="lcd-coord">{panelLabel(current)}</span>
              <span class="lcd-path">SUB {SUBGROUPS[pos.sub].toUpperCase()} · BANK {pos.bank} · NUMBER {pos.num}</span>
            </div>
            <div class="lcd-main">
              <span class="lcdkey">PATCH {patchNumber(current)}</span>
              <span class="lcdname">{cellName(current)}</span>
              {#if changed.has(current)}<span class="chg">CHANGED</span>{/if}
              {#if canFollow}<span class="follow-pill" title="Connected: selecting a pad sends that sound to the instrument so you can play it. A loaded bulk is sent to temporary memory until you Write the bank.">LIVE</span>{/if}
            </div>
          </div>

          <!-- Owner's Manual p.26 Bank/Number method: SUBGROUP → BANK 1–8 → NUMBER 1–8, columns aligned to the grid. -->
          <div class="rails">
            <div class="rail">
              <span class="rail-lab">Sub</span>
              <div class="oct sub">
                {#each SUBGROUPS as sg, t (sg)}
                  <button
                    type="button"
                    class="pad sub"
                    class:on={viewSub === t}
                    class:guide={dragPreviewSub === t || dropPos?.sub === t}
                    onclick={() => chooseSub(t as 0 | 1)}
                    ondragenter={(e) => previewSub(e, t as 0 | 1)}
                    ondragover={(e) => previewSub(e, t as 0 | 1)}
                    aria-pressed={viewSub === t}
                    title="Drag a patch onto a or b to show that half of the bank, then drop it on a pad"
                  >{sg}</button>
                {/each}
              </div>
            </div>
            <div class="rail">
              <span class="rail-lab">Bank</span>
              <div class="oct">
                {#each ONE_TO_EIGHT as b (b)}
                  <button
                    type="button"
                    class="pad"
                    class:on={pos.bank === b && pos.sub === viewSub && dragPreviewSub === null}
                    class:guide={dropPos?.bank === b}
                    onclick={() => chooseBank(b)}
                    aria-pressed={pos.bank === b && pos.sub === viewSub}
                  >{b}</button>
                {/each}
              </div>
            </div>
            <div class="rail">
              <span class="rail-lab">Num</span>
              <div class="oct">
                {#each ONE_TO_EIGHT as n (n)}
                  <button
                    type="button"
                    class="pad"
                    class:on={pos.num === n}
                    class:guide={dropPos?.num === n}
                    onclick={() => chooseNumber(n)}
                    aria-pressed={pos.num === n}
                  >{n}</button>
                {/each}
              </div>
            </div>
            <div class="stems" aria-hidden="true">
              <span class="rail-lab"></span>
              <div class="oct">
                {#each ONE_TO_EIGHT as n (n)}
                  <span class="stem" class:lit={pos.num === n || dropPos?.num === n}></span>
                {/each}
              </div>
            </div>
          </div>

          <div class="grid" role="group" aria-label="User patches {SUBGROUPS[viewSub]}">
            {#each Array.from({ length: 64 }, (_, k) => viewSub * 64 + k) as i (i)}
              {@const s = bank.slots[i]}
              {@const p = posOf(i)}
              <button
                type="button"
                class="slot"
                class:sel={current === i}
                class:rowsel={current !== i && pos.sub === viewSub && p.bank === pos.bank}
                class:colsel={current !== i && p.num === pos.num}
                class:changed={changed.has(i) || confirmed.has(i)}
                class:empty={cellName(i) === '—'}
                class:dragging={dragFrom === i}
                class:target={dropTarget === i}
                draggable={!bank.locked && !!s?.bytes}
                ondragstart={(e) => {
                  dragFrom = i;
                  dragPreviewSub = null;
                  e.dataTransfer?.setData('text/xp-slot', String(i));
                }}
                ondragend={() => endDrag()}
                ondragenter={(e) => guide(e, i)}
                ondragover={(e) => guide(e, i)}
                ondragleave={(e) => leaveSlot(e, i)}
                ondrop={(e) => drop(e, i)}
                onclick={() => choose(i)}
                ondblclick={() => openSlot(i)}
                aria-label="{patchNumber(i)} {panelLabel(i)}: {cellName(i)}{changed.has(i) ? ', changed' : ''}"
                aria-pressed={current === i}
              >
                <span class="n">{panelLabel(i)} · {patchNumber(i)}</span>
                <span class="nm">{cellName(i)}</span>
              </button>
            {/each}
          </div>
        </div>
        <p class="hint">
          {#if dropTarget !== null}
            Drop to put the patch in <b>{panelLabel(dropTarget)} · {patchNumber(dropTarget)}</b>: sub-group {SUBGROUPS[dropPos?.sub ?? 0]}, BANK {dropPos?.bank}, NUMBER {dropPos?.num}. The lit pads are the ones that pick it on the front panel.
          {:else if dragPreviewSub !== null}
            Showing sub-group <b>{SUBGROUPS[dragPreviewSub]}</b> — drop onto a pad to place the patch there.
          {:else}
            Pick a patch as on the XP-60 front panel: SUB → BANK → NUMBER (arrow keys move too). Drag a slot onto another to swap, or drop a library patch onto a slot. Drag onto <b>a</b> or <b>b</b> to show that half of the bank. Double-click opens it in the editor. Nothing reaches User memory until you write the bank.
          {/if}
        </p>
      </div>
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
          <button type="button" onclick={() => { const next = Math.max(0, bank.selected - 1); bank.swap(bank.selected, next); choose(next); }} disabled={bank.locked || bank.selected === 0}>Move up</button>
          <button type="button" onclick={() => { const next = Math.min(SLOTS - 1, bank.selected + 1); bank.swap(bank.selected, next); choose(next); }} disabled={bank.locked || bank.selected === SLOTS - 1}>Move down</button>
        </div>
      {:else}
        <BulkLibraryPanel {bank} {lib} deviceId={store.deviceId} onput={(slot) => choose(slot)} />
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
    /* One row exactly as tall as the workspace: the library list in the right panel scrolls inside its tab instead of making the slot grid taller. */
    grid-template-rows: minmax(0, 1fr);
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
    min-height: 0;
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
  .panel {
    display: flex;
    flex-direction: column;
    gap: 8px;
    flex: 1;
    min-height: 0;
    outline: none;
  }
  /* Instrument console: recessed plate holding LCD + panel pads + the 8×8 map (XP Bank/Number + FL graphite depth). */
  .console {
    display: flex;
    flex-direction: column;
    gap: 8px;
    flex: 1;
    min-height: 0;
    padding: 8px;
    border: 1px solid var(--edge);
    border-radius: var(--radius-lg);
    background:
      linear-gradient(180deg, color-mix(in srgb, var(--caption) 70%, #1a1f22), color-mix(in srgb, var(--body-deep) 88%, #000));
    box-shadow:
      inset 0 1px 0 rgba(255, 255, 255, 0.05),
      inset 0 -1px 0 rgba(0, 0, 0, 0.35);
  }
  .lcd {
    display: flex;
    flex-direction: column;
    gap: 4px;
    padding: 8px 12px 9px;
    border: 1px solid #0d1215;
    border-radius: var(--radius);
    background: linear-gradient(180deg, #1a2226, #12181b 70%, #0e1316);
    box-shadow:
      inset 0 1px 3px rgba(0, 0, 0, 0.55),
      inset 0 0 0 1px rgba(255, 255, 255, 0.03);
    font-family: var(--font-mono);
  }
  .lcd-meta {
    display: flex;
    align-items: baseline;
    gap: 10px;
    min-width: 0;
  }
  .lcd-group {
    font: 700 10px var(--font-caps);
    letter-spacing: 0.1em;
    color: var(--ok);
  }
  .lcd-coord {
    font-size: 15px;
    font-weight: 700;
    color: var(--accent);
    letter-spacing: 0.04em;
  }
  .lcd-path {
    font: 600 9px var(--font-caps);
    letter-spacing: 0.08em;
    color: var(--text-faint);
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }
  .lcd-main {
    display: flex;
    align-items: baseline;
    gap: 12px;
    min-width: 0;
  }
  .lcdkey {
    color: var(--text-dim);
    font-size: 11px;
    letter-spacing: 0.04em;
    flex: none;
  }
  .lcdname {
    font-size: 18px;
    font-weight: 600;
    color: #e8edf0;
    flex: 1;
    min-width: 0;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }
  .chg {
    flex: none;
    color: var(--warn);
    font: 700 9px var(--font-caps);
    letter-spacing: 0.08em;
  }
  .follow-pill {
    flex: none;
    padding: 1px 6px;
    border-radius: 2px;
    font: 700 9px var(--font-caps);
    letter-spacing: 0.08em;
    color: var(--accent-ink);
    background: color-mix(in srgb, var(--ok) 78%, #fff);
    box-shadow: 0 0 6px color-mix(in srgb, var(--ok) 35%, transparent);
  }
  .rails {
    display: flex;
    flex-direction: column;
    gap: 5px;
  }
  .rail,
  .stems {
    display: grid;
    grid-template-columns: 34px minmax(0, 1fr);
    align-items: center;
    gap: 8px;
  }
  .rail-lab {
    font: 700 9px var(--font-caps);
    letter-spacing: 0.1em;
    text-transform: uppercase;
    color: var(--text-faint);
    text-align: right;
  }
  .oct {
    display: grid;
    grid-template-columns: repeat(8, minmax(0, 1fr));
    gap: 3px;
  }
  /*
   * FL QuickBtn face language only (fl-quickbtn-evidence.md) — not FL pixel sizes.
   * OFF: charcoal gradient. ON: radial lamp + #be5507 rim + brown ink.
   * Latch fade ~60 ms measured live in FL 2025 (~240 fps) — no bounce/outer glow.
   * Width/height stay ours (SUB a/b share the Bank/Num 8-col grid).
   */
  .pad {
    position: relative;
    min-width: 0;
    height: 29px;
    padding: 0;
    border-radius: 2px;
    border: 1px solid #2a3236;
    border-top-color: #6a757b;
    border-left-color: #556166;
    border-right-color: #1e262a;
    border-bottom-color: #141a1d;
    background: linear-gradient(180deg, #596267 0%, #4a5259 55%, #3f484d 100%);
    box-shadow: inset 0 1px 0 rgba(255, 255, 255, 0.1);
    color: #a8b2b8;
    font: 700 11px var(--font-caps);
    letter-spacing: 0.06em;
    text-transform: uppercase;
    cursor: pointer;
    transition: background 60ms linear, border-color 60ms linear, color 60ms linear;
  }
  .pad.sub {
    font-size: 12px;
    letter-spacing: 0.04em;
  }
  .pad:hover:not(:disabled):not(.on):not(.guide) {
    color: #d9e1e5;
    background: linear-gradient(180deg, #657077 0%, #545c63 55%, #485055 100%);
  }
  .pad:active:not(:disabled):not(.on) {
    background: linear-gradient(180deg, #3a4248 0%, #4a5259 100%);
    border-top-color: #1e262a;
    border-bottom-color: #5a656a;
    box-shadow: inset 0 1px 2px rgba(0, 0, 0, 0.45);
  }
  /* ON = backlit lamp (radial), rim #be5507, ink #b14000 — not a linear orange wash. */
  .pad.on {
    color: #b14000;
    border-color: #be5507;
    background: radial-gradient(
      ellipse 92% 88% at 50% 48%,
      #ffffb4 0%,
      #ffda8d 38%,
      #ffb76a 68%,
      #fba65a 100%
    );
    box-shadow: none;
  }
  .pad.on:hover:not(:disabled) {
    background: radial-gradient(
      ellipse 92% 88% at 50% 48%,
      #ffffc0 0%,
      #ffe094 38%,
      #ffc078 68%,
      #ffb060 100%
    );
  }
  .pad:focus-visible {
    outline: none;
    background-image: linear-gradient(var(--focus-tint), var(--focus-tint)),
      linear-gradient(180deg, #596267 0%, #4a5259 55%, #3f484d 100%);
  }
  .pad.on:focus-visible {
    background-image: linear-gradient(var(--focus-tint), var(--focus-tint)),
      radial-gradient(ellipse 92% 88% at 50% 48%, #ffffb4 0%, #ffda8d 38%, #ffb76a 68%, #fba65a 100%);
  }
  .pad:disabled {
    opacity: 0.35;
    cursor: default;
  }
  /* Drag guide: warm wash on the OFF face — never the full ON lamp. */
  .pad.guide:not(.on) {
    color: #d9e1e5;
    border-top-color: #c09058;
    background: linear-gradient(
      180deg,
      color-mix(in srgb, var(--accent) 22%, #657077) 0%,
      color-mix(in srgb, var(--accent) 14%, #4a5259) 100%
    );
  }
  .stem {
    display: block;
    height: 7px;
    margin: 0 10%;
    border-radius: 1px;
    background: linear-gradient(180deg, #1a2023, #2a3337);
    box-shadow: inset 0 1px 1px rgba(0, 0, 0, 0.5);
  }
  .stem.lit {
    background: linear-gradient(180deg, #e09040, #a85818);
    box-shadow: none;
  }
  .grid {
    display: grid;
    grid-template-columns: repeat(8, minmax(0, 1fr));
    grid-template-rows: repeat(8, minmax(40px, 1fr));
    gap: 4px;
    flex: 1;
    min-height: 0;
  }
  .slot.rowsel,
  .slot.colsel {
    background: color-mix(in srgb, var(--accent) 10%, #333c40);
  }
  /*
   * Patch cells follow FL channel-rack name buttons: 1 px raised edge, flat face,
   * hover = slight brighten only (measured channel-rack / menu hover language).
   */
  .slot {
    display: flex;
    flex-direction: column;
    align-items: flex-start;
    gap: 1px;
    padding: 5px 6px;
    min-width: 0;
    text-align: left;
    border-radius: 2px;
    background: linear-gradient(180deg, #3e484c, #333c40);
    border: 1px solid #1a2125;
    border-top-color: #556166;
    border-left-color: #455055;
    border-right-color: #141a1d;
    border-bottom-color: #101418;
    box-shadow: inset 0 1px 0 rgba(255, 255, 255, 0.05);
    color: var(--text);
    cursor: pointer;
  }
  .slot:hover:not(.sel):not(.target) {
    background: linear-gradient(180deg, #4a5559, #3c464a);
    border-top-color: #677277;
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
    border-bottom-color: color-mix(in srgb, var(--warn) 65%, #101418);
  }
  .slot.sel {
    background: linear-gradient(180deg, #ffb45a, #ff8a1f 52%, #c8681a);
    color: var(--accent-ink);
    border-color: #8a4a12;
    border-top-color: #ffd29a;
    box-shadow: inset 0 1px 0 rgba(255, 255, 255, 0.28);
  }
  .slot.sel:hover {
    background: linear-gradient(180deg, #ffc070, #ff952e 52%, #d07020);
  }
  .slot.sel .n {
    color: var(--accent-ink);
  }
  .slot.dragging {
    opacity: 0.45;
  }
  .slot.target,
  .slot.sel.target {
    background: color-mix(in srgb, var(--accent) 38%, #3e484c);
    color: var(--text);
    border-top-color: #c09058;
  }
  .slot.target .n {
    color: var(--text);
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
</style>
