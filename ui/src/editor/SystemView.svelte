<script lang="ts">
  import { GroupFrame, PianoKeys, TabPanel, keySpan, isAccidental } from '../kit';
  import { SCALE_TUNE_BLOCKS } from '../model/catalog';
  import { arrangement, tabIn } from '../model/layouts.svelte';
  import { ui } from '../model/ui.svelte';
  import type { SystemSection } from '../model/layout';
  import { blockTitle, systemSectionTitle } from '../model/labels';
  import { useStore } from './context';
  import HistoryPanel from './HistoryPanel.svelte';
  import SystemPage from './SystemPage.svelte';
  import ToolsPanel from './ToolsPanel.svelte';
  import ParamControl from './ParamControl.svelte';

  const store = useStore();

  const sections: readonly SystemSection[] = ['sound', 'controllers', 'receive', 'keyboard', 'pedals', 'banks', 'preview', 'arpeggio'];

  const tabNames = [...sections.map((s) => systemSectionTitle(s)), 'Scale tune', 'Tools', 'History'];
  const tab = $derived(tabIn(arrangement.systemTab, tabNames.length));
  const TUNE_TAB = sections.length;

  let tune = $state(ui.tune);
  $effect(() => {
    ui.tune = tune;
  });
  $effect(() => {
    tune = ui.tune;
  });
  const tuneBlock = $derived(SCALE_TUNE_BLOCKS[tune] ?? 'tune17');
  const tuneIds = $derived(store.catalog.scaleTune.params.map((p) => p.id));

  // One octave of keys, C to B, with each note's cents knob on its own key: sharps above, naturals below.
  const OCT_W = 616;
  const OCT_H = 96;
  const octave = Array.from({ length: 12 }, (_, i) => 60 + i);
  const unit = OCT_W / 12;
  /** Pixel centre of a key's face along the keyboard. */
  const centre = (note: number): number => {
    const [lo, hi] = keySpan(note);
    return ((lo + hi) / 2 - 60) * unit;
  };
  const NOTES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
</script>

<div class="work">
  <p class="warn" role="note">
    System settings live in the instrument's permanent memory. Edit them here, then write them with the save ceremony
    (backup, confirm, read back). They are never sent live.
  </p>
  <TabPanel tabs={tabNames} selected={tab} name="System sections" meta={tab === TUNE_TAB ? blockTitle(tuneBlock, 'system') : ''} onchange={(i) => (arrangement.systemTab = i)}>
    {#snippet children(i)}
      {#if i < TUNE_TAB}
        <SystemPage section={sections[i] as SystemSection} />
      {:else if i === TUNE_TAB}
        <div class="targets" role="group" aria-label="Scale tune target">
          {#each SCALE_TUNE_BLOCKS as b, n (b)}
            <button type="button" class:sel={tune === n} aria-pressed={tune === n} onclick={() => (tune = n)}>
              {n === 16 ? 'Patch' : `Part ${n + 1}`}
            </button>
          {/each}
        </div>
        <div class="octave" style:width="{OCT_W}px" style:height="{OCT_H + 170}px">
          {#each octave.filter((n) => isAccidental(n)) as note (note)}
            {@const n = note - 60}
            <div class="knob up" style:left="{centre(note)}px">
              <ParamControl block={tuneBlock} id={tuneIds[n] ?? ''} label={NOTES[n]} size={30} />
            </div>
          {/each}
          <div class="piano"><PianoKeys first={60} last={71} width={OCT_W} height={OCT_H} labels={false} /></div>
          {#each octave.filter((n) => !isAccidental(n)) as note (note)}
            {@const n = note - 60}
            <div class="knob down" style:left="{centre(note)}px">
              <ParamControl block={tuneBlock} id={tuneIds[n] ?? ''} label={NOTES[n]} size={30} />
            </div>
          {/each}
        </div>
      {:else if i === TUNE_TAB + 1}
        <ToolsPanel />
      {:else}
        <HistoryPanel />
      {/if}
    {/snippet}
  </TabPanel>
</div>

<style>
  .work {
    display: flex;
    flex-direction: column;
    gap: 8px;
    padding: 10px;
    min-height: 0;
  }
  .work > :global(.tp) {
    flex: 1;
  }
  .warn {
    margin: 0;
    padding: 4px 10px;
    border: 1px solid var(--warn);
    border-radius: var(--radius);
    color: var(--warn);
  }
  .targets {
    display: flex;
    flex-wrap: wrap;
    gap: 3px;
    margin-bottom: 10px;
  }
  .targets button {
    padding: 2px 8px;
    background: var(--well-deep);
    border: 1px solid var(--edge);
    border-radius: 2px;
    cursor: pointer;
    font-family: var(--font-mono);
    font-size: 11px;
  }
  .targets button.sel {
    background: var(--accent);
    color: var(--accent-ink);
  }
  .targets button:focus-visible {
    outline: none;
    box-shadow: var(--focus);
  }
  .octave {
    position: relative;
    margin: 0 auto;
  }
  .piano {
    position: absolute;
    left: 0;
    top: 76px;
    border: 1px solid var(--edge);
  }
  .knob {
    position: absolute;
    transform: translateX(-50%);
  }
  .knob.up {
    top: 0;
  }
  .knob.down {
    top: 182px;
  }
</style>
