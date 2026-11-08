<script lang="ts">
  import { messages, type MessageKind } from '../model/messages.svelte';
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
  // Every kind of app message shares this bar; a control under the pointer still takes the main line while hovered.
  const current = $derived(messages.current);
  const waiting = $derived(Math.max(0, messages.queueSize - (current ? 1 : 0)));
  const LABEL: Record<MessageKind, string> = {
    error: 'Error',
    warning: 'Warning',
    success: 'Done',
    info: 'Note',
  };

  // Pause the status-line clock while a control hint covers it, so a message cannot expire unseen during a drag.
  $effect(() => {
    if (!hint.name) {
      messages.resume();
      return;
    }
    messages.pause();
    return () => messages.resume();
  });
</script>

<footer class="bar" aria-label="Hint bar">
  <div class="text">
    <div class="ctx">{hint.context || ' '}</div>
    <div class="line">
      {#if hint.name}
        <b>{hint.name}:</b> <span class="val">{hint.value}</span>
        {#if hint.raw}<span class="raw">({hint.raw})</span>{/if}
        {#if current}
          <span class="parked" title={current.text}>· {LABEL[current.kind]} waiting{#if waiting > 0} (+{waiting}){/if}</span>
        {/if}
      {:else if current}
        <span
          class="status-msg {current.kind}"
          role={current.kind === 'error' ? 'alert' : 'status'}
          aria-live={current.kind === 'error' ? 'assertive' : 'polite'}
          data-kind={current.kind}
          data-queue={messages.queueSize}
        >
          <span class="tag">{LABEL[current.kind]}</span>
          <span class="msg-text" title={current.text}>{current.text}</span>
          {#if waiting > 0}
            <span class="more" title="{waiting} more in queue">+{waiting}</span>
          {/if}
          <button
            type="button"
            class="x"
            aria-label="Dismiss message"
            title="Dismiss"
            onclick={() => { if (current) messages.dismiss(current.id); }}
          >×</button>
        </span>
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
  .parked {
    flex: none;
    margin-left: 8px;
    color: var(--text-faint);
    font-size: 10.5px;
  }
  .status-msg {
    display: inline-flex;
    align-items: center;
    gap: 6px;
    min-width: 0;
    max-width: 100%;
    overflow: hidden;
    color: var(--text);
  }
  .tag {
    flex: none;
    padding: 1px 6px;
    border-radius: 3px;
    font: 600 9.5px var(--font-caps);
    letter-spacing: 0.08em;
    text-transform: uppercase;
    background: var(--well-deep);
    color: var(--text);
  }
  .error .tag {
    background: var(--bad);
    color: #1c0808;
  }
  .warning .tag {
    background: var(--warn);
    color: #1d1405;
  }
  .success .tag {
    background: var(--ok);
    color: #07180c;
  }
  .info .tag {
    background: var(--well);
    color: var(--text);
  }
  .msg-text {
    min-width: 0;
    overflow: hidden;
    text-overflow: ellipsis;
  }
  .more {
    flex: none;
    color: var(--text-faint);
    font-size: 10.5px;
  }
  .x {
    flex: none;
    width: 18px;
    height: 18px;
    padding: 0;
    border: 0;
    border-radius: var(--radius);
    background: transparent;
    color: var(--text-dim);
    font-size: 14px;
    line-height: 1;
    cursor: pointer;
  }
  .x:hover {
    background: var(--well);
    color: var(--text);
  }
  .x:focus-visible {
    outline: none;
    background: var(--well);
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
