<script lang="ts">
  import { FADE_MODES, LFO_FORM_FULL, lfoTrace, type LfoSettings } from '../kit/lfoShape';
  import { clearHint, showHint } from '../kit/hint.svelte';
  import type { BlockName } from '../model/catalog';
  import { useStore } from './context';

  // The LFO drawn as it will move: the Form from the manual's chart, shifted by Offset, restarted by Key Trigger, and shaped over time by Delay, Fade and Fade
  // Mode exactly as the manual's four graphs show. The manual gives no units, so the time axis is schematic; every setting moves the drawing the right way.
  interface Props {
    block: BlockName;
    lfo: 1 | 2;
    color?: string;
  }

  let { block, lfo, color = 'var(--accent)' }: Props = $props();
  const store = useStore();

  const W = 300;
  const H = 84;
  const PAD = 6;

  const settings = $derived<LfoSettings>({
    form: store.get(block, `lfo${lfo}_waveform`),
    rate: store.get(block, `lfo${lfo}_rate`),
    offset: store.get(block, `lfo${lfo}_offset`),
    delay: store.get(block, `lfo${lfo}_delay_time`),
    fadeMode: store.get(block, `lfo${lfo}_fade_mode`),
    fade: store.get(block, `lfo${lfo}_fade_time`),
    keySync: store.get(block, `lfo${lfo}_key_trigger`) === 1,
  });
  const trace = $derived(lfoTrace(settings));

  const px = (x: number): number => PAD + x * (W - 2 * PAD);
  const py = (y: number): number => PAD + ((1 - y) / 2) * (H - 2 * PAD);
  const path = (pts: readonly { x: number; y: number }[]): string => pts.map((p, i) => `${i === 0 ? 'M' : 'L'}${px(p.x).toFixed(1)} ${py(p.y).toFixed(1)}`).join('');

  const mode = $derived(FADE_MODES[settings.fadeMode] ?? '');
  const hint = (): void =>
    showHint({
      context: `LFO ${lfo}`,
      name: 'LFO preview',
      value: `${LFO_FORM_FULL[settings.form] ?? ''}, fade mode ${mode}`,
      shortcut: 'Follows the settings; time is schematic, not to scale',
    });
</script>

<svg
  class="lfo"
  style:--c={color}
  viewBox="0 0 {W} {H}"
  role="img"
  aria-label="LFO {lfo}: {LFO_FORM_FULL[settings.form] ?? ''}, fade mode {mode}"
  onpointerenter={hint}
  onpointerleave={clearHint}
>
  <rect class="well" x="0.5" y="0.5" width={W - 1} height={H - 1} rx="2" />
  <line class="mid" x1={PAD} x2={W - PAD} y1={py(0)} y2={py(0)} />
  <line class="centre" x1={PAD} x2={W - PAD} y1={py(trace.centre)} y2={py(trace.centre)} />
  <path class="depth" d={path(trace.upper)} />
  <path class="depth" d={path(trace.lower)} />
  <path class="line" d={path(trace.line)} />
  <line class="key" x1={px(trace.keyOn)} x2={px(trace.keyOn)} y1={PAD} y2={H - PAD} />
  <text class="tag" x={px(trace.keyOn) + 3} y={H - 3}>key on</text>
  {#if trace.keyOff !== null}
    <line class="key" x1={px(trace.keyOff)} x2={px(trace.keyOff)} y1={PAD} y2={H - PAD} />
    <text class="tag" x={px(trace.keyOff) + 3} y={H - 3}>key off</text>
  {/if}
  <text class="tag mode" x={W - PAD - 2} y={11} text-anchor="end">{mode}</text>
</svg>

<style>
  .lfo {
    display: block;
    width: 100%;
    max-width: 420px;
    height: auto;
    margin-top: 6px;
  }
  .well {
    fill: var(--well);
    stroke: var(--edge);
  }
  .mid {
    stroke: var(--well-deep);
  }
  .centre {
    stroke: var(--edge-light);
    stroke-dasharray: 2 3;
  }
  .depth {
    fill: none;
    stroke: var(--c);
    opacity: 0.35;
    stroke-dasharray: 3 3;
  }
  .line {
    fill: none;
    stroke: var(--c);
    stroke-width: 1.6;
    stroke-linejoin: round;
  }
  .key {
    stroke: var(--text-faint);
    stroke-dasharray: 2 2;
  }
  .tag {
    fill: var(--text-dim);
    font: 600 8px var(--font-caps);
    letter-spacing: 0.06em;
    text-transform: uppercase;
  }
</style>
