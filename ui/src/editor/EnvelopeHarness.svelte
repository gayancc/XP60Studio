<script lang="ts">
  // Test support: one envelope section with the store it expects from its parent.
  import { ENVELOPES } from '../model/layout';
  import type { PatchStore } from '../model/store.svelte';
  import { provideStore } from './context';
  import EnvelopeSection from './EnvelopeSection.svelte';

  let { store, which = 'tvf' }: { store: PatchStore; which?: 'pitch' | 'tvf' | 'tva' } = $props();
  // svelte-ignore state_referenced_locally
  provideStore(store);
  const spec = $derived(ENVELOPES.find((e) => e.key === which)!);
</script>

<EnvelopeSection {spec} block="tone1" color="#e0883a" />
