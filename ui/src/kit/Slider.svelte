<script lang="ts">
  import { valueGestures } from './actions';
  import { controlMenuItems } from './controlMenu';
  import { toNorm } from './gesture';
  import { clearHint, showHint } from './hint.svelte';
  import { openMenu } from './menu.svelte';

  interface Props {
    value: number;
    min?: number;
    max?: number;
    step?: number;
    defaultValue?: number;
    /** Normalised origin; the fill runs from it to the thumb (0.5 for a pan slider). */
    origin?: number;
    orientation?: 'horizontal' | 'vertical';
    length?: number;
    label?: string;
    name?: string;
    context?: string;
    color?: string;
    disabled?: boolean;
    format?: (value: number) => string;
    onchange?: (value: number) => void;
    onbegin?: () => void;
    onend?: () => void;
  }

  let {
    value = $bindable(),
    min = 0,
    max = 127,
    step = 1,
    defaultValue,
    origin = 0,
    orientation = 'horizontal',
    length = 110,
    label = '',
    name = label,
    context = '',
    color = 'var(--accent)',
    disabled = false,
    format,
    onchange,
    onbegin,
    onend,
  }: Props = $props();

  const vertical = $derived(orientation === 'vertical');
  const range = $derived({ min, max, step });
  const norm = $derived(toNorm(value, range));
  const meaning = $derived(format ? format(value) : String(value));
  const lo = $derived(Math.min(norm, origin) * 100);
  const hi = $derived(Math.max(norm, origin) * 100);

  const publish = (): void =>
    showHint({
      context,
      name: name || label,
      value: meaning,
      raw: format && meaning !== String(value) ? String(value) : '',
      shortcut: 'Ctrl+drag fine · dbl-click reset',
    });

  function set(v: number): void {
    if (disabled || v === value) return;
    value = v;
    onchange?.(v);
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
    axis: vertical ? ('y' as const) : ('x' as const),
    pxFull: Math.max(1, length),
    onMenu: (x: number, y: number) =>
      openMenu(
        x,
        y,
        controlMenuItems({ range, get: () => value, set, defaultValue, begin: onbegin, end: onend }),
      ),
  });
</script>

<div
  class="slider"
  class:vertical
  class:disabled
  style:--fill={color}
  style:--len="{length}px"
  use:valueGestures={opts}
  role="slider"
  tabindex={disabled ? -1 : 0}
  aria-label={name || label}
  aria-orientation={orientation}
  aria-valuemin={min}
  aria-valuemax={max}
  aria-valuenow={value}
  aria-valuetext={meaning}
  aria-disabled={disabled}
  onpointerenter={publish}
  onpointerleave={clearHint}
  onfocus={publish}
  onblur={clearHint}
>
  <div class="track">
    <div class="fill" style={vertical ? `bottom:${lo}%;height:${hi - lo}%` : `left:${lo}%;width:${hi - lo}%`}></div>
    <div class="thumb" style={vertical ? `bottom:calc(${norm * 100}% - 6px)` : `left:calc(${norm * 100}% - 6px)`}></div>
  </div>
  {#if label}<span class="caps">{label}</span>{/if}
</div>

<style>
  .slider {
    display: inline-flex;
    flex-direction: column;
    align-items: center;
    gap: 3px;
    user-select: none;
    touch-action: none;
    outline: none;
    border-radius: var(--radius);
    cursor: ew-resize;
  }
  .slider.vertical {
    cursor: ns-resize;
  }
  .slider:focus-visible .thumb,
  .slider:focus-visible .fill {
    filter: brightness(1.4);
  }
  .slider:focus-visible .caps {
    color: var(--text);
  }
  .track {
    position: relative;
    width: var(--len);
    height: 8px;
    background: var(--track);
    border: 1px solid var(--edge);
    border-radius: 2px;
    margin: 0 6px;
  }
  .vertical .track {
    width: 8px;
    height: var(--len);
    margin: 6px 0;
  }
  .fill {
    position: absolute;
    background: var(--fill);
    opacity: 0.9;
    inset: 0 auto 0 0;
  }
  .vertical .fill {
    inset: auto 0 0 0;
  }
  .thumb {
    position: absolute;
    width: 12px;
    height: 12px;
    top: -3px;
    border-radius: 2px;
    background: linear-gradient(var(--cap-top), var(--cap-bottom));
    border: 1px solid var(--edge);
  }
  .vertical .thumb {
    top: auto;
    left: -3px;
  }
  .disabled {
    opacity: 0.4;
    cursor: default;
  }
</style>
