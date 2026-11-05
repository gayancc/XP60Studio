<script lang="ts">
  import { HintBar, MessageStack } from '../kit';
  import { blockInfo, isNameCharacter, type Mode } from '../model/catalog';
  import { blockNames } from '../model/kinds';
  import { blockTitle, contextOf } from '../model/labels';
  import { effectiveKeys, loadOverrides, matchCommand, saveOverrides, type KeyMap } from '../model/shortcuts';
  import type { PatchStore } from '../model/store.svelte';
  import type { SynthService } from '../model/synth.svelte';
  import { controls } from '../model/controls.svelte';
  import { carriesFiles, dropMeaning, openPatchFile, previewFile } from '../model/openFile';
  import { ui } from '../model/ui.svelte';
  import CommandPalette, { type CommandInfo, type PaletteItem } from './CommandPalette.svelte';
  import { provideStore } from './context';
  import BankView from './BankView.svelte';
  import LibraryView from './LibraryView.svelte';
  import PatchView from './PatchView.svelte';
  import PerformanceView from './PerformanceView.svelte';
  import RhythmView from './RhythmView.svelte';
  import { provideServices, type Services } from './services';
  import SystemView from './SystemView.svelte';
  import { copySelected, jumpToParam, pasteSelected, runInit, runMutate, runRandomize } from './toolActions';
  import TopBar from './TopBar.svelte';

  interface Props {
    store: PatchStore;
    synth?: SynthService;
    services?: Services;
    mode: Mode;
    onmode: (mode: Mode) => void;
  }

  let { store, synth, services, mode, onmode }: Props = $props();
  // One workspace per document kind: the host re-creates this component when the mode changes.
  // svelte-ignore state_referenced_locally
  provideStore(store);
  // svelte-ignore state_referenced_locally
  provideServices(services);

  const queueState = $derived(store.queue.state === 'failed' ? 'timeout' : store.queue.state === 'behind' ? 'behind' : 'ok');

  // ---- dropping a file anywhere in the window ----

  let dragging = $state(false);

  function dragover(e: DragEvent): void {
    if (!carriesFiles(e.dataTransfer?.types)) return;
    e.preventDefault();
    dragging = true;
  }

  async function drop(e: DragEvent): Promise<void> {
    dragging = false;
    if (!carriesFiles(e.dataTransfer?.types)) return;
    e.preventDefault();
    const files = [...(e.dataTransfer?.files ?? [])];
    const first = files[0];
    if (!first) return;
    const why = dropMeaning(mode);
    if (why) {
      store.error = why;
      return;
    }
    const raw = new Uint8Array(await first.arrayBuffer());
    if (mode === 'library') {
      services?.library.stage(first.name, raw);
    } else if (mode === 'bank') {
      try {
        const { bytes, names } = previewFile(raw);
        services?.bank.loadFile(bytes, first.name, names);
      } catch (err) {
        if (services) services.bank.error = err instanceof Error ? err.message : String(err);
      }
    } else {
      openPatchFile(store, raw, first.name);
    }
  }

  // ---- commands and shortcuts ----

  const modeCommands: Array<[string, string, Mode]> = [
    ['mode.patch', 'Go to Patch', 'patch'],
    ['mode.performance', 'Go to Performance', 'performance'],
    ['mode.rhythm', 'Go to Rhythm set', 'rhythm'],
    ['mode.system', 'Go to System', 'system'],
    ['mode.library', 'Go to Library', 'library'],
    ['mode.bank', 'Go to Bank', 'bank'],
  ];

  const actions: Record<string, { label: string; run: () => void }> = {
    palette: { label: 'Command palette', run: () => (ui.paletteOpen = true) },
    undo: { label: 'Undo', run: () => store.undo() },
    redo: { label: 'Redo', run: () => store.redo() },
    init: { label: 'Init (neutral sound)', run: () => runInit(store) },
    randomize: { label: 'Randomize', run: () => runRandomize(store) },
    mutate: { label: 'Mutate', run: () => runMutate(store) },
    ab: { label: 'Switch A/B', run: () => store.abSwitch() },
    copy: { label: 'Copy tone, part or key', run: () => copySelected(store) },
    paste: { label: 'Paste', run: () => void pasteSelected(store) },
    ...Object.fromEntries(modeCommands.map(([id, label, m]) => [id, { label, run: () => onmode(m) }])),
  };
  const commands: CommandInfo[] = Object.entries(actions).map(([id, a]) => ({ id, label: a.label }));
  const knownIds = new Set(Object.keys(actions));

  let overrides = $state<KeyMap>(loadOverrides(knownIds));
  const keys = $derived(effectiveKeys(overrides));

  function setKeys(next: KeyMap): void {
    overrides = next;
    saveOverrides(next);
  }

  /** Commands that change the sound being edited: they make no sense in the Library and Bank workspaces. */
  const editorCommands = new Set(['undo', 'redo', 'init', 'randomize', 'mutate', 'ab', 'copy', 'paste']);

  function onkeydown(e: KeyboardEvent): void {
    if (e.key === 'Escape' && controls.learning) {
      controls.cancelLearn();
      return;
    }
    const id = matchCommand(keys, e);
    if (ui.paletteOpen) {
      // The palette's own shortcut closes it again.
      if (id === 'palette') {
        e.preventDefault();
        ui.paletteOpen = false;
      }
      return;
    }
    if (!id) return;
    const target = e.target;
    const typing =
      target instanceof HTMLElement && (target.isContentEditable || target.matches('input, textarea, select'));
    // While typing, only the palette and the workspace keys (function keys) act; the rest belong to the text box.
    if (typing && id !== 'palette' && !id.startsWith('mode.')) return;
    if (editorCommands.has(id) && (mode === 'library' || mode === 'bank')) return;
    e.preventDefault();
    actions[id]?.run();
  }

  // Parameters are searchable too: "tvf cutoff" finds Cutoff Frequency and jumps to it.
  const paramItems = $derived.by((): PaletteItem[] => {
    if (!ui.paletteOpen || mode === 'library' || mode === 'bank') return [];
    const kind = store.kind;
    const out: PaletteItem[] = [];
    for (const block of blockNames(kind)) {
      for (const p of blockInfo(store.catalog, kind, block).params) {
        if (isNameCharacter(p)) continue;
        out.push({
          id: `${block}:${p.id}`,
          label: `${blockTitle(block, kind)} · ${p.name}`,
          hint: contextOf(block, p.id, kind),
          run: () => jumpToParam(kind, block, p.id),
        });
      }
    }
    return out;
  });

  const items = $derived<PaletteItem[]>([
    ...Object.entries(actions).map(([id, a]) => ({ id: `cmd:${id}`, label: a.label, hint: keys[id] || 'command', run: a.run })),
    ...paramItems,
  ]);
</script>

<svelte:window {onkeydown} />

<div class="app" class:dragging ondragover={dragover} ondragleave={() => (dragging = false)} ondrop={drop} role="presentation">
  <TopBar {synth} {mode} {onmode} {keys} run={(id) => actions[id]?.run()} />

  <div class="stage">
  {#if mode === 'patch'}
    <PatchView {synth} />
  {:else if mode === 'performance'}
    <PerformanceView />
  {:else if mode === 'rhythm'}
    <RhythmView />
  {:else if mode === 'system'}
    <SystemView />
  {:else if mode === 'library'}
    <LibraryView onopened={() => onmode('patch')} />
  {:else}
    <BankView onopened={() => onmode('patch')} />
  {/if}
  </div>

  <div class="foot">
    <HintBar
      sound={store.sync}
      midiIn={synth?.connected ?? false}
      midiOut={store.queue.sent > 0}
      queued={store.queue.pending}
      {queueState}
    />
  </div>
</div>

<MessageStack />

<CommandPalette
  open={ui.paletteOpen}
  {items}
  {commands}
  {keys}
  onclose={() => (ui.paletteOpen = false)}
  onkeys={setKeys}
/>

<style>
  .app {
    height: 100vh;
    display: grid;
    grid-template-rows: auto minmax(0, 1fr) auto;
    overflow: hidden;
  }
  .app.dragging {
    outline: 2px dashed var(--accent);
    outline-offset: -4px;
  }
  .stage {
    min-height: 0;
    min-width: 0;
    overflow: hidden;
    display: flex;
  }
  .stage > :global(*) {
    flex: 1;
    min-width: 0;
    min-height: 0;
  }
</style>
