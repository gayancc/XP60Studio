<script lang="ts">
  // Test support: the Bank workspace with a store and the services it expects from its parent.
  import type { PatchStore } from '../model/store.svelte';
  import { provideStore } from './context';
  import { provideServices, type Services } from './services';
  import BankView from './BankView.svelte';

  interface Props {
    store: PatchStore;
    services: Services;
  }
  let { store, services }: Props = $props();
  // svelte-ignore state_referenced_locally
  provideStore(store);
  // svelte-ignore state_referenced_locally
  provideServices(services);
</script>

<BankView onopened={() => {}} />
