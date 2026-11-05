<script lang="ts">
  // Test support: the Library view with the services and the store it expects from its parent.
  import type { PatchStore } from '../model/store.svelte';
  import { provideStore } from './context';
  import LibraryView from './LibraryView.svelte';
  import { provideServices, type Services } from './services';

  interface Props {
    services: Services;
    store: PatchStore;
  }

  let { services, store }: Props = $props();
  // svelte-ignore state_referenced_locally
  provideServices(services);
  // svelte-ignore state_referenced_locally
  provideStore(store);
</script>

<LibraryView onopened={() => {}} />
