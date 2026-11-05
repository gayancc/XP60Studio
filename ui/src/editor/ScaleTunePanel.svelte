<script lang="ts">
  import { untrack } from 'svelte';
  import { Knob, Segmented } from '../kit';
  import { SCALE_TUNE_BLOCKS } from '../model/catalog';
  import { centsOf, EQUAL_RAW, formatCents, isEqualScale, NOTE_NAMES, presetOf, rawOf, SCALE_PRESETS, targetName, type ScalePreset } from '../model/scaleTune';
  import { reportInfo } from '../model/messages.svelte';
  import { ui } from '../model/ui.svelte';
  import { useStore } from './context';
  import ParamControl from './ParamControl.svelte';

  // The keyboard scale: twelve notes, each moved from equal temperament by up to -64 to +63 cents, for Patch mode and for each of the 16 Performance parts.
  // A switch on the page turns the scale on; with it off the instrument plays equal temperament and the values below are kept, not used.
  const store = useStore();

  const ids = $derived(store.catalog.scaleTune.params.map((p) => p.id));
  const PATCH = 16;
  /** The tuning target: 0 to 15 are Parts, 16 is Patch mode. */
  const target = $derived(ui.tune);
  const block = $derived(SCALE_TUNE_BLOCKS[target] ?? 'tune17');
  const raws = $derived(ids.map((id) => store.get(block, id)));
  const current = $derived(presetOf(raws));
  const scaleOn = $derived(store.get('common', 'scale_tune_switch') === 1);
  const usingParts = $derived(target !== PATCH);
  /** A dot on a Part's button when its scale is not equal temperament. */
  const partChanged = $derived(Array.from({ length: 16 }, (_, p) => !isEqualScale(ids.map((id) => store.get(SCALE_TUNE_BLOCKS[p] ?? 'tune1', id)))));
  const patchChanged = $derived(!isEqualScale(ids.map((id) => store.get('tune17', id))));

  // Turning the scale off leaves the values in place but the instrument stops using them: say so when it happens.
  let seenOn = untrack(() => scaleOn);
  $effect(() => {
    if (seenOn && !scaleOn) reportInfo('Scale tune is off: the instrument plays equal temperament. Your values are kept, just not used.');
    seenOn = scaleOn;
  });

  let confirm = $state<'toAll' | 'fromPatch' | null>(null);
  $effect(() => {
    void target;
    confirm = null;
  });

  function setTarget(index: number): void {
    ui.tune = index;
  }

  function write(blocks: readonly string[], values: readonly number[], label: string): void {
    store.begin(label);
    for (const b of blocks) values.forEach((v, i) => store.set(b, ids[i] ?? '', v));
    store.end();
  }

  const applyPreset = (p: ScalePreset): void => write([block], p.cents.map(rawOf), `Scale tune · ${targetName(target)} · ${p.name}`);

  const allOthers = $derived(SCALE_TUNE_BLOCKS.filter((_, i) => i !== target && (usingParts ? i < PATCH : true)));
  function copyToAll(): void {
    write(allOthers, raws, `Scale tune · copy ${targetName(target)} to ${usingParts ? 'the other Parts' : 'all Parts'}`);
    confirm = null;
  }
  function copyFromPatch(): void {
    write([block], ids.map((id) => store.get('tune17', id)), `Scale tune · ${targetName(target)} from Patch mode`);
    confirm = null;
  }

  // One octave of keys, C to B, drawn here with seven equal white keys so every knob sits straight over or under its own key: sharps over the black keys, naturals
  // under the white keys. A note being edited, hovered or focused tints its key.
  const WHITE = 112;
  const OCT_W = WHITE * 7;
  const KEY_H = 96;
  const BLACK_W = 66;
  const BLACK_H = 58;
  /** Note index (C = 0) and where its knob sits: a white key's centre, or the line between two white keys for a sharp. */
  const NATURALS = [0, 2, 4, 5, 7, 9, 11] as const;
  const SHARPS = [
    { n: 1, boundary: 1 },
    { n: 3, boundary: 2 },
    { n: 6, boundary: 4 },
    { n: 8, boundary: 5 },
    { n: 10, boundary: 6 },
  ] as const;
  let active = $state<number | null>(null);
</script>

{#snippet noteKnob(n: number, x: number, up: boolean)}
  {@const id = ids[n] ?? ''}
  {@const raw = raws[n] ?? EQUAL_RAW}
  {@const cents = centsOf(raw)}
  <div
    class="note"
    class:up
    role="group"
    aria-label="{NOTE_NAMES[n]}"
    style:left="{x}px"
    data-note={NOTE_NAMES[n]}
    onpointerenter={() => (active = n)}
    onpointerleave={() => (active = null)}
    onfocusin={() => (active = n)}
    onfocusout={() => (active = null)}
  >
    {#if up}<b class="val" class:zero={cents === 0}>{formatCents(cents)}</b>{:else}<span class="nm">{NOTE_NAMES[n]}</span>{/if}
    <Knob
      value={raw}
      min={0}
      max={127}
      origin={0.5}
      defaultValue={EQUAL_RAW}
      size={64}
      name="Scale Tune for {NOTE_NAMES[n]}"
      context="Scale tune · {targetName(target)}"
      format={(v) => `${formatCents(centsOf(v))} cents`}
      onchange={(v) => store.set(block, id, v)}
      onbegin={() => store.begin(`Scale tune · ${targetName(target)} · ${NOTE_NAMES[n]}`)}
      onend={() => store.end()}
    />
    {#if up}<span class="nm">{NOTE_NAMES[n]}</span>{:else}<b class="val" class:zero={cents === 0}>{formatCents(cents)}</b>{/if}
  </div>
{/snippet}

<div class="scale" class:off={!scaleOn}>
  <div class="head">
    <ParamControl block="common" id="scale_tune_switch" label="Scale tune" />
  </div>

  <div class="target">
    <div class="field">
      <span class="caps">Applies to</span>
      <Segmented options={['Patch mode', 'Performance parts']} selected={usingParts ? 1 : 0} name="Which scale" context="Scale tune" onchange={(i) => setTarget(i === 0 ? PATCH : 0)} />
    </div>
    {#if usingParts}
      <div class="field parts">
        <span class="caps">Part <span class="soft">(a dot: not equal temperament)</span></span>
        <div class="grid" role="group" aria-label="Part to tune">
          {#each partChanged as changed, p (p)}
            <button type="button" class:sel={target === p} aria-pressed={target === p} onclick={() => setTarget(p)} aria-label="Part {p + 1}{changed ? ', not equal temperament' : ''}">
              {p + 1}{#if changed}<i aria-hidden="true"></i>{/if}
            </button>
          {/each}
        </div>
      </div>
    {/if}
  </div>

  <div class="presets" role="group" aria-label="Start from a scale">
    <span class="caps">Start from</span>
    {#each SCALE_PRESETS as p (p.id)}
      <button type="button" class="chip" class:sel={current?.id === p.id} aria-pressed={current?.id === p.id} title={p.note} onclick={() => applyPreset(p)}>
        {p.name}
      </button>
    {/each}
    <span class="soft">The manual's example scales. <b>{targetName(target)}</b>: {current ? current.name : 'custom'}.</span>
  </div>

  <div class="board" style:width="{OCT_W}px">
    <div class="lane top">
      {#each SHARPS as k (k.n)}{@render noteKnob(k.n, k.boundary * WHITE, true)}{/each}
    </div>
    <div class="piano" style:height="{KEY_H}px" aria-hidden="true">
      {#each NATURALS as n, i (n)}
        <span class="white" class:on={active === n} style:left="{i * WHITE + 1}px" style:width="{WHITE - 2}px"></span>
      {/each}
      {#each SHARPS as k (k.n)}
        <span class="black" class:on={active === k.n} style:left="{k.boundary * WHITE - BLACK_W / 2}px" style:width="{BLACK_W}px" style:height="{BLACK_H}px"></span>
      {/each}
    </div>
    <div class="lane bottom">
      {#each NATURALS as n, i (n)}{@render noteKnob(n, i * WHITE + WHITE / 2, false)}{/each}
    </div>
  </div>

  <div class="actions">
    {#if confirm === 'toAll'}
      <span>Copy {targetName(target)}'s scale to {usingParts ? 'the other 15 Parts' : 'all 16 Parts'}? It replaces their scales.</span>
      <button type="button" class="danger" onclick={copyToAll}>Copy</button>
      <button type="button" onclick={() => (confirm = null)}>Cancel</button>
    {:else if confirm === 'fromPatch'}
      <span>Replace {targetName(target)}'s scale with the Patch-mode scale?</span>
      <button type="button" class="danger" onclick={copyFromPatch}>Replace</button>
      <button type="button" onclick={() => (confirm = null)}>Cancel</button>
    {:else}
      <button type="button" onclick={() => (confirm = 'toAll')}>{usingParts ? 'Copy to the other Parts…' : 'Copy to all Parts…'}</button>
      {#if usingParts}<button type="button" onclick={() => (confirm = 'fromPatch')}>Copy from Patch mode…</button>{/if}
      <span class="soft">Tuning one octave tunes every octave. 1 cent is 1/100 of a semitone. Double-click a knob to reset it.</span>
    {/if}
  </div>
</div>

<style>
  .scale {
    display: flex;
    flex-direction: column;
    gap: 12px;
    align-items: center;
  }
  .scale > * {
    align-self: stretch;
  }
  .head {
    display: flex;
    align-items: center;
    gap: 14px;
  }
  .target {
    display: flex;
    gap: 22px;
    align-items: flex-start;
    flex-wrap: wrap;
  }
  .field {
    display: flex;
    flex-direction: column;
    gap: 4px;
  }
  .caps {
    font: 600 10px var(--font-caps);
    letter-spacing: 0.08em;
    text-transform: uppercase;
    color: var(--text-dim);
  }
  .soft {
    color: var(--text-faint);
    font-size: 11px;
    text-transform: none;
    letter-spacing: 0;
    font-family: inherit;
    font-weight: 400;
  }
  .grid {
    display: grid;
    grid-template-columns: repeat(16, 34px);
    gap: 3px;
  }
  .grid button,
  .chip,
  .actions button {
    background: var(--well-deep);
    border: 1px solid var(--edge);
    border-radius: var(--radius);
    padding: 3px 10px;
    cursor: pointer;
    color: inherit;
    position: relative;
  }
  .grid button {
    padding: 4px 0;
    font-family: var(--font-mono);
    font-size: 11px;
  }
  .grid button i {
    position: absolute;
    top: 3px;
    right: 4px;
    width: 5px;
    height: 5px;
    border-radius: 50%;
    background: var(--accent);
  }
  .grid button.sel i {
    background: var(--accent-ink);
  }
  .grid button.sel,
  .chip.sel {
    background: var(--accent);
    color: var(--accent-ink);
    border-color: var(--accent-soft);
  }
  .grid button:hover:not(.sel),
  .chip:hover:not(.sel),
  .actions button:hover {
    background: var(--well);
  }
  button:focus-visible {
    outline: none;
    background-image: var(--focus-fill);
  }
  .presets {
    display: flex;
    gap: 8px;
    align-items: center;
    flex-wrap: wrap;
  }
  .chip {
    border-radius: 14px;
  }
  .danger {
    background: var(--bad) !important;
    color: #120404;
    border-color: var(--bad);
    font-weight: 600;
  }
  .board {
    position: relative;
    margin: 4px auto 0;
    display: flex;
    flex-direction: column;
    gap: 6px;
  }
  .scale.off .board {
    opacity: 0.6;
  }
  .lane {
    position: relative;
    height: 104px;
  }
  .note {
    position: absolute;
    top: 0;
    transform: translateX(-50%);
    width: 80px;
    height: 104px;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: space-between;
  }
  .val {
    font: 600 15px var(--font-mono);
    line-height: 16px;
    color: var(--accent);
  }
  .val.zero {
    color: var(--text-faint);
    font-weight: 400;
  }
  .nm {
    font: 600 11px var(--font-caps);
    line-height: 14px;
    letter-spacing: 0.06em;
    color: var(--text-dim);
  }
  .piano {
    position: relative;
  }
  .white,
  .black {
    position: absolute;
    top: 0;
    transition: background 0.08s;
  }
  .white {
    height: 100%;
    background: color-mix(in srgb, var(--text) 84%, var(--body-deep));
    border-radius: 0 0 4px 4px;
  }
  .black {
    background: #1a1e21;
    border-radius: 0 0 3px 3px;
    z-index: 1;
  }
  .white.on {
    background: color-mix(in srgb, var(--accent) 55%, color-mix(in srgb, var(--text) 84%, var(--body-deep)));
  }
  .black.on {
    background: color-mix(in srgb, var(--accent) 55%, #1a1e21);
  }
  .actions {
    display: flex;
    gap: 8px;
    align-items: center;
    flex-wrap: wrap;
  }
</style>
