<script lang="ts">
  import { MenuBar, Tabs, ToggleLed, XpReadout, type Menu } from '../kit';
  import { hint } from '../kit/hint.svelte';
  import { SYNC_STATES } from '../kit/syncState';
  import type { Mode } from '../model/catalog';
  import { KIND_TITLES } from '../model/kinds';
  import { exportPatch, type ExportFormat } from '../model/exportPatch';
  import { canStepBank, openBankSlot, stepBankSlot } from '../model/bankNav';
  import { openPatchFile } from '../model/openFile';
  import type { SynthService } from '../model/synth.svelte';
  import type { KeyMap } from '../model/shortcuts';
  import { describeSound } from '../model/textExport';
  import { say, ui } from '../model/ui.svelte';
  import { applyTheme, storedTheme, toggled, type Theme } from '../theme/theme';
  import { useStore } from './context';
  import SaveCeremony from './SaveCeremony.svelte';
  import { tauriBackend } from '../model/backend';
  import { controls } from '../model/controls.svelte';
  import ControlMapPanel from './ControlMapPanel.svelte';
  import LayoutPanel from './LayoutPanel.svelte';
  import StoragePanel from './StoragePanel.svelte';
  import ExpansionPanel from './ExpansionPanel.svelte';
  import SynthBar from './SynthBar.svelte';
  import WindowControls from './WindowControls.svelte';

  interface Props {
    synth?: SynthService;
    mode: Mode;
    onmode: (mode: Mode) => void;
    keys: KeyMap;
    /** Run a workspace command by id (undo, randomize, go to a workspace, ...). */
    run: (id: string) => void;
  }

  let { synth, mode, onmode, keys, run }: Props = $props();
  const modes: readonly Mode[] = ['patch', 'performance', 'rhythm', 'system', 'library', 'bank'];
  const titles: Record<Mode, string> = { ...KIND_TITLES, library: 'Library', bank: 'Bank' };
  const store = useStore();
  const editing = $derived(mode !== 'library' && mode !== 'bank');
  const livePossible = $derived(editing && mode !== 'system');

  let theme = $state<Theme>(storedTheme());
  let input: HTMLInputElement | undefined = $state();

  async function pick(e: Event & { currentTarget: HTMLInputElement }): Promise<void> {
    const file = e.currentTarget.files?.[0];
    e.currentTarget.value = '';
    if (!file) return;
    openPatchFile(store, new Uint8Array(await file.arrayBuffer()), file.name);
  }

  async function exportOpen(format: ExportFormat): Promise<void> {
    say(await exportPatch(store, tauriBackend, format));
  }

  function choose(i: number): void {
    openBankSlot(store, i);
  }

  const k = (id: string): string => keys[id] ?? '';

  // The status readout. While a control is under the pointer it shows that control's value and name, the way the hardware shows
  // the parameter you are touching; otherwise it shows what is open, how it stands and the instrument link.
  const READOUT_TITLES: Record<Mode, string> = { patch: 'PATCH', performance: 'PERFORM', rhythm: 'RHYTHM SET', system: 'SYSTEM', library: 'LIBRARY', bank: 'BANK BUILDER' };
  const readout = $derived.by(() => {
    const touching = hint.name !== '';
    const link = synth?.connected ? `XP-60 · ID ${synth.info?.device_number ?? '?'}` : 'No instrument';
    const slot = ui.bank && mode === 'patch' ? `#${String(ui.bankSlot + 1).padStart(3, '0')}` : '';
    const pending = store.queue.pending + store.queue.inFlight;
    return {
      title: READOUT_TITLES[mode],
      badge: editing ? SYNC_STATES[store.sync].label : '',
      badgeColor: SYNC_STATES[store.sync].color,
      hovering: touching,
      linked: !!synth?.connected,
      main: touching ? (hint.value || hint.raw || hint.name) : store.name.trimEnd() || 'INIT NAME',
      tag: touching ? '' : slot,
      status: touching ? hint.name : link + (livePossible && store.live ? ' · live' : ''),
      status2: touching || pending === 0 ? '' : `TX ${pending}`,
      busy: pending > 0,
    };
  });

  async function copyText(): Promise<void> {
    try {
      await navigator.clipboard.writeText(describeSound(store));
      say(`Copied "${store.name.trimEnd() || 'the sound'}" as text.`);
    } catch {
      store.error = 'The text could not be copied: the system clipboard is not available.';
    }
  }

  const menus = $derived<Menu[]>([
    {
      title: 'File',
      items: [
        { label: 'New', run: () => store.newPatch(), disabled: !editing },
        { label: 'Open .syx or .mid…', run: () => input?.click(), disabled: mode !== 'patch' },
        { separator: true },
        { label: 'Export patch as .syx…', run: () => void exportOpen('syx'), disabled: mode !== 'patch' },
        { label: 'Export patch as .mid…', run: () => void exportOpen('mid'), disabled: mode !== 'patch' },
      ],
    },
    {
      title: 'Edit',
      items: [
        { label: 'Undo', hint: k('undo'), run: () => run('undo'), disabled: !store.canUndo || !editing },
        { label: 'Redo', hint: k('redo'), run: () => run('redo'), disabled: !store.canRedo || !editing },
        { separator: true },
        { label: 'Init (neutral sound)', hint: k('init'), run: () => run('init'), disabled: !editing },
        { label: 'Randomize', hint: k('randomize'), run: () => run('randomize'), disabled: !editing },
        { label: 'Mutate', hint: k('mutate'), run: () => run('mutate'), disabled: !editing },
        { label: 'Switch A/B', hint: k('ab'), run: () => run('ab'), disabled: !editing },
        { separator: true },
        { label: 'Copy', hint: k('copy'), run: () => run('copy'), disabled: !editing },
        { label: 'Paste', hint: k('paste'), run: () => run('paste'), disabled: !editing },
        { label: 'Copy as text', run: () => void copyText(), disabled: !editing },
      ],
    },
    ...(synth ? [{ title: 'Synth', panel: synthPanel } satisfies Menu] : []),
    ...(synth ? [{ title: 'Controllers', panel: controllersPanel } satisfies Menu] : []),
    ...(synth ? [{ title: 'Settings', panel: settingsPanel } satisfies Menu] : []),
    { title: 'Layouts', panel: layoutsPanel },
    {
      title: 'View',
      items: [
        { label: 'Command palette and shortcuts…', hint: k('palette'), run: () => run('palette') },
        { separator: true },
        { label: 'Light theme', checked: theme === 'light', run: () => applyTheme((theme = toggled(theme))) },
      ],
    },
  ]);
</script>

{#snippet synthPanel()}
  {#if synth}<SynthBar {synth} />{/if}
{/snippet}

{#snippet layoutsPanel()}
  <LayoutPanel />
{/snippet}

{#snippet controllersPanel()}
  <ControlMapPanel />
{/snippet}

{#snippet settingsPanel()}
  <div class="settings">
    <ExpansionPanel />
    <StoragePanel backend={tauriBackend} />
  </div>
{/snippet}

<!-- The native title bar is off: the empty parts of the header drag the window, and a double-click on them maximizes it. -->
<header class="top" data-tauri-drag-region>
  <div class="cols" data-tauri-drag-region>
  <div class="rows" data-tauri-drag-region>
  <div class="row" data-tauri-drag-region>
    <h1 data-tauri-drag-region>XPPatchStudio</h1>
    <MenuBar {menus} name="Main menu" />
    <span class="state" data-tauri-drag-region class:ok={synth?.connected}>
      {#if synth}{synth.connected ? `XP-60 · ID ${synth.info?.device_number}` : 'No instrument'}{:else}Browser preview{/if}
    </span>
    <div class="spacer" data-tauri-drag-region></div>
    {#if ui.notice}<span class="note" role="status">{ui.notice}</span>{/if}
    {#if controls.learning}<span class="learn" role="status">Move a controller to link it to {controls.learning.name} (Esc cancels)</span>{:else if controls.notice}<span class="note" role="status">{controls.notice}</span>{/if}
    {#if synth?.error}<span class="err" role="alert" title={synth.error}>Synth: {synth.error}</span>{:else if synth?.notice}<span class="note" role="status">{synth.notice}</span>{/if}
    {#if store.error}<span class="err" role="alert">{store.error}</span>{/if}
    {#if ui.bank && mode === 'patch'}
      <label class="slot">
        <span class="caps">Bank</span>
        <button type="button" class="step" disabled={!canStepBank(-1)} onclick={() => stepBankSlot(store, -1)} aria-label="Previous patch" title="Previous patch">‹</button>
        <select aria-label="Patch in {ui.bank.file}" onchange={(e) => choose(e.currentTarget.selectedIndex)}>
          {#each ui.bank.names as [s, n], i (s)}
            <option selected={i === ui.bankSlot}>{String(s + 1).padStart(3, '0')}  {n.trimEnd()}</option>
          {/each}
        </select>
        <button type="button" class="step" disabled={!canStepBank(1)} onclick={() => stepBankSlot(store, 1)} aria-label="Next patch" title="Next patch">›</button>
      </label>
    {/if}
    <span class="origin" title="Where the open sound came from">{store.origin}</span>
  </div>
  <div class="row" data-tauri-drag-region>
    <Tabs
      tabs={modes.map((m) => titles[m])}
      selected={modes.indexOf(mode)}
      name="What to edit"
      variant="pill"
      onchange={(i) => onmode(modes[i] ?? 'patch')}
    />
    <div class="spacer" data-tauri-drag-region></div>
    {#if editing}
      <button type="button" disabled={!store.canUndo} onclick={() => store.undo()}>Undo</button>
      <button type="button" disabled={!store.canRedo} onclick={() => store.redo()}>Redo</button>
    {/if}
    {#if livePossible}
      <ToggleLed checked={store.live} label="Live edit" name="Live edit to the temporary patch" context="Synth" onchange={(on) => store.setLive(on)} />
      <button
        type="button"
        disabled={!synth?.connected}
        onclick={() => store.sendPatch()}
        title={synth?.connected ? "Send everything to the instrument's temporary copy" : 'No instrument connected: connect the XP-60 in the Synth menu first'}
      >Send to synth</button>
    {/if}
    {#if mode === 'patch'}<SaveCeremony />{/if}
  </div>
  </div>
  <XpReadout {...readout} />
  <WindowControls />
  </div>
  <input bind:this={input} type="file" accept=".syx,.mid,.midi" hidden onchange={pick} aria-label="Open a .syx or .mid file" />
</header>

<style>
  .settings {
    display: flex;
    flex-direction: column;
    gap: 14px;
  }
  .cols {
    display: flex;
    align-items: center;
    gap: 10px;
  }
  .rows {
    flex: 1;
    min-width: 0;
    overflow: hidden;
    display: flex;
    flex-direction: column;
    gap: 4px;
  }
  .top {
    display: block;
    min-width: 0;
    padding: 4px 10px 5px;
    background: var(--caption);
    border-bottom: 1px solid var(--edge);
    position: relative;
    z-index: 800;
  }
  .row {
    display: flex;
    align-items: center;
    gap: 10px;
    min-height: 24px;
  }
  h1 {
    margin: 0 6px 0 0;
    font: 600 13px var(--font-caps);
    letter-spacing: 0.06em;
    color: var(--accent);
  }
  button,
  select {
    white-space: nowrap;
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
  button:focus-visible,
  select:focus-visible {
    outline: none;
    box-shadow: var(--focus);
  }
  button:disabled {
    opacity: 0.4;
    cursor: default;
  }
  /* The window controls must stay on screen at the smallest window, whatever the first row holds: so the header may never grow
     past the window, and what can give way (the bank picker, the origin text, notices) shrinks first. */
  .slot {
    display: flex;
    align-items: center;
    gap: 5px;
    flex: 0 1 190px;
    min-width: 70px;
  }
  .slot select {
    flex: 1;
    min-width: 0;
    text-overflow: ellipsis;
  }
  .slot .caps {
    flex: none;
  }
  .spacer {
    flex: 1;
    min-width: 0;
  }
  .state {
    flex: none;
    white-space: nowrap;
    color: var(--text-dim);
    font-size: 11px;
  }
  .state.ok {
    color: var(--ok);
  }
  .err {
    flex: 0 1 auto;
    min-width: 0;
    color: var(--bad);
    max-width: 380px;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }
  .learn,
  .note {
    flex: 0 1 auto;
    min-width: 0;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }
  .learn {
    color: var(--accent);
  }
  .note {
    color: var(--ok);
  }
  .origin {
    flex: 0 1 auto;
    min-width: 0;
    color: var(--text-dim);
    max-width: 240px;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }
  .step {
    width: 22px;
    padding: 1px 0;
  }
</style>
