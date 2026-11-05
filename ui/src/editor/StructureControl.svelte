<script lang="ts">
  import { tick } from 'svelte';
  import { clearHint, showHint } from '../kit/hint.svelte';
  import { reportInfo } from '../model/messages.svelte';
  import StructureDiagram from '../kit/StructureDiagram.svelte';
  import { describeStructure, STRUCTURE_TYPES, structureLegend, structureRole } from '../model/structure';
  import { useStore } from './context';
  import ParamControl from './ParamControl.svelte';

  // How a pair of Tones is wired (Structure type, ten of them) and the booster level. The wiring is shown as a diagram you click to choose from, instead of a number.
  interface Props {
    /** "Tones 1 and 2" or "Tones 3 and 4". */
    title: string;
    typeId: string;
    boosterId: string;
    /** The two Tones this structure joins. */
    tones: readonly [string, string];
  }
  let { title, typeId, boosterId, tones }: Props = $props();
  const store = useStore();

  const laneNames = $derived<readonly [string, string]>([tones[0].replace('tone', 'TONE '), tones[1].replace('tone', 'TONE ')]);
  const type = $derived(store.get('common', typeId) + 1);
  /** Owner's Manual p.49: the Booster gain only matters for types 3 and 4. */
  const boosterUsed = $derived(type === 3 || type === 4);
  /** Owner's Manual p.49: "In a selection of TYPE2-10, turning off one Tone will make the other Tone have a TYPE1 structure." */
  const oneToneOff = $derived(type > 1 && tones.some((t) => store.get(t, 'tone_switch') !== 1));

  // When a Tone of a pair is off, this pair plays as type 1: say so once when that becomes true.
  $effect(() => {
    if (oneToneOff) reportInfo(`${title}: one of these Tones is off, so the other plays as type 1.`);
  });

  let open = $state(false);
  let trigger: HTMLButtonElement | undefined = $state();
  let popup: HTMLDivElement | undefined = $state();
  let pos = $state({ left: 0, top: 0 });
  let hi = $state(1);

  async function show(): Promise<void> {
    const r = trigger?.getBoundingClientRect();
    const W = 770;
    const H = 540;
    const left = Math.max(8, Math.min(r?.left ?? 8, window.innerWidth - W - 8));
    const below = (r?.bottom ?? 0) + 4;
    pos = { left, top: below + H > window.innerHeight - 8 ? Math.max(8, (r?.top ?? 0) - H - 4) : below };
    hi = type;
    open = true;
    await tick();
    (popup?.querySelector('.cell.on') as HTMLElement | null)?.focus();
  }

  function close(refocus = true): void {
    open = false;
    if (refocus) trigger?.focus();
  }

  function choose(t: number): void {
    store.begin(`${title} · Structure`);
    store.set('common', typeId, t - 1);
    store.end();
    close();
  }

  function keydown(e: KeyboardEvent): void {
    const moves: Record<string, number> = { ArrowRight: 1, ArrowLeft: -1, ArrowDown: 3, ArrowUp: -3 };
    if (e.key === 'Escape') {
      e.preventDefault();
      e.stopPropagation();
      close();
    } else if (e.key in moves) {
      e.preventDefault();
      hi = Math.min(10, Math.max(1, hi + (moves[e.key] ?? 0)));
      void tick().then(() => (popup?.querySelector(`[data-type="${hi}"]`) as HTMLElement | null)?.focus());
    } else if (e.key === 'Enter' || e.key === ' ') {
      if ((e.target as HTMLElement).dataset.type === undefined) return;
      e.preventDefault();
      choose(hi);
    }
  }

  function outside(e: PointerEvent): void {
    if (!open) return;
    const t = e.target as Node;
    if (popup?.contains(t) || trigger?.contains(t)) return;
    close(false);
  }

  const announce = (): void => showHint({ context: title, name: `Structure type ${type}`, value: describeStructure(type), shortcut: 'Click to choose how the two Tones are connected' });
</script>

<svelte:window onpointerdown={outside} />

<div class="sc">
  <span class="caps">{title}</span>
  <div class="row">
    <button
      type="button"
      class="trig"
      class:open
      bind:this={trigger}
      aria-haspopup="dialog"
      aria-expanded={open}
      aria-label="{title}: structure type {type}. Choose another."
      onclick={() => (open ? close() : void show())}
      onpointerenter={announce}
      onpointerleave={clearHint}
      onfocus={announce}
      onblur={clearHint}
    >
      <StructureDiagram {type} width={260} captions={laneNames} />
      <span class="num">TYPE {type}</span>
    </button>
    <span class="boost" class:unused={!boosterUsed} title={boosterUsed ? '' : 'The booster gain only applies to structure types 3 and 4'}>
      <ParamControl block="common" id={boosterId} />
    </span>
  </div>
  <p class="role">{structureRole(type)}</p>
  <p class="legend">{structureLegend(type)}</p>
</div>

{#if open}
  <div class="pop" bind:this={popup} style:left="{pos.left}px" style:top="{pos.top}px" role="dialog" aria-label="Choose the structure for {title}" tabindex="-1" onkeydown={keydown}>
    <header><span class="who">{title}</span><span class="now">Type {type}</span></header>
    <div class="grid">
      {#each STRUCTURE_TYPES as t (t)}
        <button type="button" class="cell" class:on={t === type} data-type={t} aria-pressed={t === type} aria-label="Type {t}. {describeStructure(t)}" onclick={() => choose(t)} onpointerenter={() => (hi = t)}>
          <span class="n">TYPE {t}</span>
          <StructureDiagram type={t} width={236} captions={['TONE 1 (3)', 'TONE 2 (4)']} />
        </button>
      {/each}
    </div>
    <p class="desc"><b>{describeStructure(hi)}</b> {structureRole(hi)}</p>
    <p class="legend pad">{structureLegend(hi)}</p>
  </div>
{/if}

<style>
  .sc {
    display: flex;
    flex-direction: column;
    gap: 3px;
  }
  .row {
    display: flex;
    flex-wrap: wrap;
    align-items: flex-end;
    gap: 6px 10px;
  }
  .trig {
    display: flex;
    flex-direction: column;
    align-items: flex-start;
    gap: 2px;
    padding: 5px 8px 4px;
    background: var(--track);
    border: 1px solid var(--edge);
    border-radius: var(--radius);
    cursor: pointer;
    color: var(--text-dim);
    box-shadow: inset 0 1px 2px rgba(0, 0, 0, 0.35);
  }
  .trig:hover,
  .trig.open {
    background: var(--well-deep);
    color: var(--text);
  }
  .trig:focus-visible {
    outline: none;
    background-image: var(--focus-fill);
  }
  .boost.unused {
    opacity: 0.4;
  }
  .boost.unused :global(*) {
    pointer-events: none;
  }
  .role,
  .legend {
    margin: 0;
    max-width: 330px;
    white-space: normal;
    overflow-wrap: anywhere;
    color: var(--text-dim);
    font-size: 10.5px;
    line-height: 1.35;
  }
  .legend {
    font: 600 9px var(--font-caps);
    letter-spacing: 0.06em;
    color: var(--text-faint);
  }
  .legend.pad {
    padding: 0 12px 10px;
  }
  .num {
    font: 600 9.5px var(--font-caps);
    letter-spacing: 0.1em;
  }
  .pop {
    position: fixed;
    z-index: 950;
    width: 770px;
    max-height: calc(100vh - 16px);
    overflow-y: auto;
    display: flex;
    flex-direction: column;
    background: var(--body);
    border: 1px solid var(--edge-light);
    border-radius: var(--radius-lg);
    box-shadow: 0 12px 32px rgba(0, 0, 0, 0.55);
    outline: none;
  }
  header {
    display: flex;
    justify-content: space-between;
    padding: 6px 10px;
    background: var(--caption);
    border-bottom: 1px solid var(--edge);
  }
  .who {
    font: 600 10.5px var(--font-caps);
    letter-spacing: 0.08em;
    text-transform: uppercase;
    color: var(--text-dim);
  }
  .now {
    font: 600 12px var(--font-caps);
    color: var(--accent);
  }
  .grid {
    display: grid;
    grid-template-columns: 1fr 1fr 1fr;
    gap: 6px;
    padding: 8px;
  }
  .cell {
    display: flex;
    flex-direction: column;
    gap: 3px;
    align-items: flex-start;
    padding: 6px 8px;
    background: var(--body-deep);
    border: 0;
    border-radius: var(--radius);
    cursor: pointer;
    color: var(--text-dim);
    text-align: left;
  }
  .cell:hover {
    background: var(--well-deep);
    color: var(--text);
  }
  .cell.on {
    background: color-mix(in srgb, var(--accent) 18%, var(--body-deep));
    color: var(--text);
  }
  .cell:focus-visible {
    outline: none;
    background-image: var(--focus-fill);
  }
  .n {
    font: 600 9.5px var(--font-caps);
    letter-spacing: 0.1em;
  }
  .desc {
    margin: 0;
    padding: 6px 12px 10px;
    min-height: 34px;
    color: var(--text-dim);
    font-size: 11px;
    line-height: 1.35;
  }
</style>
