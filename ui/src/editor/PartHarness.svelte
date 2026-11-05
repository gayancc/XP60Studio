<script lang="ts">
  // Test support: a Part's patch picker with the store it expects from its parent.
  import type { PatchStore } from '../model/store.svelte';
  import { provideStore } from './context';
  import PartPatchPick from './PartPatchPick.svelte';

  interface Props {
    store: PatchStore;
  }
  let { store }: Props = $props();
  // svelte-ignore state_referenced_locally
  provideStore(store);
</script>

<PartPatchPick block="part1" partName="Part 1" />
