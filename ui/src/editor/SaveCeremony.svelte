<script lang="ts">
  import { onDestroy } from 'svelte';
  import { Stepper } from '../kit';
  import { useStore } from './context';
  import { useServices } from './services';

  const store = useStore();
  const flow = useServices()?.save;

  let open = $state(false);
  let slot = $state(flow?.slot ?? 0);
  let confirmUndo = $state(false);
  let cancelButton: HTMLButtonElement | undefined = $state();
  const label = (n: number): string => `USER:${String(n + 1).padStart(3, '0')}`;

  // Leaving this workspace while a save is armed must not leave it armed.
  onDestroy(() => {
    void flow?.cancel();
  });

  async function close(): Promise<void> {
    if (flow?.working) return; // an arm or a write in progress is not interrupted
    await flow?.cancel();
    flow?.reset();
    confirmUndo = false;
    open = false;
  }

  async function arm(): Promise<void> {
    confirmUndo = false;
    // The patch is frozen now: what is confirmed below is exactly what gets written.
    await flow?.arm(slot, store.snapshot(), store.name.trimEnd());
  }

  $effect(() => {
    // When the confirmation appears, focus the safe choice, not the destructive one.
    if (flow?.phase === 'armed') cancelButton?.focus();
  });
</script>

{#if flow}
  <div class="save">
    <button
      type="button"
      class="open"
      aria-expanded={open}
      aria-controls="save-panel"
      disabled={flow.working}
      onclick={() => (open ? close() : (open = true))}>Save to synth…</button>
    {#if open}
      <div
        id="save-panel"
        class="panel"
        role="dialog"
        aria-modal="true"
        aria-label="Save to the instrument's User memory"
        tabindex="-1"
        onkeydown={(e) => {
          if (e.key === 'Escape') {
            e.preventDefault();
            void close();
          }
        }}
      >
        <h2>Save into User memory</h2>
        <p class="warn">
          This <b>replaces</b> a patch inside the XP-60 for good. The patch now in that slot is read and kept in a backup
          file first. Live editing never does this.
        </p>

        {#if flow.phase === 'idle' || flow.phase === 'failed' || flow.phase === 'done'}
          <div class="row">
            <Stepper bind:value={slot} min={0} max={127} label="User slot" name="User slot" format={(v) => String(v + 1)} />
            <button type="button" onclick={arm}>1. Read slot and back it up</button>
          </div>
        {/if}

        {#if flow.phase === 'arming'}
          <p role="status">Reading {label(flow.slot)} and saving its backup…</p>
        {/if}

        {#if flow.phase === 'armed' && flow.armed}
          <div class="confirm" role="alert">
            <p>
              <b>{label(flow.armed.slot)}</b> holds <b>“{flow.armed.occupant_name.trimEnd() || '(unnamed)'}”</b>.
            </p>
            <p class="dim">Backed up to {flow.armed.backup_path}</p>
            <p>
              Replace it with <b>“{flow.frozenName || '(unnamed)'}”</b> (the patch as it is now; later edits are not included)?
            </p>
            <div class="row">
              <button type="button" bind:this={cancelButton} onclick={() => flow.cancel()}>Cancel</button>
              <button type="button" class="danger" onclick={() => flow.commit()}>2. Replace {label(flow.armed.slot)}</button>
            </div>
          </div>
        {/if}

        {#if flow.phase === 'writing'}
          <p role="status">Writing and reading back… keep the instrument connected.</p>
        {/if}

        {#if flow.phase === 'done' && flow.report}
          <p class="ok" role="status">
            Saved: {label(flow.report.slot)} was read back and matches ({flow.report.messages} messages, {flow.report.bytes} bytes).
          </p>
        {/if}

        {#if flow.phase === 'failed' && flow.error}
          <p class="err" role="alert">{flow.error}</p>
          <p class="dim">If User Memory Protect is on, turn it off on the instrument and try again.</p>
        {/if}

        {#if flow.last && !flow.working && flow.phase !== 'armed'}
          <div class="undo">
            {#if confirmUndo}
              <p>
                Put <b>“{flow.last.oldName.trimEnd() || '(unnamed)'}”</b> back into <b>{label(flow.last.slot)}</b>? The backup is
                <span class="dim">{flow.last.backupPath}</span>. This replaces what is there now.
              </p>
              <div class="row">
                <button type="button" onclick={() => (confirmUndo = false)}>Keep it</button>
                <button
                  type="button"
                  class="danger"
                  onclick={async () => {
                    confirmUndo = false;
                    await flow.undo();
                  }}>Put the old patch back</button>
              </div>
            {:else}
              <button type="button" onclick={() => (confirmUndo = true)}>Undo the last save…</button>
            {/if}
          </div>
        {/if}

        <div class="row end"><button type="button" onclick={close} disabled={flow.working}>Close</button></div>
      </div>
    {/if}
  </div>
{/if}

<style>
  .save {
    position: relative;
  }
  button {
    background: var(--well-deep);
    border: 1px solid var(--edge);
    border-radius: var(--radius);
    padding: 3px 10px;
    cursor: pointer;
    color: inherit;
  }
  button:hover:not(:disabled) {
    background: var(--well);
  }
  button:focus-visible {
    outline: none;
    box-shadow: var(--focus);
  }
  button:disabled {
    opacity: 0.4;
    cursor: default;
  }
  .panel {
    position: absolute;
    z-index: 900;
    top: calc(100% + 6px);
    right: 0;
    width: 430px;
    padding: 12px;
    background: var(--caption);
    border: 1px solid var(--warn);
    border-radius: var(--radius-lg);
    box-shadow: 0 8px 24px rgba(0, 0, 0, 0.5);
  }
  h2 {
    margin: 0 0 6px;
    font: 600 13px var(--font-caps);
  }
  .warn {
    margin: 0 0 10px;
    color: var(--warn);
  }
  .row {
    display: flex;
    gap: 8px;
    align-items: flex-end;
    margin: 8px 0;
  }
  .row.end {
    justify-content: flex-end;
  }
  .danger {
    background: var(--bad);
    color: #120404;
    border-color: var(--bad);
    font-weight: 600;
  }
  .confirm p,
  .undo p {
    margin: 4px 0;
  }
  .dim {
    color: var(--text-dim);
    word-break: break-all;
  }
  .ok {
    color: var(--ok);
  }
  .err {
    color: var(--bad);
  }
</style>
