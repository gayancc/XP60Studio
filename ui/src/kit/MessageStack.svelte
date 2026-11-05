<script lang="ts">
  import { messages, type MessageKind } from '../model/messages.svelte';

  // Every error, warning, piece of information and confirmation of the app, stacked in the bottom-left corner above the hint bar. Nothing else in the window
  // shows a message line. An error has role "alert" (read out at once); the rest are polite status messages.
  const LABEL: Record<MessageKind, string> = { error: 'Error', warning: 'Warning', info: 'Info', success: 'Done' };
</script>

<div class="stack" aria-label="Messages">
  {#each messages.list as m (m.id)}
    <div class="msg {m.kind}" role={m.kind === 'error' ? 'alert' : 'status'} data-kind={m.kind}>
      <span class="tag">{LABEL[m.kind]}</span>
      <span class="text">{m.text}</span>
      <button type="button" class="x" aria-label="Dismiss message" title="Dismiss" onclick={() => messages.dismiss(m.id)}>×</button>
    </div>
  {/each}
</div>

<style>
  .stack {
    position: fixed;
    left: 10px;
    bottom: 50px;
    z-index: 980;
    display: flex;
    flex-direction: column;
    gap: 6px;
    max-width: min(460px, calc(100vw - 20px));
    pointer-events: none;
  }
  .msg {
    pointer-events: auto;
    display: flex;
    align-items: flex-start;
    gap: 8px;
    padding: 7px 8px 7px 10px;
    background: var(--caption);
    border-radius: var(--radius-lg);
    box-shadow: 0 6px 18px rgba(0, 0, 0, 0.45);
    line-height: 1.35;
  }
  .tag {
    flex: none;
    margin-top: 1px;
    padding: 1px 6px;
    border-radius: 3px;
    font: 600 9.5px var(--font-caps);
    letter-spacing: 0.08em;
    text-transform: uppercase;
    background: var(--well-deep);
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
    color: var(--text-dim);
  }
  .text {
    min-width: 0;
    overflow-wrap: anywhere;
  }
  .error .text {
    color: var(--text);
  }
  .x {
    flex: none;
    margin-left: auto;
    width: 20px;
    height: 20px;
    padding: 0;
    border: 0;
    border-radius: var(--radius);
    background: transparent;
    color: var(--text-dim);
    font-size: 15px;
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
</style>
