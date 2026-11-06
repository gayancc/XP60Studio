<script lang="ts">
  import { onDestroy, onMount } from 'svelte';
  import { Meter, Stepper, TabPanel, ToggleLed } from '../kit';
  import { SLOTS } from '../model/bank.svelte';
  import { PATCH_COUNT, PER_BANK, panelLabel, patchNumber, posOf, slotOf, soundSelect, SUBGROUPS } from '../model/bankPanel';
  import { boardName, factoryPatchName, PATCH_GROUPS } from '../model/factory';
  import { expansions } from '../model/expansions.svelte';
  import { PatchStore } from '../model/store.svelte';
  import { tauriBackend, type BackupFile } from '../model/backend';
  import { arrangement, tabIn } from '../model/layouts.svelte';
  import { asSyx, toMidiFile } from '../model/midiFile';
  import { errorText, reportError } from '../model/messages.svelte';
  import { saveFile } from '../model/saveFile';
  import { say } from '../model/ui.svelte';
  import type { SynthService } from '../model/synth.svelte';
  import { useStore } from './context';
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
  // The front panel: patch group, then sub-group a or b, then BANK 1-8 and NUMBER 1-8 (Owner's Manual p.26).
  // Only USER is the bank being edited here; the presets are fixed ROM, shown by name; expansion boards depend on what is installed.
  // XP-A to XP-D are the boards ticked in Settings, in slot order; a slot with no board is greyed out.
  const GROUP_BUTTONS = $derived([
    ...PATCH_GROUPS.map((g) => ({ key: g.key, label: g.label.replace(' (GM)', ''), fixed: g.fixedBank, board: null as number | null, enabled: true })),
    ...['XP-A', 'XP-B', 'XP-C', 'XP-D'].map((k, i) => ({ key: k, label: k, fixed: null, board: expansions.slots[i] ?? null, enabled: expansions.slots[i] !== undefined })),
  ]);
  const ONE_TO_EIGHT = [1, 2, 3, 4, 5, 6, 7, 8];
  let group = $state(0);
  let presetSlot = $state(0);
  let sub = $state<0 | 1>(0);
  const inUser = $derived(group === 0);
  const current = $derived(bank ? (inUser ? bank.selected : presetSlot) : 0);
  const pos = $derived(posOf(current));
  const fixedBank = $derived(GROUP_BUTTONS[group]?.fixed ?? null);
  const board = $derived(GROUP_BUTTONS[group]?.board ?? null);
  const boardLabel = $derived(board === null ? '' : boardName(board) ?? `Board ${board}`);

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
    if (inUser) {
      if (bank) bank.selected = next;
    } else {
      presetSlot = next;
    }
    const sel = soundSelect(GROUP_BUTTONS[group]?.key ?? '', next);
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
    if (inUser) {
      const s = bank?.slots[i];
      return s?.bytes ? s.name.trimEnd() || '(unnamed)' : '—';
    }
    return (fixedBank ? factoryPatchName(fixedBank, i + 1) : undefined)?.trimEnd() || '—';
  };
  // Roland's patch names for a board are not held yet, so an expansion patch is shown by its number and the board that holds it.
  const lcdName = $derived(board !== null ? boardLabel : cellName(current));
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
      <!-- svelte-ignore a11y_no_noninteractive_element_interactions -->
      <div class="panel" role="group" aria-label="Patch selection" tabindex="-1" onkeydown={panelKey}>
        <div class="lcd" aria-live="polite">
          <span class="lcdkey">{GROUP_BUTTONS[group]?.label}:{patchNumber(current)}</span>
          <span class="lcdname">{lcdName}</span>
          <span class="lcdloc">[{panelLabel(current)}]</span>
          {#if board !== null}<span class="rom">Expansion board (read-only)</span>{:else if !inUser}<span class="rom">Read-only preset</span>{:else if changed.has(current)}<span class="chg">changed</span>{/if}
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
            <span class="klabel">Patch group</span>
            {#each GROUP_BUTTONS as g, gi (g.key)}
              <button
                type="button"
                class="pad"
                class:on={group === gi}
                disabled={!g.enabled}
                title={g.board !== null ? `SR-JV80-${String(g.board).padStart(2, '0')} ${boardName(g.board) ?? ''}, read-only` : g.enabled ? (gi === 0 ? 'Your User memory: the bank on this screen' : 'Built-in preset, read-only') : 'No board ticked for this slot: tick the boards installed in your keyboard in Settings, in slot order'}
                onclick={() => (group = gi)}>{g.label}{#if g.board !== null}<small>{boardName(g.board) ?? g.board}</small>{/if}</button>
            {/each}
          </div>
          <div class="krow">
            <span class="klabel">Sub-group</span>
            {#each SUBGROUPS as sg, t (sg)}
              <button type="button" class="pad" class:on={sub === t} onclick={() => chooseSub(t as 0 | 1)}>{sg}</button>
            {/each}
            <span class="klabel gap">Bank</span>
            {#each ONE_TO_EIGHT as b (b)}
              <button type="button" class="pad num" class:on={pos.bank === b && pos.sub === sub} onclick={() => chooseBank(b)}>{b}</button>
            {/each}
            <span class="klabel gap">Number</span>
            {#each ONE_TO_EIGHT as n (n)}
              <button type="button" class="pad num" class:on={pos.num === n} onclick={() => chooseNumber(n)}>{n}</button>
            {/each}
          </div>
        </div>

        <div class="grid" role="group" aria-label="{GROUP_BUTTONS[group]?.label} patches {SUBGROUPS[sub]}">
          {#each Array.from({ length: 64 }, (_, k) => sub * 64 + k) as i (i)}
            {@const s = bank.slots[i]}
            {@const p = posOf(i)}
            <button
              type="button"
              class="slot"
              class:sel={current === i}
              class:rowsel={current !== i && pos.sub === sub && p.bank === pos.bank}
              class:colsel={current !== i && p.num === pos.num}
              class:changed={inUser && (changed.has(i) || confirmed.has(i))}
              class:empty={board === null && cellName(i) === '—'}
              class:dragging={dragFrom === i}
              draggable={inUser && !bank.locked && !!s?.bytes}
              ondragstart={(e) => {
                dragFrom = i;
                e.dataTransfer?.setData('text/xp-slot', String(i));
              }}
              ondragend={() => (dragFrom = null)}
              ondragover={(e) => {
                const t = e.dataTransfer?.types ?? [];
                if (inUser && !bank.locked && (t.includes('text/xp-slot') || t.includes('text/xp-library'))) e.preventDefault();
              }}
              ondrop={(e) => inUser && drop(e, i)}
              onclick={() => choose(i)}
              ondblclick={() => inUser && openSlot(i)}
              aria-label="{GROUP_BUTTONS[group]?.label} {patchNumber(i)} {panelLabel(i)}: {cellName(i)}{inUser && changed.has(i) ? ', changed' : ''}"
              aria-pressed={current === i}
            >
              <span class="n">{panelLabel(i)} · {patchNumber(i)}</span>
              <span class="nm">{cellName(i)}</span>
            </button>
          {/each}
        </div>
        <p class="hint">
          {#if inUser}
            Pick a patch with BANK and NUMBER as on the front panel, or click it (arrow keys move too). Drag a slot onto another to swap them, or drag a library patch onto a slot. Double-click opens it in the editor. Nothing reaches the synth until you write the bank.
          {:else if board !== null}
            This is the {boardLabel} board in {GROUP_BUTTONS[group]?.label.replace('XP', 'EXP')}. Its patches are fixed in the board and only sound while it is installed; they are shown by number because their names are not held yet. Select one to hear it on the instrument.
          {:else}
            Preset patches are built into every XP-60 and cannot be changed. To use one, load it into the editor from the instrument, change it, and save the copy to User memory.
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
  .rom {
    color: var(--text-dim);
    font-size: 11px;
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
  .pad small {
    display: block;
    font: 500 9px var(--font-caps);
    opacity: 0.8;
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
