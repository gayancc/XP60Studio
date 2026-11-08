<script lang="ts">
  import { valueGestures } from './actions';
  import { controlMenuItems } from './controlMenu';
  import { clearHint, showHint } from './hint.svelte';
  import { clamp } from './gesture';
  import { openMenu } from './menu.svelte';

  // A value stepped with minus and plus, drawn the way the FL Studio family draws its small selectors: a recessed value readout between two flat tiles, no
  // border. Click a tile to step by one, press and hold to keep stepping (faster the longer it is held), drag up or down on the value, turn the wheel over it,
  // press the arrow keys, double-click to put it back, right-click for the value menu.
  interface Props {
    value: number;
    min?: number;
    max?: number;
    defaultValue?: number;
    label?: string;
    name?: string;
    context?: string;
    disabled?: boolean;
    /** Width of the value readout in pixels. */
    width?: number;
    /** High-contrast face for dark instrument strips (light ink on deep wells). */
    contrast?: boolean;
    format?: (value: number) => string;
    onchange?: (value: number) => void;
    onbegin?: () => void;
    onend?: () => void;
  }

  let {
    value = $bindable(),
    min = 0,
    max = 15,
    defaultValue,
    label = '',
    name = label,
    context = '',
    disabled = false,
    width = 44,
    contrast = false,
    format,
    onchange,
    onbegin,
    onend,
  }: Props = $props();

  const range = $derived({ min, max, step: 1 });
  const shown = $derived(format ? format(value) : String(value));

  const publish = (): void =>
    showHint({ context, name: name || label, value: shown, raw: format && shown !== String(value) ? String(value) : '', shortcut: 'Click − or + · hold to repeat · drag or wheel on the value' });

  function set(v: number): void {
    const next = clamp(v, min, max);
    if (disabled || next === value) return;
    value = next;
    onchange?.(next);
    publish();
  }

  /** One change as one undo step (a keyboard activation, which has no press and release to bracket it). */
  function commit(v: number): void {
    if (clamp(v, min, max) === value) return;
    onbegin?.();
    set(v);
    onend?.();
  }

  // Press and hold: the first step is immediate, then after a pause it repeats, speeding up. The whole hold is one undo step.
  let holdTimer: ReturnType<typeof setTimeout> | undefined;
  let holding = false;
  const FIRST_DELAY = 380;
  const REPEAT = [110, 90, 70, 55, 40];

  function press(dir: 1 | -1, e: PointerEvent): void {
    if (disabled || e.button !== 0 || holding) return;
    const target = e.currentTarget as HTMLElement;
    target.setPointerCapture?.(e.pointerId);
    holding = true;
    onbegin?.();
    set(value + dir);
    let n = 0;
    const again = (): void => {
      if (!holding) return;
      set(value + dir);
      holdTimer = setTimeout(again, REPEAT[Math.min(n++ >> 2, REPEAT.length - 1)]);
    };
    holdTimer = setTimeout(again, FIRST_DELAY);
  }

  function release(): void {
    clearTimeout(holdTimer);
    if (!holding) return;
    holding = false;
    onend?.();
  }

  // The control removed while a tile is held: stop and close the undo group.
  $effect(() => () => release());

  const opts = $derived({
    range,
    get: () => value,
    set,
    defaultValue,
    disabled,
    begin: onbegin,
    end: onend,
    pxFull: 120,
    onMenu: (x: number, y: number) => openMenu(x, y, controlMenuItems({ range, get: () => value, set, defaultValue })),
  });
</script>

<div class="stepper" class:disabled class:contrast style:--w="{width}px">
  {#if label}<span class="caps">{label}</span>{/if}
  <div class="row">
    <button
      type="button"
      class="tile"
      aria-label="Decrease {name || label}"
      disabled={disabled || value <= min}
      tabindex="-1"
      onpointerdown={(e) => press(-1, e)}
      onpointerup={release}
      onpointercancel={release}
      onclick={(e) => e.detail === 0 && commit(value - 1)}>−</button>
    <div
      class="val"
      use:valueGestures={opts}
      role="spinbutton"
      tabindex={disabled ? -1 : 0}
      aria-label={name || label}
      aria-valuemin={min}
      aria-valuemax={max}
      aria-valuenow={value}
      aria-valuetext={shown}
      onpointerenter={publish}
      onpointerleave={clearHint}
      onfocus={publish}
      onblur={clearHint}
    >
      {shown}
    </div>
    <button
      type="button"
      class="tile"
      aria-label="Increase {name || label}"
      disabled={disabled || value >= max}
      tabindex="-1"
      onpointerdown={(e) => press(1, e)}
      onpointerup={release}
      onpointercancel={release}
      onclick={(e) => e.detail === 0 && commit(value + 1)}>+</button>
  </div>
</div>

<style>
  .stepper {
    display: inline-flex;
    flex-direction: column;
    align-items: flex-start;
    gap: 2px;
  }
  .caps {
    font: 600 10px var(--font-caps);
    letter-spacing: 0.08em;
    text-transform: uppercase;
    color: var(--text-dim);
  }
  /* No border: the three parts are told apart by their fill, with a hairline of the page showing between them. */
  .row {
    display: inline-flex;
    align-items: stretch;
    gap: 1px;
    border-radius: var(--radius);
    overflow: hidden;
    background: var(--body-deep);
  }
  .tile {
    width: 22px;
    padding: 0;
    border: 0;
    background: var(--caption);
    color: var(--text-dim);
    font: 600 15px/1 var(--font-caps);
    cursor: pointer;
    user-select: none;
    touch-action: none;
  }
  .tile:hover:not(:disabled) {
    color: var(--accent);
    background: var(--well);
  }
  .tile:active:not(:disabled) {
    color: var(--accent-ink);
    background: var(--accent);
  }
  .tile:disabled {
    opacity: 0.35;
    cursor: default;
  }
  .tile:focus-visible {
    outline: none;
  }
  .val {
    min-width: var(--w);
    padding: 3px 6px;
    text-align: center;
    background: var(--track);
    color: var(--text);
    font-family: var(--font-mono);
    font-size: 12px;
    line-height: 16px;
    white-space: nowrap;
    cursor: ns-resize;
    user-select: none;
    touch-action: none;
    outline: none;
  }
  .val:focus-visible {
    color: var(--accent);
  }
  .disabled {
    opacity: 0.4;
  }
  /* Locked light-on-dark so parent theme cannot wash out the readout. */
  .contrast .caps {
    color: #a8b6be;
  }
  .contrast .row {
    background: #0c1114;
  }
  .contrast .tile {
    background: #334047;
    color: #d5dee4;
  }
  .contrast .tile:hover:not(:disabled) {
    color: #ffe0b8;
    background: #3f4d56;
  }
  .contrast .tile:active:not(:disabled) {
    color: #1c1208;
    background: var(--accent);
  }
  .contrast .val {
    background: #12181c;
    color: #eef3f6;
  }
  .contrast .val:focus-visible {
    color: #ffb56a;
  }
</style>
