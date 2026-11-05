<script lang="ts">
  // Test support: the EFX type list and parameter knobs with the store they expect from their parent.
  import type { PatchStore } from '../model/store.svelte';
  import { provideStore } from './context';
  import EfxParam from './EfxParam.svelte';
  import EfxTypePick from './EfxTypePick.svelte';

  interface Props {
    store: PatchStore;
  }
  let { store }: Props = $props();
  // svelte-ignore state_referenced_locally
  provideStore(store);
</script>

<EfxTypePick block="common" id="efx_type" />
{#each Array.from({ length: 12 }, (_, i) => i + 1) as n (n)}
  <EfxParam block="common" id={`efx_parameter_${n}`} />
{/each}
