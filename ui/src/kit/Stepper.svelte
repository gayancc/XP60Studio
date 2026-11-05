<script lang="ts">
  import { valueGestures } from './actions';
  import { controlMenuItems } from './controlMenu';
  import { clearHint, showHint } from './hint.svelte';
  import { clamp } from './gesture';
  import { openMenu } from './menu.svelte';

  interface Props {
    value: number;
    min?: number;
    max?: number;
    defaultValue?: number;
    label?: string;
    name?: string;
    context?: string;
    disabled?: boolean;
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
    format,
    onchange,
    onbegin,
    onend,
  }: Props = $props();

  const range = $derived({ min, max, step: 1 });
  const shown = $derived(format ? format(value) : String(value));

  const publish = (): void =>
    showHint({ context, name: name || label, value: shown, raw: format && shown !== String(value) ? String(value) : '' });

  function set(v: number): void {
    const next = clamp(v, min, max);
    if (disabled || next === value) return;
    value = next;
    onchange?.(next);
    publish();
  }

  function commit(v: number): void {
    if (clamp(v, min, max) === value) return;
    onbegin?.();
    set(v);
    onend?.();
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
    onMenu: (x: number, y: number) =>
      openMenu(x, y, controlMenuItems({ range, get: () => value, set, defaultValue })),
  });
</script>

<div class="stepper" class:disabled>
  {#if label}<span class="caps">{label}</span>{/if}
  <div class="row">
    <button type="button" aria-label="Decrease {name || label}" {disabled} tabindex="-1" onclick={() => commit(value - 1)}>−</button>
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
    <button type="button" aria-label="Increase {name || label}" {disabled} tabindex="-1" onclick={() => commit(value + 1)}>+</button>
  </div>
</div>

<style>
  .stepper {
    display: inline-flex;
    flex-direction: column;
    gap: 2px;
  }
  .row {
    display: inline-flex;
    border: 1px solid var(--edge);
    border-radius: var(--radius);
    background: var(--track);
    overflow: hidden;
  }
  button {
    background: var(--caption);
    border: 0;
    width: 18px;
    cursor: pointer;
    color: var(--text-dim);
  }
  button:hover:not(:disabled) {
    color: var(--text);
    background: var(--well);
  }
  .val {
    min-width: 38px;
    padding: 2px 6px;
    text-align: center;
    font-family: var(--font-mono);
    font-size: 11px;
    cursor: ns-resize;
    user-select: none;
    touch-action: none;
    outline: none;
  }
  .val:focus-visible {
    box-shadow: inset var(--focus);
  }
  .disabled {
    opacity: 0.4;
  }
</style>
