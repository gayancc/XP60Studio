<script lang="ts">
  // Test support: the routing panel with the store it expects from its parent.
  import type { PatchStore } from '../model/store.svelte';
  import { provideStore } from './context';
  import RoutingPanel from './RoutingPanel.svelte';

  interface Props {
    store: PatchStore;
  }
  let { store }: Props = $props();
  // svelte-ignore state_referenced_locally
  provideStore(store);
</script>

<RoutingPanel />
