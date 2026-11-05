<script lang="ts">
  import { ContextMenu } from '../kit';
  import type { DocKindName, Mode } from '../model/catalog';
  import { arrangement, layouts } from '../model/layouts.svelte';
  import type { PatchStore } from '../model/store.svelte';
  import type { SynthService } from '../model/synth.svelte';
  import type { Services } from './services';
  import Workspace from './Workspace.svelte';

  interface Props {
    /** One store per kind of document; each keeps its own history and live-edit queue. */
    stores: Record<DocKindName, PatchStore>;
    /** Present only in the desktop app, where an instrument can be connected. */
    synth?: SynthService;
    /** Library and save ceremony; present only in the desktop app. */
    services?: Services;
  }

  let { stores, synth, services }: Props = $props();
  const mode = $derived<Mode>(arrangement.mode);
  // What is on screen is written down whenever it changes, so the next run opens the same way.
  $effect(() => {
    void JSON.stringify(arrangement);
    layouts.persistCurrent();
  });
</script>

<ContextMenu />

{#key mode}
  <Workspace store={stores[mode === 'library' || mode === 'bank' ? 'patch' : mode]} {synth} {services} {mode} onmode={(m) => (arrangement.mode = m)} />
{/key}
