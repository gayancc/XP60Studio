<script lang="ts">
  // Test support: the four-Tone view with the store it expects from its parent.
  import type { PatchStore } from '../model/store.svelte';
  import type { TonePart } from './TonePanel.svelte';
  import { provideStore } from './context';
  import AllTonesPanel from './AllTonesPanel.svelte';

  interface Props {
    store: PatchStore;
    part?: TonePart | 'all';
    onselect?: (i: number) => void;
  }
  let { store, part = 'pitch', onselect = () => {} }: Props = $props();
  // svelte-ignore state_referenced_locally
  provideStore(store);
</script>

<AllTonesPanel {part} colors={['#e0883a', '#5a9cc6', '#86ad54', '#a97bb8']} selected={0} {onselect} />
