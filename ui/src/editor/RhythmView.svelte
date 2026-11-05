<script lang="ts">
  import { GroupFrame, PianoRoll, TabPanel, type RollBar } from '../kit';
  import { RHYTHM_NOTE_BLOCKS, type BlockName } from '../model/catalog';
  import { arrangement, tabIn } from '../model/layouts.svelte';
  import { ui } from '../model/ui.svelte';
  import { noteSectionOf, paramsInSection, RHYTHM_ENVELOPES, envelopeParamIds, type EnvelopeSpec, type NoteSection } from '../model/layout';
  import { blockTitle, midiNoteName, noteSectionTitle } from '../model/labels';
  import { useStore } from './context';
  import EnvelopeSection from './EnvelopeSection.svelte';
  import HistoryPanel from './HistoryPanel.svelte';
  import ToolsPanel from './ToolsPanel.svelte';
  import ParamControl from './ParamControl.svelte';

  const store = useStore();

  const FIRST = 35;
  const LAST = FIRST + 63;
  const ROLL_W = 380;
  const ROLL_H = 540;

  const color = 'var(--accent)';
  let selected = $state(ui.key);
  $effect(() => {
    ui.key = selected;
  });
  $effect(() => {
    selected = ui.key;
  });
  const block = $derived<BlockName>(RHYTHM_NOTE_BLOCKS[selected] ?? 'note35');
  const selectedNote = $derived(FIRST + selected);

  const tabNames = ['Wave', 'Pitch', 'Filter', 'Amp', 'Output', 'Set', 'Tools', 'History'];
  const tab = $derived(tabIn(arrangement.rhythmTab, tabNames.length));

  const specOf = (key: EnvelopeSpec['key']): EnvelopeSpec => RHYTHM_ENVELOPES.find((e) => e.key === key) as EnvelopeSpec;

  function plain(section: NoteSection, env?: EnvelopeSpec): string[] {
    const drawn = new Set(env ? envelopeParamIds(env) : []);
    return paramsInSection(store.catalog.rhythmNote, noteSectionOf, section).filter((id) => !drawn.has(id));
  }

  const waveIds = $derived(plain('wave'));
  const pitchIds = $derived(plain('pitch', specOf('pitch')));
  const tvfIds = $derived(plain('tvf', specOf('tvf')));
  const tvaIds = $derived(plain('tva', specOf('tva')));
  const outputIds = $derived(plain('output'));

  let draft = $state<string | null>(null);
  const shownName = $derived(draft ?? store.name.trimEnd());

  function commitName(): void {
    if (draft !== null) store.setName(draft);
    draft = null;
  }

  const pick = (note: number): void => {
    selected = Math.min(63, Math.max(0, note - FIRST));
  };

  /** One bar per key, as long as the key's level: a drum-style piano roll. */
  const bars = $derived<RollBar[]>(
    RHYTHM_NOTE_BLOCKS.map((b, i) => {
      const on = store.get(b, 'tone_switch') === 1;
      return {
        note: FIRST + i,
        value: store.get(b, 'tone_level') / 127,
        color: 'var(--info)',
        dim: !on,
        selected: FIRST + i === selectedNote,
      };
    }),
  );
</script>

<div class="work">
  <section class="rollcol" aria-label="Rhythm keys">
    <PianoRoll
      first={FIRST}
      last={LAST}
      width={ROLL_W}
      height={ROLL_H}
      kbW={66}
      {bars}
      marked={selectedNote}
      context="Rhythm keys"
      onkey={(n, down) => down && pick(n)}
      onbar={pick}
    />
  </section>

  <div class="right">
    <div class="head">
      <label class="name">
        <span class="caps">Rhythm set name</span>
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
          aria-label="Rhythm set name"
        />
      </label>
      <div class="sel">
        <span class="caps">Selected key</span>
        <b>{midiNoteName(selectedNote)}</b><span class="n">{selectedNote}</span>
      </div>
    </div>
  <TabPanel tabs={tabNames} selected={tab} name="Key sections" {color} meta={blockTitle(block, 'rhythm')} onchange={(i) => (arrangement.rhythmTab = i)}>
    {#snippet children(i)}
      {#if i === 0}
        <GroupFrame title={noteSectionTitle('wave')} {color}>
          {#each waveIds as id (id)}<ParamControl {block} {id} {color} />{/each}
        </GroupFrame>
      {:else if i === 1}
        <GroupFrame title={noteSectionTitle('pitch')} {color}>
          {#each pitchIds as id (id)}<ParamControl {block} {id} {color} />{/each}
          <EnvelopeSection spec={specOf('pitch')} {block} {color} />
        </GroupFrame>
      {:else if i === 2}
        <GroupFrame title={noteSectionTitle('tvf')} {color}>
          {#each tvfIds as id (id)}<ParamControl {block} {id} {color} />{/each}
          <EnvelopeSection spec={specOf('tvf')} {block} {color} />
        </GroupFrame>
      {:else if i === 3}
        <GroupFrame title={noteSectionTitle('tva')} {color}>
          {#each tvaIds as id (id)}<ParamControl {block} {id} {color} />{/each}
          <EnvelopeSection spec={specOf('tva')} {block} {color} />
        </GroupFrame>
      {:else if i === 4}
        <GroupFrame title={noteSectionTitle('output')} {color}>
          {#each outputIds as id (id)}<ParamControl {block} {id} {color} />{/each}
        </GroupFrame>
      {:else if i === 5}
        <p class="dim">The rhythm set's own name is above the keys; its common settings are held back until they are confirmed on the instrument.</p>
      {:else if i === 6}
        <ToolsPanel sections={(['wave', 'pitch', 'tvf', 'tva', 'output'] as NoteSection[]).map((s) => ({ title: noteSectionTitle(s), ids: paramsInSection(store.catalog.rhythmNote, noteSectionOf, s) }))} />
      {:else}
        <HistoryPanel />
      {/if}
    {/snippet}
  </TabPanel>
  </div>
</div>

<style>
  .work {
    display: grid;
    grid-template-columns: auto minmax(0, 1fr);
    gap: 10px;
    padding: 10px;
    min-height: 0;
  }
  .right {
    display: flex;
    flex-direction: column;
    gap: 8px;
    min-width: 0;
    min-height: 0;
  }
  .right > :global(.tp) {
    flex: 1;
  }
  .head {
    display: flex;
    gap: 24px;
    align-items: flex-end;
    padding: 6px 8px;
    background: var(--body);
    border: 1px solid var(--edge);
    border-radius: var(--radius-lg);
  }
  .name {
    display: flex;
    flex-direction: column;
    gap: 2px;
    width: 160px;
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
  .sel {
    display: flex;
    gap: 8px;
    align-items: baseline;
  }
  .sel b {
    font: 600 15px var(--font-caps);
    color: var(--accent);
  }
  .n {
    font-family: var(--font-mono);
    color: var(--text-dim);
  }
  .dim {
    color: var(--text-dim);
    margin: 0;
  }
</style>
