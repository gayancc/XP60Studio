<script lang="ts">
  import { GroupFrame, KeyMapH, TabPanel, type MapLane } from '../kit';
  import { PART_BLOCKS } from '../model/catalog';
  import { arrangement, tabIn } from '../model/layouts.svelte';
  import { ui } from '../model/ui.svelte';
  import { paramsInSection, partSectionOf, performanceCommonSectionOf, type PartSection, type PerformanceCommonSection } from '../model/layout';
  import { partSectionTitle, performanceCommonTitle } from '../model/labels';
  import { useStore } from './context';
  import HistoryPanel from './HistoryPanel.svelte';
  import ToolsPanel from './ToolsPanel.svelte';
  import ParamControl from './ParamControl.svelte';
  import PartPatchPick from './PartPatchPick.svelte';
  import EffectPlugin from './EffectPlugin.svelte';

  const store = useStore();

  /** Part colours: a ramp around the hue wheel so parts are told apart at a glance. */
  const partColor = (i: number): string => `hsl(${(i * 360) / 16 + 20} 38% 58%)`;

  let selected = $state(ui.part);
  $effect(() => {
    ui.part = selected;
  });
  $effect(() => {
    selected = ui.part;
  });

  // Space is planned for the keys: the top panel is a full-width keyboard with the parts' ranges in lanes under it, eight
  // parts to a page; the part's own settings are the tabs below. Nothing else competes for the keyboard's width.
  const detailTabs = ['Source', 'Mix', 'Tune', 'MIDI', 'Common', 'Effects', 'Tools', 'History'];
  const page = $derived(tabIn(arrangement.perfPage, 2));
  const detail = $derived(tabIn(arrangement.perfDetail, detailTabs.length));
  const block = $derived(PART_BLOCKS[selected] ?? 'part1');
  const color = $derived(partColor(selected));

  const lanesOf = (pg: number): MapLane[] =>
    PART_BLOCKS.slice(pg * 8, pg * 8 + 8).map((b, j) => {
      const i = pg * 8 + j;
      return {
        id: i,
        low: store.get(b, 'keyboard_range_lower'),
        high: store.get(b, 'keyboard_range_upper'),
        color: partColor(i),
        name: `Part ${i + 1}`,
        selected: selected === i,
        dim: store.get(b, 'receive_switch') !== 1,
      };
    });

  const commonSections: readonly PerformanceCommonSection[] = ['general', 'efx', 'chorus', 'reverb', 'voices'];
  const partSections: readonly PartSection[] = ['source', 'mix', 'tune', 'midi'];
  const commonIds = (s: PerformanceCommonSection): string[] => paramsInSection(store.catalog.performanceCommon, performanceCommonSectionOf, s);
  const partIds = (s: PartSection): string[] => paramsInSection(store.catalog.performancePart, partSectionOf, s);
  void commonSections;

  let draft = $state<string | null>(null);
  const shownName = $derived(draft ?? store.name.trimEnd());

  function commitName(): void {
    if (draft !== null) store.setName(draft);
    draft = null;
  }

  function setRange(b: string, r: { low: number; high: number }): void {
    if (r.low > store.get(b, 'keyboard_range_upper')) {
      store.set(b, 'keyboard_range_upper', r.high);
      store.set(b, 'keyboard_range_lower', r.low);
    } else {
      store.set(b, 'keyboard_range_lower', r.low);
      store.set(b, 'keyboard_range_upper', r.high);
    }
  }
</script>

<div class="work">
  <TabPanel tabs={['Parts 1–8', 'Parts 9–16']} selected={page} name="Parts" {color} meta={store.name.trimEnd()} onchange={(i) => (arrangement.perfPage = i)}>
    {#snippet children(pg)}
      <KeyMapH
        lanes={lanesOf(pg)}
        context="Key map"
        gutterWidth={318}
        onselect={(id) => (selected = id)}
        onbegin={(id) => store.begin(`Part ${id + 1} · Key range`)}
        onend={() => store.end()}
        onrange={(id, r) => setRange(PART_BLOCKS[id] as string, r)}
      >
        {#snippet gutter(l)}
          {@const b = PART_BLOCKS[l.id] as string}
          <button type="button" class="tag" onclick={() => (selected = l.id)} aria-pressed={selected === l.id} aria-label="Edit Part {l.id + 1}">{l.id + 1}</button>
          <ParamControl block={b} id="receive_switch" label="" color={l.color} />
          <PartPatchPick block={b} partName="Part {l.id + 1}" />
        {/snippet}
      </KeyMapH>
    {/snippet}
  </TabPanel>

  <TabPanel tabs={detailTabs} selected={detail} name="Details" color={(i) => (i < 4 ? color : 'var(--accent)')} meta={detail < 4 ? `Part ${selected + 1}` : ''} onchange={(i) => (arrangement.perfDetail = i)}>
    {#snippet children(i)}
      {#if i < 4}
        {@const s = partSections[i] as PartSection}
        <GroupFrame title={partSectionTitle(s)} {color}>
          {#each partIds(s) as id (id)}<ParamControl {block} {id} {color} />{/each}
        </GroupFrame>
      {:else if i === 4}
        <div class="common">
          <label class="name">
            <span class="caps">Performance name</span>
            <input
              type="text"
              maxlength="12"
              spellcheck="false"
              value={shownName}
              oninput={(e) => (draft = e.currentTarget.value)}
              onblur={commitName}
              onkeydown={(e) => {
                if (e.key === 'Enter') e.currentTarget.blur();
                if (e.key === 'Escape') {
                  draft = null;
                  e.currentTarget.blur();
                }
              }}
              aria-label="Performance name"
            />
          </label>
          <GroupFrame title={performanceCommonTitle('general')}>
            {#each commonIds('general') as id (id)}<ParamControl block="common" {id} />{/each}
          </GroupFrame>
          <GroupFrame title={performanceCommonTitle('voices')}>
            {#each commonIds('voices') as id (id)}<ParamControl block="common" {id} size={20} />{/each}
          </GroupFrame>
        </div>
      {:else if i === 5}
        <div class="stack">
          {#each ['efx', 'chorus', 'reverb'] as const as s (s)}
            <GroupFrame title={performanceCommonTitle(s)}>
              {#if s === 'efx'}
                <EffectPlugin ids={commonIds(s)} />
              {:else}
                {#each commonIds(s) as id (id)}<ParamControl block="common" {id} />{/each}
              {/if}
            </GroupFrame>
          {/each}
        </div>
      {:else if i === 6}
        <div class="wide">
          <ToolsPanel sections={partSections.map((s) => ({ title: partSectionTitle(s), ids: partIds(s) }))} />
        </div>
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
    gap: 10px;
    padding: 10px;
    min-height: 0;
  }
  /* The key map takes the room it needs; the details share what is left. */
  .work > :global(.tp:first-child) {
    flex: none;
  }
  .work > :global(.tp:last-child) {
    flex: 1;
    min-height: 0;
  }
  .tag {
    width: 22px;
    text-align: left;
    background: transparent;
    border: 0;
    cursor: pointer;
    color: var(--text);
    font: 600 12px var(--font-mono);
  }
  .tag:focus-visible {
    outline: none;
    background-image: var(--focus-fill);
  }
  .common {
    display: grid;
    grid-template-columns: 170px repeat(auto-fit, minmax(260px, 1fr));
    gap: 10px;
    align-items: start;
  }
  /* EFX has 22 controls and takes the full width; Chorus and Reverb share the row beneath it. */
  .stack {
    display: grid;
    grid-template-columns: 11fr 9fr;
    gap: 8px;
    align-items: start;
  }
  .stack > :global(fieldset:first-child) {
    grid-column: 1 / -1;
  }
  /* The tools are a tall column in a narrow panel; here the room is wide and short, so they sit in columns. */
  .wide :global(.tools) {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(330px, 1fr));
    gap: 8px 18px;
    align-items: start;
  }
  .name {
    display: flex;
    flex-direction: column;
    gap: 2px;
  }
  input {
    background: var(--track);
    color: var(--text);
    border: 1px solid var(--edge);
    border-radius: var(--radius);
    padding: 4px 6px;
    font-family: var(--font-mono);
    font-size: 13px;
  }
  input:focus {
    outline: none;
    border-color: var(--accent);
  }
</style>
