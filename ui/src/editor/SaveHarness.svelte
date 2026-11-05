<script lang="ts">
  // Test support: the two save ceremonies with the store and services they expect from their parent.
  import type { PatchStore } from '../model/store.svelte';
  import { provideStore } from './context';
  import { provideServices, type Services } from './services';
  import SaveCeremony from './SaveCeremony.svelte';
  import SystemSaveCeremony from './SystemSaveCeremony.svelte';

  interface Props {
    store: PatchStore;
    services: Services;
    which: 'patch' | 'system';
  }
  let { store, services, which }: Props = $props();
  // svelte-ignore state_referenced_locally
  provideStore(store);
  // svelte-ignore state_referenced_locally
  provideServices(services);
</script>

{#if which === 'patch'}<SaveCeremony />{:else}<SystemSaveCeremony />{/if}
