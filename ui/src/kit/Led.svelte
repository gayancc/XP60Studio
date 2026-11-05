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
    border: 1px solid var(--edge);
    flex: none;
  }
  .led.on {
    background: var(--c);
    box-shadow: 0 0 5px var(--c);
  }
</style>
