<script lang="ts">
  // Test support: a Tone card with the store it expects from its parent.
  import type { PatchStore } from '../model/store.svelte';
  import { provideStore } from './context';
  import ToneCard from './ToneCard.svelte';

  interface Props {
    store: PatchStore;
    selected?: boolean;
    onselect?: () => void;
  }

  let { store, selected = false, onselect = () => {} }: Props = $props();
  // svelte-ignore state_referenced_locally
  provideStore(store);
</script>

<ToneCard block="tone1" color="#e0883a" {selected} {onselect} />
