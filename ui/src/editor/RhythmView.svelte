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
  import { resetBlock } from './toolActions';
  import { resetChanges } from '../model/tools';

  const store = useStore();

  const FIRST = 35;
  const LAST = FIRST + 63;
  const ROLL_W = 380;
  /** The roll fills whatever height the work area has, never less than a usable minimum. */
  let rollBox = $state(0);
  const rollH = $derived(Math.max(320, rollBox || 540));

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

  const resetKey = (note: number): void => resetBlock(store, RHYTHM_NOTE_BLOCKS[note - FIRST] as BlockName, midiNoteName(note));
  const resetAllKeys = (): void =>
    store.applyMany(RHYTHM_NOTE_BLOCKS.flatMap((b) => resetChanges(store.catalog, store.kind, b)), 'Reset all keys');

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
  <!-- svelte-ignore a11y_no_noninteractive_element_interactions -->
  <div
    class="rollcol"
    aria-label="Rhythm keys"
    role="group"
    tabindex="-1"
    onkeydown={(e) => {
      const d = e.key === 'ArrowUp' ? 1 : e.key === 'ArrowDown' ? -1 : 0;
      if (d === 0) return;
      e.preventDefault();
      pick(selectedNote + d * (e.shiftKey ? 12 : 1));
    }}
  >
    <div class="rollfit" bind:clientHeight={rollBox}>
    <PianoRoll
      first={FIRST}
      last={LAST}
      width={ROLL_W}
      height={rollH}
      kbW={66}
      {bars}
      marked={selectedNote}
      context="Rhythm keys"
      onkey={(n, down) => down && pick(n)}
      onbar={pick}
      onbegin={(n) => store.begin(`${midiNoteName(n)} · Level`)}
      onend={() => store.end()}
      onlevel={(n, v) => store.set(RHYTHM_NOTE_BLOCKS[n - FIRST] as BlockName, 'tone_level', Math.round(v * 127))}
      onreset={(n) => resetBlock(store, RHYTHM_NOTE_BLOCKS[n - FIRST] as BlockName, `${midiNoteName(n)} level`, ['tone_level'])}
      ontoggle={(n) => {
        const b = RHYTHM_NOTE_BLOCKS[n - FIRST] as BlockName;
        store.begin(`${midiNoteName(n)} · On/off`);
        store.set(b, 'tone_switch', store.get(b, 'tone_switch') === 1 ? 0 : 1);
        store.end();
      }}
    />
    </div>
    <p class="how">Click a key or its row to edit it (Up/Down arrows step through keys). Drag along a row to set its level. Double-click a row to switch the key on or off, Alt+click to reset its level. Faded rows are off.</p>
  </div>

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
      <div class="resets">
        <button type="button" onclick={() => resetKey(selectedNote)} title="Put the selected key back at neutral settings">Reset key</button>
        <button type="button" onclick={resetAllKeys} title="Put every key back at neutral settings (one undo step)">Reset all keys</button>
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
    grid-template-rows: minmax(0, 1fr);
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
  .resets {
    display: flex;
    gap: 6px;
    margin-left: auto;
  }
  .resets button {
    background: var(--well-deep);
    border: 1px solid var(--edge);
    border-radius: var(--radius);
    padding: 3px 10px;
    cursor: pointer;
    color: inherit;
    font: inherit;
  }
  .resets button:hover {
    background: var(--well);
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
  .rollcol {
    display: flex;
    flex-direction: column;
    gap: 6px;
    max-width: 400px;
    min-height: 0;
    outline: none;
  }
  /* The roll is laid over this box, so the box takes the height the right-hand side needs and the roll then fills it. */
  .rollfit {
    position: relative;
    flex: 1;
    min-height: 320px;
    width: 382px;
  }
  .rollfit > :global(.roll) {
    position: absolute;
    inset: 0 auto auto 0;
  }
  .how {
    margin: 0;
    font-size: 11px;
    line-height: 1.35;
    color: var(--text-dim);
  }
  .dim {
    color: var(--text-dim);
    margin: 0;
  }
</style>
