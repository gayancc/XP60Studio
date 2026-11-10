<script lang="ts">
  import type { Snippet } from 'svelte';

  // The one body for every "step left / step right" control: a single recessed face with the previous and next segments built into it (split by a hairline groove,
  // not separate buttons) around whatever the control shows in the middle. A press steps at once; holding repeats, faster the longer it is held, and the whole hold is
  // one undo group (`onhold`). Optionally the wheel steps, and a thin strip along the bottom edge shows a position.
  interface Props {
    canPrev: boolean;
    canNext: boolean;
    /** One step back (-1) or forward (+1); called for the press and again for every repeat. */
    onstep: (delta: -1 | 1) => void;
    /** Brackets a hold or a keyboard activation, so it is one undo step. */
    onhold?: (phase: 'start' | 'end') => void;
    prevLabel?: string;
    nextLabel?: string;
    /** Tooltips and hint text for the two segments. */
    onhint?: (what: string | null) => void;
    disabled?: boolean;
    /** Turning the wheel anywhere over the face steps (leave off where the middle already handles the wheel). */
    wheel?: boolean;
    /** 0 to 1: how far along a list the current entry is (draws the strip); omit for no strip. */
    strip?: number;
    /** Light-on-dark face for instrument strips. */
    contrast?: boolean;
    /** Width of the whole face in pixels; omit to fill the parent. */
    width?: number;
    children: Snippet;
  }

  let { canPrev, canNext, onstep, onhold, prevLabel = 'Previous', nextLabel = 'Next', onhint, disabled = false, wheel = false, strip, contrast = false, width, children }: Props = $props();

  let root: HTMLDivElement | undefined = $state();
  let holdTimer: ReturnType<typeof setTimeout> | undefined;
  let holding = false;
  const FIRST_DELAY = 380;
  const REPEAT = [110, 90, 70, 55, 40];

  const can = (delta: -1 | 1): boolean => !disabled && (delta < 0 ? canPrev : canNext);

  function press(delta: -1 | 1, e: PointerEvent): void {
    if (e.button !== 0 || holding || !can(delta)) return;
    (e.currentTarget as HTMLElement).setPointerCapture?.(e.pointerId);
    holding = true;
    onhold?.('start');
    onstep(delta);
    let n = 0;
    const again = (): void => {
      if (!holding) return;
      if (!can(delta)) return release();
      onstep(delta);
      holdTimer = setTimeout(again, REPEAT[Math.min(n++ >> 2, REPEAT.length - 1)]);
    };
    holdTimer = setTimeout(again, FIRST_DELAY);
  }

  function release(): void {
    clearTimeout(holdTimer);
    if (!holding) return;
    holding = false;
    onhold?.('end');
  }

  /** A keyboard press (Enter or Space) arrives as a click with no pointer; pointer presses were already handled on press. */
  function key(delta: -1 | 1, e: MouseEvent): void {
    if (e.detail !== 0 || !can(delta)) return;
    onhold?.('start');
    onstep(delta);
    onhold?.('end');
  }

  $effect(() => {
    const el = root;
    if (!el || !wheel) return;
    const turn = (e: WheelEvent): void => {
      if (e.deltaY === 0) return;
      e.preventDefault();
      const delta = e.deltaY > 0 ? 1 : -1;
      if (!can(delta)) return;
      onhold?.('start');
      onstep(delta);
      onhold?.('end');
    };
    el.addEventListener('wheel', turn, { passive: false });
    return () => el.removeEventListener('wheel', turn);
  });

  // The control removed while a segment is held: stop and close the undo group.
  $effect(() => () => release());
</script>

<div class="face" class:contrast class:disabled bind:this={root} style:width={width === undefined ? undefined : `${width}px`}>
  <button
    type="button"
    class="seg prev"
    aria-label={prevLabel}
    disabled={disabled || !canPrev}
    tabindex="-1"
    onpointerdown={(e) => press(-1, e)}
    onpointerup={release}
    onpointercancel={release}
    onclick={(e) => key(-1, e)}
    onpointerenter={() => onhint?.(prevLabel)}
    onpointerleave={() => onhint?.(null)}
  >
    <svg width="6" height="9" viewBox="0 0 6 9" aria-hidden="true"><path d="M5 1L1.5 4.5L5 8" /></svg>
  </button>
  <div class="mid">{@render children()}</div>
  <button
    type="button"
    class="seg next"
    aria-label={nextLabel}
    disabled={disabled || !canNext}
    tabindex="-1"
    onpointerdown={(e) => press(1, e)}
    onpointerup={release}
    onpointercancel={release}
    onclick={(e) => key(1, e)}
    onpointerenter={() => onhint?.(nextLabel)}
    onpointerleave={() => onhint?.(null)}
  >
    <svg width="6" height="9" viewBox="0 0 6 9" aria-hidden="true"><path d="M1 1L4.5 4.5L1 8" /></svg>
  </button>
  {#if strip !== undefined}
    <span class="track" aria-hidden="true"><span class="fill" style:width="{Math.min(1, Math.max(0, strip)) * 100}%"></span></span>
  {/if}
</div>

<style>
  /* One face, drawn like FL's closed combo (top-light dark gradient). The two end segments are parts of it, split by a hairline groove, not buttons of their own. */
  .face {
    position: relative;
    display: flex;
    align-items: stretch;
    height: var(--step-h, 23px);
    min-width: 0;
    box-sizing: border-box;
    border-radius: 3px;
    overflow: hidden;
    background: linear-gradient(180deg, #4e5357 0%, #2f3438 42%, #272c30 100%);
    color: #bbc3bd;
  }
  .face.contrast {
    background: linear-gradient(180deg, #1c2328 0%, #12181c 100%);
    color: #eef3f6;
  }
  .face.disabled {
    opacity: 0.4;
  }
  .mid {
    flex: 1;
    min-width: 0;
    display: flex;
    align-items: stretch;
  }
  .mid > :global(*) {
    flex: 1;
    min-width: 0;
  }
  .seg {
    flex: none;
    width: 22px;
    padding: 0;
    border: 0;
    border-radius: 0;
    background: transparent;
    display: grid;
    place-items: center;
    cursor: pointer;
    user-select: none;
    touch-action: none;
  }
  .prev {
    box-shadow: inset -1px 0 0 #1b1f22, inset -2px 0 0 rgb(255 255 255 / 0.05);
  }
  .next {
    box-shadow: inset 1px 0 0 #1b1f22, inset 2px 0 0 rgb(255 255 255 / 0.05);
  }
  .seg svg {
    fill: none;
    stroke: #8e979c;
    stroke-width: 1.7;
    stroke-linecap: round;
    stroke-linejoin: round;
  }
  .contrast .seg svg {
    stroke: #aab6bd;
  }
  .seg:hover:not(:disabled) {
    background-color: rgb(255 255 255 / 0.07);
  }
  .seg:hover:not(:disabled) svg {
    stroke: #e2e7e3;
  }
  .seg:active:not(:disabled) {
    background-color: rgb(0 0 0 / 0.22);
  }
  .seg:focus-visible {
    outline: none;
    background-image: var(--focus-fill);
  }
  .seg:disabled {
    cursor: default;
  }
  .seg:disabled svg {
    stroke: #596065;
  }
  /* A position along a list: a hair-thin strip on the bottom edge that fills from the left. */
  .track {
    position: absolute;
    left: 0;
    right: 0;
    bottom: 0;
    height: 2px;
    background: rgb(0 0 0 / 0.35);
    pointer-events: none;
  }
  .fill {
    display: block;
    height: 100%;
    min-width: 2px;
    background: var(--step-accent, var(--accent));
  }
</style>
