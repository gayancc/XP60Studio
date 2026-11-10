<script lang="ts">
  import Dropdown, { type DropdownGroup } from './Dropdown.svelte';
  import StepFace from './StepFace.svelte';
  import { clearHint, showHint } from './hint.svelte';

  // Any value taken from a fixed list, with step left and step right: one face holding the previous and next segments and the entry name, which opens the list.
  // Optionally each entry carries a number (a patch slot) shown in the accent colour, and a strip along the bottom edge shows where the entry sits in the list.
  interface Props {
    /** The entries, in list order. */
    names: readonly string[];
    /** Optional number shown before each entry (padded to three digits), such as a patch's slot. */
    numbers?: readonly number[];
    groups?: readonly DropdownGroup[];
    selected: number;
    /** Step one entry back (-1) or forward (+1); by default the neighbouring entry is chosen with `onselect`. */
    onstep?: (delta: -1 | 1) => void;
    canPrev?: boolean;
    canNext?: boolean;
    onselect: (index: number) => void;
    /** Names the control on the hint bar and for assistive technology. */
    name?: string;
    context?: string;
    /** What the segments are called ("Previous patch"); defaults to "Previous" and "Next" plus the name. */
    prevLabel?: string;
    nextLabel?: string;
    /** Width in pixels of the whole control; omit to fill the parent. */
    width?: number;
    /** Draw the position strip along the bottom edge. */
    strip?: boolean;
    disabled?: boolean;
  }

  let {
    names,
    numbers,
    groups,
    selected,
    onstep,
    canPrev = selected > 0,
    canNext = selected < names.length - 1,
    onselect,
    name = 'Value',
    context = '',
    prevLabel = `Previous ${name}`,
    nextLabel = `Next ${name}`,
    width,
    strip = true,
    disabled = false,
  }: Props = $props();

  let face: HTMLElement | undefined = $state();

  const place = $derived(names.length > 1 ? selected / (names.length - 1) : 0);
  const pad = (i: number): string => String(numbers?.[i] ?? i).padStart(3, '0');

  function step(delta: -1 | 1): void {
    if (onstep) onstep(delta);
    else onselect(selected + delta);
  }

  const hint = (what: string | null): void => (what === null ? clearHint() : showHint({ context, name, value: what, shortcut: 'Click to step · hold to repeat · wheel to scroll' }));
</script>

{#snippet slotNo(i: number)}<span class="no">{pad(i)}</span>{/snippet}

<div class="ss" role="group" bind:this={face} style:width={width === undefined ? undefined : `${width}px`}>
  <StepFace {canPrev} {canNext} {disabled} wheel strip={strip ? place : undefined} {prevLabel} {nextLabel} onstep={step} onhint={hint}>
    <Dropdown options={names} {groups} {selected} {name} {context} {disabled} bare anchor={face} glyph={numbers ? slotNo : undefined} onchange={onselect} />
  </StepFace>
</div>

<style>
  .ss {
    min-width: 0;
    max-width: 100%;
  }
  .no {
    margin-right: 7px;
    color: var(--step-accent, var(--accent));
    font-weight: 600;
    font-variant-numeric: tabular-nums;
  }
</style>
