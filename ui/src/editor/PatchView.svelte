<script lang="ts">
  import { DockPanel, Segmented, TabPanel } from '../kit';
  import { TONE_BLOCKS } from '../model/catalog';
  import { arrangement, tabIn } from '../model/layouts.svelte';
  import { ui } from '../model/ui.svelte';
  import CommonPanel from './CommonPanel.svelte';
  import ControllerMatrix from './ControllerMatrix.svelte';
  import { useStore } from './context';
  import HistoryPanel from './HistoryPanel.svelte';
  import ToneCard from './ToneCard.svelte';
  import TonePanel, { type ToneGroup } from './TonePanel.svelte';
  import OverviewPanel from './OverviewPanel.svelte';
  import RoutingPanel from './RoutingPanel.svelte';
  import ZonesPanel from './ZonesPanel.svelte';
  import AllTonesPanel from './AllTonesPanel.svelte';
  import BankListPanel from './BankListPanel.svelte';
  import ToolsPanel from './ToolsPanel.svelte';
  import AuditionPanel from './AuditionPanel.svelte';
  import type { SynthService } from '../model/synth.svelte';
  import { toneParams, type ToneSection } from '../model/layout';
  import { toneSectionTitle } from '../model/labels';

  interface Props {
    synth?: SynthService;
  }

  let { synth }: Props = $props();
  const store = useStore();

  const colors = ['var(--tone-1)', 'var(--tone-2)', 'var(--tone-3)', 'var(--tone-4)'] as const;
  let selected = $state(ui.tone);
  $effect(() => {
    ui.tone = selected;
  });
  // A jump from the command palette changes the selection from outside.
  $effect(() => {
    selected = ui.tone;
  });

  // The big area: what is shaped for the selected Tone, then the settings of the whole patch.
  // Each tab gathers what belongs together in the signal path, and fits the smallest window without a scroll bar.
  //   Zones: where each Tone plays (keys, velocity).
  //   Wave and pitch: the sound source. Filter and amp: the two stages that shape it. LFO and modulation: what moves it.
  //   Patch: the settings of the whole patch, including its controllers. Structure: how the Tones are joined. Routing: where each Tone's sound goes (output and
  //   effect sends) and the path it takes. Effects: the effect, chorus and reverb themselves.
  const toneTabs: ReadonlyArray<[string, ToneGroup | 'zonesout']> = [
    ['Zones', 'zonesout'],
    ['Wave and pitch', 'wavepitch'],
    ['Filter and amp', 'filteramp'],
    ['LFO and modulation', 'lfomod'],
  ];
  const patchTabs = ['Patch', 'Structure', 'Routing', 'Effects'] as const;
  const tabNames = [...toneTabs.map(([n]) => n), ...patchTabs];
  const tab = $derived(tabIn(arrangement.patchTab, tabNames.length));
  /** The sound sections show the selected Tone, or all four Tones side by side. */
  const toneView = $derived(tabIn(arrangement.toneView, 2));
  const isTone = (i: number): boolean => i < toneTabs.length;

  // The side tabs: playing, sound-design tools, history.
  const sideTabs = ['Play', 'Overview', 'Bank', 'Tools', 'History'];
  const side = $derived(tabIn(arrangement.patchSide, sideTabs.length));

  const sectionNames: readonly ToneSection[] = ['wave', 'pitch', 'tvf', 'tva', 'lfo1', 'lfo2', 'controllers', 'output'];
  const sections = $derived(sectionNames.map((s) => ({ title: toneSectionTitle(s), ids: toneParams(store.catalog.tone, s) })));
  const block = $derived(TONE_BLOCKS[selected] ?? 'tone1');
  const color = $derived(colors[selected] ?? 'var(--accent)');
</script>

<div class="work">
  <aside class="left">
    <DockPanel title="Tones" meta={store.name.trimEnd()} focused={false}>
      <div class="cards">
        {#each TONE_BLOCKS as b, i (b)}
          <ToneCard block={b} color={colors[i] ?? 'var(--accent)'} selected={selected === i} onselect={() => (selected = i)} />
        {/each}
      </div>
    </DockPanel>
    <TabPanel tabs={sideTabs} selected={side} name="Side tools" onchange={(i) => (arrangement.patchSide = i)} meta={side === 4 ? `${store.position} / ${store.items.length}` : ''}>
      {#snippet children(i)}
        {#if i === 0}
          {#if synth}<AuditionPanel {synth} />{:else}<p class="dim">Playing the keyboard needs the desktop app with a connected XP-60.</p>{/if}
        {:else if i === 1}
          <OverviewPanel {colors} {selected} onselect={(n) => (selected = n)} />
        {:else if i === 2}
          <BankListPanel />
        {:else if i === 3}
          <ToolsPanel {sections} />
        {:else}
          <HistoryPanel />
        {/if}
      {/snippet}
    </TabPanel>
  </aside>

  <main class="center">
    <TabPanel
      tabs={tabNames}
      selected={tab}
      name="Sound sections"
      color={(i) => (isTone(i) ? color : 'var(--accent)')}
      onchange={(i) => (arrangement.patchTab = i)}
    >
      {#snippet children(i)}
        {#if i === 0}
          <ZonesPanel {colors} {selected} onselect={(n) => (selected = n)} />
        {:else if isTone(i)}
          {@const part = (toneTabs[i]?.[1] ?? 'wavepitch') as ToneGroup}
          <div class="toneview">
            <div class="view">
              <span class="caps">Show</span>
              <Segmented options={['Selected Tone', 'All 4 Tones']} selected={toneView} name="Tones shown in this section" context="Patch" onchange={(v) => (arrangement.toneView = v)} />
            </div>
            {#if toneView === 1}
              <AllTonesPanel {part} {colors} {selected} onselect={(n) => (selected = n)} />
            {:else}
              <TonePanel {block} {color} {part} />
            {/if}
          </div>
        {:else if i === toneTabs.length}
          <CommonPanel columns={[['patch', 'play'], ['portamento', 'controllers']]} showName />
        {:else if i === toneTabs.length + 1}
          <CommonPanel sections={['structure']} />
        {:else if i === toneTabs.length + 2}
          <RoutingPanel onjump={() => (arrangement.patchTab = tabNames.indexOf('Effects'))} />
        {:else}
          <div class="stackcol">
            <CommonPanel sections={['efx']} />
            <CommonPanel sections={['chorus', 'reverb']} />
          </div>
        {/if}
      {/snippet}
    </TabPanel>
  </main>
</div>

<style>
  .stackcol {
    display: flex;
    flex-direction: column;
    gap: 10px;
  }
  .toneview {
    height: 100%;
    display: flex;
    flex-direction: column;
    min-height: 0;
  }
  .view {
    display: flex;
    align-items: center;
    gap: 8px;
    margin-bottom: 8px;
    flex: none;
  }
  .work {
    display: grid;
    grid-template-columns: 376px minmax(0, 1fr);
    gap: 10px;
    padding: 10px;
    min-height: 0;
  }
  .left,
  .center {
    display: flex;
    flex-direction: column;
    gap: 10px;
    min-width: 0;
    min-height: 0;
  }
  .center > :global(*) {
    flex: 1;
  }
  .left > :global(.tp) {
    flex: 1;
  }
  .cards {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 8px;
  }
  .dim {
    color: var(--text-dim);
    margin: 0;
  }
</style>
