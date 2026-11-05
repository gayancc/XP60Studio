<script lang="ts">
  import { Knob, Slider, ToggleLed } from '../kit';
  import { blockTitle, midiNoteName } from '../model/labels';
  import { ui } from '../model/ui.svelte';
  import { useStore } from './context';
  import { copySelected, pasteSelected, runInit, runMutate, runRandomize, selectedBlock } from './toolActions';

  interface Props {
    /** Named groups of parameter ids of the selected block that can be copied on their own (a section). */
    sections?: ReadonlyArray<{ title: string; ids: readonly string[] }>;
  }

  let { sections = [] }: Props = $props();
  const store = useStore();

  let sectionIndex = $state(-1);
  let showChanges = $state(false);
  const block = $derived(selectedBlock(store.kind));
  const blockLabel = $derived(blockTitle(block, store.kind));
  const clip = $derived(ui.clip);
  const changes = $derived(showChanges ? store.changesSinceOpened : []);
  const canPaste = $derived(clip !== null && clip.kind === store.kind);

  function copy(): void {
    const s = sectionIndex >= 0 ? sections[sectionIndex] : undefined;
    copySelected(store, s ? `${blockLabel} ${s.title}` : undefined, s?.ids);
  }

  function valueText(id: string, blockName: string, value: number): string {
    try {
      return store.formatValue(blockName, id, value);
    } catch {
      return String(value);
    }
  }
</script>

<div class="tools">
    <div class="row">
      <button type="button" onclick={() => runInit(store)} title="Start from a neutral sound (Ctrl+D)">Init</button>
      <ToggleLed
        checked={ui.tools.allBlocks}
        label="Everything"
        name="Apply tools to everything"
        context="Sound design"
        onchange={(on) => (ui.tools.allBlocks = on)}
      />
      <span class="dim">{ui.tools.allBlocks ? 'whole sound' : blockLabel}</span>
    </div>

    <div class="row">
      <Knob value={Math.round(ui.tools.strength * 100)} min={0} max={100} step={5} size={30} label="Amount" name="Randomize amount" context="Sound design" format={(v) => `${v}%`} onchange={(v) => (ui.tools.strength = v / 100)} />
      <button type="button" onclick={() => runRandomize(store)} title="Replace a share of the parameters at random (Ctrl+R)">Randomize</button>
      <Knob value={Math.round(ui.tools.amount * 100)} min={0} max={100} step={5} size={30} label="Nudge" name="Mutate amount" context="Sound design" format={(v) => `${v}%`} onchange={(v) => (ui.tools.amount = v / 100)} />
      <button type="button" onclick={() => runMutate(store)} title="Nudge every parameter a little (Ctrl+M)">Mutate</button>
    </div>
    <p class="dim">Names and routing are never changed. Each tool is one undo step.</p>

    <div class="row">
      <span class="caps">A/B</span>
      <button type="button" class:on={store.ab.active === 'a'} onclick={() => store.ab.active !== 'a' && store.abSwitch()}>A</button>
      <button type="button" class:on={store.ab.active === 'b'} onclick={() => store.ab.active !== 'b' && store.abSwitch()}>B</button>
      <span class="dim">Ctrl+B switches</span>
    </div>
    <div class="row">
      <Slider
        value={Math.round(ui.tools.morph * 100)}
        min={0}
        max={100}
        length={150}
        label="Morph A to B"
        name="Morph"
        context="Sound design"
        disabled={!store.canMorph}
        format={(v) => `${v}%`}
        onbegin={() => store.begin('Morph')}
        onchange={(v) => {
          ui.tools.morph = v / 100;
          store.morphTo(v / 100);
        }}
        onend={() => {
          store.endMorph();
          ui.tools.morph = 0;
        }}
      />
      {#if !store.canMorph}<span class="dim">Press B, change the sound, then morph.</span>{/if}
    </div>

    <div class="row">
      {#if sections.length > 0}
        <select aria-label="What to copy" bind:value={sectionIndex}>
          <option value={-1}>Whole {blockLabel}</option>
          {#each sections as s, i (s.title)}<option value={i}>{s.title} only</option>{/each}
        </select>
      {/if}
      <button type="button" onclick={copy}>Copy</button>
      <button type="button" onclick={() => pasteSelected(store)} disabled={!canPaste} title={clip ? `Paste ${clip.label} into ${blockLabel}` : 'Nothing copied yet'}>
        Paste{clip ? ` ${clip.label}` : ''}
      </button>
    </div>

    <div class="row">
      <ToggleLed checked={showChanges} label="Changes since opened" name="Show what changed" context="Sound design" onchange={(on) => (showChanges = on)} />
    </div>
    {#if showChanges}
      {#if changes.length === 0}
        <p class="dim">Nothing has changed since this sound was opened.</p>
      {:else}
        <ul class="diff" aria-label="Changed parameters">
          {#each changes as c (c.block + c.id)}
            <li>
              <span class="where">{store.kind === 'rhythm' ? midiNoteName(Number(c.block.slice(4))) : c.block}</span>
              <span class="what">{c.name}</span>
              <span class="old">{valueText(c.id, c.block, c.a)}</span>
              <span aria-hidden="true">→</span>
              <b>{valueText(c.id, c.block, c.b)}</b>
            </li>
          {/each}
        </ul>
      {/if}
    {/if}
</div>

<style>
  .tools {
    display: flex;
    flex-direction: column;
    gap: 8px;
  }
  .row {
    display: flex;
    gap: 8px;
    align-items: flex-end;
    flex-wrap: wrap;
  }
  button,
  select {
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
    background-image: var(--focus-fill);
  }
  button:disabled {
    opacity: 0.4;
    cursor: default;
  }
  button.on {
    background: var(--accent);
    color: var(--accent-ink);
  }
  .dim {
    color: var(--text-dim);
    margin: 0;
    font-size: 11px;
  }
  .diff {
    list-style: none;
    margin: 0;
    padding: 0;
    max-height: 160px;
    overflow: auto;
    font-size: 11px;
  }
  .diff li {
    display: flex;
    gap: 6px;
    padding: 1px 2px;
    align-items: baseline;
  }
  .where {
    color: var(--text-faint);
    min-width: 42px;
  }
  .what {
    flex: 1;
  }
  .old {
    color: var(--text-dim);
  }
</style>
