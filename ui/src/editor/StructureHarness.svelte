<script lang="ts">
  // Test support: the structure control with the store it expects from its parent.
  import type { PatchStore } from '../model/store.svelte';
  import { provideStore } from './context';
  import StructureControl from './StructureControl.svelte';

  interface Props {
    store: PatchStore;
  }
  let { store }: Props = $props();
  // svelte-ignore state_referenced_locally
  provideStore(store);
</script>

<StructureControl title="Tones 1 and 2" typeId="structure_type_1_2" boosterId="booster_1_2" tones={['tone1', 'tone2']} />
