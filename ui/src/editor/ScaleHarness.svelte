<script lang="ts">
  // Test support: the Scale tune page with the store it expects from its parent.
  import type { PatchStore } from '../model/store.svelte';
  import { provideStore } from './context';
  import ScaleTunePanel from './ScaleTunePanel.svelte';

  let { store }: { store: PatchStore } = $props();
  // svelte-ignore state_referenced_locally
  provideStore(store);
</script>

<ScaleTunePanel />
