<script lang="ts">
  import { hint } from './hint.svelte';
  import Led from './Led.svelte';
  import Meter from './Meter.svelte';
  import StateChip from './StateChip.svelte';
  import type { SyncState } from './syncState';

  interface Props {
    sound?: SyncState;
    midiIn?: boolean;
    midiOut?: boolean;
    /** Messages waiting to be sent, and how many the queue is sized for. */
    queued?: number;
    queueCapacity?: number;
    /** Set when the queue is behind pace or a send timed out. */
    queueState?: 'ok' | 'behind' | 'timeout';
  }

  let { sound = 'local', midiIn = false, midiOut = false, queued = 0, queueCapacity = 64, queueState = 'ok' }: Props =
    $props();

  const fill = $derived(queueCapacity > 0 ? queued / queueCapacity : 0);
</script>

<footer class="bar" aria-label="Hint bar">
  <div class="text" aria-live="off">
    <div class="ctx">{hint.context || ' '}</div>
    <div class="line">
      {#if hint.name}
        <b>{hint.name}:</b> <span class="val">{hint.value}</span>
        {#if hint.raw}<span class="raw">({hint.raw})</span>{/if}
      {:else}
        <span class="idle">Hover a control to see its name and value.</span>
      {/if}
      <span class="sc">{hint.shortcut}</span>
    </div>
  </div>
  <div class="status">
    <span class="io"><Led on={midiIn} color="var(--ok)" label="MIDI in" /><span class="caps">In</span></span>
    <span class="io"><Led on={midiOut} color="var(--accent)" label="MIDI out" /><span class="caps">Out</span></span>
    <span class="queue" title="Send queue">
      <span class="caps">Queue {queued}</span>
      <Meter
        level={queueState === 'timeout' ? 1 : fill}
        length={56}
        label="Send queue"
        warnAt={queueState === 'behind' ? 0 : 2}
        badAt={queueState === 'timeout' ? 0 : 2}
      />
    </span>
    <StateChip state={sound} />
  </div>
</footer>

<style>
  .bar {
    display: flex;
    justify-content: space-between;
    align-items: center;
    gap: 16px;
    padding: 4px 10px;
    background: var(--hint);
    border-top: 1px solid var(--edge);
    min-height: 38px;
  }
  .text {
    min-width: 0;
    flex: 1;
  }
  .ctx {
    font-size: 10px;
    color: var(--text-faint);
  }
  .line {
    display: flex;
    gap: 6px;
    align-items: baseline;
    white-space: nowrap;
    overflow: hidden;
  }
  .val {
    color: var(--accent);
  }
  .raw,
  .idle {
    color: var(--text-dim);
  }
  .sc {
    margin-left: auto;
    color: var(--text-faint);
    font-size: 10.5px;
  }
  .status {
    display: flex;
    align-items: center;
    gap: 12px;
    flex: none;
  }
  .io,
  .queue {
    display: inline-flex;
    align-items: center;
    gap: 4px;
  }
</style>
