<script lang="ts">
  import { valueGestures } from './actions';
  import { controlMenuItems, type MidiLink } from './controlMenu';
  import { arcPath, normToAngle, polar, SWEEP_END, SWEEP_START, valueArc } from './geometry';
  import { toNorm } from './gesture';
  import { clearHint, showHint } from './hint.svelte';
  import { openMenu } from './menu.svelte';

  interface Props {
    value: number;
    min?: number;
    max?: number;
    step?: number;
    defaultValue?: number;
    /** Normalised origin for bipolar knobs (0.5 for -N..+N); omit for unipolar. */
    origin?: number | null;
    label?: string;
    /** Longer name for the hint bar and screen readers. */
    name?: string;
    context?: string;
    size?: 20 | 30 | 38 | 64;
    color?: string;
    disabled?: boolean;
    /** Turns the raw value into its meaning, for example "+1 oct". */
    format?: (value: number) => string;
    onchange?: (value: number) => void;
    onbegin?: () => void;
    onend?: () => void;
    /** Offer "Link to MIDI CC" in the right-click menu (only with an instrument connection to listen on). */
    midi?: MidiLink;
  }

  let {
    value = $bindable(),
    min = 0,
    max = 127,
    step = 1,
    defaultValue,
    origin = null,
    label = '',
    name = label,
    context = '',
    size = 38,
    color = 'var(--accent)',
    disabled = false,
    format,
    onchange,
    onbegin,
    onend,
    midi,
  }: Props = $props();

  const uid = $props.id();
  const range = $derived({ min, max, step });
  const norm = $derived(toNorm(value, range));
  const angle = $derived(normToAngle(norm));
  const arc = $derived(valueArc(norm, origin));
  const c = $derived(size / 2);
  const stroke = $derived(Math.max(2.5, Math.round(size * 0.1)));
  const ringR = $derived(c - stroke / 2 - 1);
  const capR = $derived(ringR - stroke / 2 - 1.5);
  const dot = $derived(polar(c, c, capR * 0.62, angle));
  const meaning = $derived(format ? format(value) : String(value));

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
    onMenu: (x: number, y: number) =>
      openMenu(
        x,
        y,
        controlMenuItems({
          range,
          get: () => value,
          set,
          defaultValue,
          begin: onbegin,
          end: onend,
          midi,
        }),
      ),
  });
</script>

<div
  class="knob"
  class:disabled
  style:--arc={color}
  style:width="{Math.max(size, 60)}px"
  use:valueGestures={opts}
  role="slider"
  tabindex={disabled ? -1 : 0}
  aria-label={name || label}
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
  <svg width={size} height={size} viewBox="0 0 {size} {size}" aria-hidden="true">
    <defs>
      <linearGradient id="cap-{uid}" x1="0" y1="0" x2="0" y2="1">
        <stop offset="0" stop-color="var(--cap-top)" />
        <stop offset="1" stop-color="var(--cap-bottom)" />
      </linearGradient>
    </defs>
    <circle cx={c} cy={c} r={ringR} fill="none" stroke="var(--track)" stroke-width={stroke} />
    <path
      d={arcPath(c, c, ringR, SWEEP_START, SWEEP_END)}
      fill="none"
      stroke="var(--edge)"
      stroke-width={stroke}
      stroke-linecap="butt"
      opacity="0.55"
    />
    <path
      d={arcPath(c, c, ringR, arc.from, arc.to)}
      fill="none"
      stroke="var(--arc)"
      stroke-width={stroke}
      stroke-linecap="butt"
    />
    {#if origin !== null}
      {@const a = polar(c, c, ringR + stroke / 2 + 0.5, normToAngle(origin))}
      {@const b = polar(c, c, ringR + stroke / 2 + 2.5, normToAngle(origin))}
      <line x1={a.x} y1={a.y} x2={b.x} y2={b.y} stroke="var(--text-dim)" stroke-width="1" />
    {/if}
    <circle cx={c} cy={c} r={capR} fill="url(#cap-{uid})" stroke="var(--edge)" stroke-width="1" />
    <circle cx={dot.x} cy={dot.y} r={Math.max(1.2, size * 0.04)} fill="var(--text)" />
  </svg>
  {#if label}<span class="caps">{label}</span>{/if}
</div>

<style>
  .knob {
    display: inline-flex;
    flex-direction: column;
    align-items: center;
    gap: 2px;
    user-select: none;
    touch-action: none;
    cursor: ns-resize;
    border-radius: var(--radius);
    outline: none;
  }
  /* Keyboard focus is shown by the knob itself lighting up, never by a box, border or tint around it. */
  .knob:focus-visible svg {
    filter: brightness(1.4) saturate(1.15);
  }
  .knob:focus-visible .caps {
    color: var(--text);
  }
  .knob:hover svg path:nth-of-type(2) {
    filter: brightness(1.15);
  }
  .knob.disabled {
    opacity: 0.4;
    cursor: default;
  }
  .caps {
    max-width: 100%;
    text-align: center;
    line-height: 1.1;
    overflow-wrap: break-word;
  }
</style>
