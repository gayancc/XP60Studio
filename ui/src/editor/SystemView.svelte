<script lang="ts">
  import { onMount } from 'svelte';
  import { TabPanel } from '../kit';
  import { reportWarning } from '../model/messages.svelte';
  import { SCALE_TUNE_BLOCKS } from '../model/catalog';
  import { arrangement, tabIn } from '../model/layouts.svelte';
  import { ui } from '../model/ui.svelte';
  import type { SystemSection } from '../model/layout';
  import { blockTitle, systemSectionTitle } from '../model/labels';
  import { useStore } from './context';
  import HistoryPanel from './HistoryPanel.svelte';
  import SystemPage from './SystemPage.svelte';
  import ToolsPanel from './ToolsPanel.svelte';
  import ScaleTunePanel from './ScaleTunePanel.svelte';

  const store = useStore();

  // Shown once when the page opens, in the corner with every other message.
  onMount(() => {
    reportWarning('System settings live in the instrument\'s permanent memory. Edit them here, then write them with the save ceremony (backup, confirm, read back). They are never sent live.');
  });

  const sections: readonly SystemSection[] = ['sound', 'controllers', 'receive', 'keyboard', 'pedals', 'banks', 'preview', 'arpeggio'];

  const tabNames = [...sections.map((s) => systemSectionTitle(s)), 'Scale tune', 'Tools', 'History'];
  const tab = $derived(tabIn(arrangement.systemTab, tabNames.length));
  const TUNE_TAB = sections.length;

  const tuneBlock = $derived(SCALE_TUNE_BLOCKS[ui.tune] ?? 'tune17');
</script>

<div class="work">
  <TabPanel tabs={tabNames} selected={tab} name="System sections" meta={tab === TUNE_TAB ? blockTitle(tuneBlock, 'system') : ''} onchange={(i) => (arrangement.systemTab = i)}>
    {#snippet children(i)}
      {#if i < TUNE_TAB}
        <SystemPage section={sections[i] as SystemSection} />
      {:else if i === TUNE_TAB}
        <ScaleTunePanel />
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
</style>
