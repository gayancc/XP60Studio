<script lang="ts">
  import type { PatchStore } from '../model/store.svelte';
  import type { SynthService } from '../model/synth.svelte';
  import { provideStore } from './context';
  import SynthBar from './SynthBar.svelte';
  let { store, synth, visible = true }: { store: PatchStore; synth: SynthService; visible?: boolean } = $props();
  // svelte-ignore state_referenced_locally
  provideStore(store);
</script>

{#if visible}<SynthBar {synth} />{/if}
