<script lang="ts">
  import { valueGestures } from './actions';
  import { controlMenuItems } from './controlMenu';
  import { clearHint, showHint } from './hint.svelte';
  import { clamp } from './gesture';
  import { openMenu } from './menu.svelte';
  import StepFace from './StepFace.svelte';

  // A value stepped left and right: one recessed face (StepFace) holding the previous and next segments around the readout.
  // Click a segment to step by one, press and hold to keep stepping (faster the longer it is held), drag up or down on the value, turn the wheel over it,
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
  <StepFace
    {contrast}
    {disabled}
    canPrev={value > min}
    canNext={value < max}
    prevLabel="Decrease {name || label}"
    nextLabel="Increase {name || label}"
    onstep={(d) => set(value + d)}
    onhold={(phase) => (phase === 'start' ? onbegin?.() : onend?.())}
  >
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
  </StepFace>
</div>

<style>
  .stepper {
    display: inline-flex;
    flex-direction: column;
    align-items: flex-start;
    gap: 2px;
    --step-h: var(--work-h);
  }
  .caps {
    font: 600 10px var(--font-caps);
    letter-spacing: 0.08em;
    text-transform: uppercase;
    color: var(--text-dim);
  }
  /* The value is the middle of the shared face (StepFace draws the face, the segments and the hold-to-repeat). */
  .val {
    min-width: var(--w);
    padding: 0 6px;
    display: flex;
    align-items: center;
    justify-content: center;
    color: inherit;
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
  .disabled .caps {
    opacity: 0.4;
  }
  .contrast .caps {
    color: #a8b6be;
  }
</style>
