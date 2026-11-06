<script lang="ts">
  import { formPath } from './lfoShape';

  // A small reference drawing of one LFO Form, as in the manual's Form chart.
  interface Props {
    form: number;
    width?: number;
    height?: number;
  }

  let { form, width = 30, height = 14 }: Props = $props();
  const d = $derived(formPath(form, width, height));
</script>

<svg {width} {height} viewBox="0 0 {width} {height}" aria-hidden="true" class="glyph"><path {d} /></svg>

<style>
  .glyph {
    flex: none;
    fill: none;
    stroke: currentColor;
    stroke-width: 1.3;
    stroke-linejoin: round;
    stroke-linecap: round;
  }
</style>
