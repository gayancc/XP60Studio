<script lang="ts">
  import { clearHint, showHint } from './hint.svelte';
  import { SYNC_STATES, type SyncState } from './syncState';

  interface Props {
    state: SyncState;
  }

  let { state }: Props = $props();
  const info = $derived(SYNC_STATES[state]);
</script>

<span
  class="chip"
  style:--c={info.color}
  role="status"
  aria-label="Sound state: {info.label}. {info.meaning}"
  onpointerenter={() => showHint({ name: 'Sound state', value: info.label, raw: info.meaning })}
  onpointerleave={clearHint}
>
  <i></i>{info.label}
</span>

<style>
  .chip {
    display: inline-flex;
    align-items: center;
    gap: 5px;
    padding: 1px 8px 1px 6px;
    border: 1px solid color-mix(in srgb, var(--c) 55%, var(--edge-light));
    border-radius: 10px;
    background: color-mix(in srgb, var(--c) 14%, var(--chip-bg));
    font-family: var(--font-caps);
    font-size: 10px;
    letter-spacing: 0.06em;
    color: var(--chip-text);
    white-space: nowrap;
  }
  i {
    width: 6px;
    height: 6px;
    border-radius: 50%;
    background: var(--c);
    box-shadow: 0 0 3px color-mix(in srgb, var(--c) 70%, transparent);
  }
</style>
