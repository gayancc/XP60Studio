<script lang="ts">
  interface Props {
    on?: boolean;
    color?: string;
    size?: number;
    label?: string;
  }

  let { on = false, color = 'var(--ok)', size = 8, label = '' }: Props = $props();
</script>

<span
  class="led"
  class:on
  style:--c={color}
  style:width="{size}px"
  style:height="{size}px"
  role="img"
  aria-label={label ? `${label}: ${on ? 'on' : 'off'}` : undefined}
></span>

<style>
  .led {
    display: inline-block;
    border-radius: 50%;
    background: var(--track);
    /* Mixer insert lamp, measured 2026-11-08: gray bezel, no coloured bloom. */
    border: 1px solid #646a6c;
    box-shadow: none;
    flex: none;
  }
  .led.on {
    /* Hot core #f7f8c7, lime body, darker green rim, then the same gray bezel. */
    background: radial-gradient(circle at 50% 38%, #f7f8c7 0 18%, var(--c) 50%, color-mix(in srgb, var(--c) 42%, #061000) 100%);
    border-color: #646a6c;
    box-shadow: none;
  }
</style>
