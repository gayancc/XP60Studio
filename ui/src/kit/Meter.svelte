<script lang="ts">
  interface Props {
    /** 0 to 1. */
    level: number;
    orientation?: 'horizontal' | 'vertical';
    length?: number;
    label?: string;
    /** Fraction above which the bar turns amber, and above which red. */
    warnAt?: number;
    badAt?: number;
  }

  let { level, orientation = 'horizontal', length = 90, label = '', warnAt = 0.7, badAt = 0.9 }: Props = $props();

  const clamped = $derived(Math.min(1, Math.max(0, level)));
  const tone = $derived(clamped >= badAt ? 'var(--bad)' : clamped >= warnAt ? 'var(--warn)' : 'var(--ok)');
  const vertical = $derived(orientation === 'vertical');
</script>

<div
  class="meter"
  class:vertical
  style:--len="{length}px"
  style:--tone={tone}
  role="meter"
  aria-label={label}
  aria-valuemin="0"
  aria-valuemax="100"
  aria-valuenow={Math.round(clamped * 100)}
>
  <div class="bar" style={vertical ? `height:${clamped * 100}%` : `width:${clamped * 100}%`}></div>
</div>

<style>
  .meter {
    width: var(--len);
    height: 6px;
    background: var(--track);
    border: 1px solid var(--edge);
    border-radius: 2px;
    overflow: hidden;
    display: flex;
    align-items: stretch;
  }
  .meter.vertical {
    width: 6px;
    height: var(--len);
    align-items: flex-end;
  }
  .bar {
    background: var(--tone);
    transition: width 60ms linear, height 60ms linear;
  }
</style>
