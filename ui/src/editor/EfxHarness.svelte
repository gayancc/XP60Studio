<script lang="ts">
  // Test support: the Effects plug-in with the store it expects from its parent.
  import type { PatchStore } from '../model/store.svelte';
  import { provideStore } from './context';
  import EffectPlugin from './EffectPlugin.svelte';

  interface Props {
    store: PatchStore;
  }
  let { store }: Props = $props();
  // svelte-ignore state_referenced_locally
  provideStore(store);
  const ids = ['efx_type', ...Array.from({ length: 12 }, (_, i) => `efx_parameter_${i + 1}`)];
</script>

<EffectPlugin {ids} />
