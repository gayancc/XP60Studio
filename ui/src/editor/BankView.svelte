<script lang="ts">
  import { onDestroy, onMount } from 'svelte';
  import { Meter, Stepper, TabPanel, ToggleLed } from '../kit';
  import { SLOTS } from '../model/bank.svelte';
  import { PATCH_COUNT, PER_BANK, panelLabel, patchNumber, posOf, slotOf, soundSelect, SUBGROUPS } from '../model/bankPanel';
  import { PatchStore } from '../model/store.svelte';
  import { tauriBackend, type BackupFile } from '../model/backend';
  import { arrangement, tabIn } from '../model/layouts.svelte';
  import { asSyx, toMidiFile } from '../model/midiFile';
  import { previewFile } from '../model/openFile';
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

  let fileInput: HTMLInputElement | undefined = $state();
  let dragFrom = $state<number | null>(null);
  /** The slot a patch being dragged is over right now. While it is set, the front-panel buttons that pick that slot are lit as a guide. */
  let dropTarget = $state<number | null>(null);
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

  async function pickFile(e: Event & { currentTarget: HTMLInputElement }): Promise<void> {
    const file = e.currentTarget.files?.[0];
    e.currentTarget.value = '';
    if (!file || !bank) return;
    try {
      const { bytes, names } = previewFile(new Uint8Array(await file.arrayBuffer()), file.name);
      bank.loadFile(bytes, file.name, names);
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

  async function putLibraryPatch(id: number, slot: number): Promise<void> {
    const bytes = await lib?.fetchBlocks(id);
    if (bytes) bank?.assign(slot, bytes, 'library');
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

  function drop(e: DragEvent, slot: number): void {
    e.preventDefault();
    if (bank?.locked) return;
    const payload = parsePayload(e.dataTransfer?.getData(DRAG_TYPE));
    const lib = e.dataTransfer?.getData('text/xp-library');
    const from = e.dataTransfer?.getData('text/xp-slot');
    if (payload?.type === 'bulk') {
      // A whole bulk is asked about first: it replaces every slot. The question is on the Library tab.
      if (services?.library) services.library.bulks.pendingLoad = payload.bulk;
      arrangement.bankTab = 1;
    } else if (payload) {
      // One patch goes in the slot; several go into the slots that follow it.
      payload.items.forEach((item, i) => {
        if (item.kind !== 'bank-slot' && slot + i < PATCH_COUNT) void putLibraryPatch(item.patchId, slot + i);
      });
    } else if (lib) void putLibraryPatch(Number(lib), slot);
    else if (from !== undefined && from !== '') bank?.swap(Number(from), slot);
    // Same path as a click: the landing slot stays selected, and with "Select on the instrument" on the keyboard follows it too.
    if (payload?.type !== 'bulk') choose(slot);
    dragFrom = null;
    dropTarget = null;
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

  // Choosing a patch from anywhere (a click, an arrow key, the slot list) brings its sub-group up.
  $effect(() => {
    sub = posOf(current).sub;
  });

  // Pressing a patch can select it on the instrument as well, the way an external keyboard does (Bank Select and Program Change,
  // Owner's Manual p.181). Nothing is written to memory; the instrument's temporary sound is replaced.
  let followSynth = $state(true);
  let midiChannel = $state(0);
  const canFollow = $derived(!!synth?.connected);

  function choose(slot: number): void {
    const next = Math.min(PATCH_COUNT - 1, Math.max(0, slot));
    if (bank) bank.selected = next;
    const sel = soundSelect('USER', next);
    if (followSynth && canFollow && sel) void synth?.selectSound(midiChannel, sel.msb, sel.lsb, sel.program);
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
          <button type="button" onclick={() => fileInput?.click()} disabled={bank.locked}>Load .syx / .mid…</button>
          <input bind:this={fileInput} type="file" accept=".syx,.mid,.midi" hidden onchange={pickFile} aria-label="Load a .syx or .mid bank file" />
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
        <div class="lcd" aria-live="polite">
          <span class="lcdkey">USER:{patchNumber(current)}</span>
          <span class="lcdname">{cellName(current)}</span>
          <span class="lcdloc">[{panelLabel(current)}]</span>
          {#if changed.has(current)}<span class="chg">changed</span>{/if}
        </div>

        <div class="follow">
          <ToggleLed
            checked={followSynth}
            label="Select on the instrument"
            name="Select the patch on the instrument too"
            context="Bank"
            disabled={!canFollow}
            onchange={(on) => (followSynth = on)}
          />
          <Stepper bind:value={midiChannel} min={0} max={15} label="Channel" name="Patch receive channel" context="Bank" format={(v) => String(v + 1)} disabled={!canFollow || !followSynth} />
          <span class="dim">{canFollow ? (followSynth ? 'Sends Bank Select and Program Change; the instrument must receive them (System, MIDI).' : 'Off: only this screen changes.') : 'Connect the instrument to select patches on it.'}</span>
        </div>

        <div class="keys">
          <div class="krow">
            <span class="klabel">Sub-group</span>
            {#each SUBGROUPS as sg, t (sg)}
              <button type="button" class="pad" class:on={sub === t} class:guide={dropPos?.sub === t} onclick={() => chooseSub(t as 0 | 1)}>{sg}</button>
            {/each}
            <span class="klabel gap">Bank</span>
            {#each ONE_TO_EIGHT as b (b)}
              <button type="button" class="pad num" class:on={pos.bank === b && pos.sub === sub} class:guide={dropPos?.bank === b} onclick={() => chooseBank(b)}>{b}</button>
            {/each}
            <span class="klabel gap">Number</span>
            {#each ONE_TO_EIGHT as n (n)}
              <button type="button" class="pad num" class:on={pos.num === n} class:guide={dropPos?.num === n} onclick={() => chooseNumber(n)}>{n}</button>
            {/each}
          </div>
        </div>

        <div class="grid" role="group" aria-label="User patches {SUBGROUPS[sub]}">
          {#each Array.from({ length: 64 }, (_, k) => sub * 64 + k) as i (i)}
            {@const s = bank.slots[i]}
            {@const p = posOf(i)}
            <button
              type="button"
              class="slot"
              class:sel={current === i}
              class:rowsel={current !== i && pos.sub === sub && p.bank === pos.bank}
              class:colsel={current !== i && p.num === pos.num}
              class:changed={changed.has(i) || confirmed.has(i)}
              class:empty={cellName(i) === '—'}
              class:dragging={dragFrom === i}
              class:target={dropTarget === i}
              draggable={!bank.locked && !!s?.bytes}
              ondragstart={(e) => {
                dragFrom = i;
                e.dataTransfer?.setData('text/xp-slot', String(i));
              }}
              ondragend={() => {
                dragFrom = null;
                dropTarget = null;
              }}
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
        <p class="hint">
          {#if dropTarget !== null}
            Drop to put the patch in <b>{panelLabel(dropTarget)} · {patchNumber(dropTarget)}</b>: sub-group {SUBGROUPS[dropPos?.sub ?? 0]}, BANK {dropPos?.bank}, NUMBER {dropPos?.num}. The lit buttons are the ones that pick it on the front panel.
          {:else}
            Pick a patch with BANK and NUMBER as on the front panel, or click it (arrow keys move too). Drag a slot onto another to swap them, or drag a library patch onto a slot. Double-click opens it in the editor. Nothing reaches the synth until you write the bank.
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
        <BulkLibraryPanel {bank} {lib} deviceId={store.deviceId} />
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
  .lcd {
    display: flex;
    align-items: baseline;
    gap: 14px;
    padding: 8px 12px;
    background: var(--track);
    border: 1px solid var(--edge);
    border-radius: var(--radius);
    font-family: var(--font-mono);
  }
  .lcdkey {
    color: var(--text-dim);
    font-size: 13px;
  }
  .lcdname {
    font-size: 20px;
    font-weight: 600;
    color: var(--accent);
    flex: 1;
    min-width: 0;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }
  .lcdloc {
    color: var(--text-dim);
  }
  .chg {
    color: var(--warn);
    font-size: 11px;
  }
  .follow {
    display: flex;
    align-items: flex-end;
    gap: 12px;
    flex-wrap: wrap;
  }
  .keys {
    display: flex;
    flex-direction: column;
    gap: 6px;
  }
  .krow {
    display: flex;
    align-items: center;
    flex-wrap: wrap;
    gap: 5px;
  }
  .klabel {
    font: 600 10px var(--font-caps);
    text-transform: uppercase;
    color: var(--text-dim);
    min-width: 74px;
  }
  .klabel.gap {
    min-width: 0;
    margin-left: 14px;
  }
  .pad {
    min-width: 40px;
    padding: 6px 8px;
    font-family: var(--font-mono);
    font-weight: 600;
  }
  .pad.num {
    min-width: 34px;
  }
  .pad.on {
    background: var(--accent);
    color: var(--accent-ink);
    border-color: var(--accent-soft);
  }
  .pad:disabled {
    opacity: 0.35;
    cursor: default;
  }
  /* While a patch is dragged over a slot, the panel buttons that pick that slot are tinted as a guide (a tint, never a ring, and it pulses softly). */
  .pad.guide:not(.on),
  .pad.guide.on {
    background: color-mix(in srgb, var(--accent) 62%, var(--well-deep));
    color: var(--text);
    animation: guide-pulse 0.9s ease-in-out infinite alternate;
  }
  @keyframes guide-pulse {
    from {
      background: color-mix(in srgb, var(--accent) 38%, var(--well-deep));
    }
    to {
      background: color-mix(in srgb, var(--accent) 72%, var(--well-deep));
    }
  }
  @media (prefers-reduced-motion: reduce) {
    .pad.guide {
      animation: none;
    }
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
    background: color-mix(in srgb, var(--accent) 12%, var(--well-deep));
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
  .slot.target,
  .slot.sel.target {
    background: color-mix(in srgb, var(--accent) 52%, var(--well-deep));
    color: var(--text);
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
