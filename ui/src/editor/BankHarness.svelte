<script lang="ts">
  // Test support: the bank list with the store it expects from its parent.
  import type { PatchStore } from '../model/store.svelte';
  import { provideStore } from './context';
  import BankListPanel from './BankListPanel.svelte';

  let { store }: { store: PatchStore } = $props();
  // svelte-ignore state_referenced_locally
  provideStore(store);
</script>

<BankListPanel />
