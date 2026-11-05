<script lang="ts">
  import { onDestroy } from 'svelte';
  import { useStore } from './context';
  import { useServices } from './services';

  // Writing the System settings into the instrument. System settings have no temporary copy: this replaces them in the XP-60's permanent
  // memory, so it goes through the same two deliberate steps as saving a patch (read and back up, then replace) and can be undone.
  const store = useStore();
  const flow = useServices()?.systemSave;

  let open = $state(false);
  let trigger: HTMLButtonElement | undefined = $state();
  let pos = $state({ top: 0, right: 0 });

  // The panel is placed against the window (fixed), not inside the top bar, which clips what hangs below it.
  function place(): void {
    const r = trigger?.getBoundingClientRect();
    pos = { top: (r?.bottom ?? 0) + 6, right: Math.max(8, window.innerWidth - (r?.right ?? window.innerWidth)) };
  }
  let confirmUndo = $state(false);
  let cancelButton: HTMLButtonElement | undefined = $state();

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
    // The settings are frozen now: what is confirmed below is exactly what gets written.
    await flow?.arm(store.snapshot());
  }

  $effect(() => {
    // When the confirmation appears, focus the safe choice, not the destructive one.
    if (flow?.phase === 'armed') cancelButton?.focus();
  });
</script>

{#if flow}
  <div class="save">
    <button type="button" class="open" bind:this={trigger} aria-expanded={open} aria-controls="system-save-panel" disabled={flow.working} onclick={() => (open ? close() : ((open = true), place()))}>
      Write to synth…
    </button>
    {#if open}
      <div
        id="system-save-panel"
        class="panel"
        style:top="{pos.top}px"
        style:right="{pos.right}px"
        role="dialog"
        aria-modal="true"
        aria-label="Write the System settings to the instrument"
        tabindex="-1"
        onkeydown={(e) => {
          if (e.key === 'Escape') {
            e.preventDefault();
            void close();
          }
        }}
      >
        <h2>Write the System settings</h2>
        <p class="warn">
          This <b>replaces</b> the XP-60's System settings for good: master tune, controller and pedal assignments, MIDI receive and transmit switches and channels, scale tune
          and the rest of the System page. The settings now in the instrument are read and kept in a backup file first. Editing here never does this.
        </p>

        {#if flow.phase === 'idle' || flow.phase === 'failed' || flow.phase === 'done'}
          <div class="row"><button type="button" onclick={arm}>1. Read the settings and back them up</button></div>
        {/if}

        {#if flow.phase === 'arming'}
          <div class="row"><button type="button" disabled aria-busy="true">Reading the settings and backing them up…</button></div>
        {/if}

        {#if flow.phase === 'armed' && flow.armed}
          <div class="confirm" role="alert">
            <p class="dim">Backed up to {flow.armed.backup_path}</p>
            <p>Replace the instrument's System settings with the ones on this page (as they are now; later edits are not included)?</p>
            <div class="row">
              <button type="button" bind:this={cancelButton} onclick={() => flow.cancel()}>Cancel</button>
              <button type="button" class="danger" onclick={() => flow.commit()}>2. Replace the System settings</button>
            </div>
          </div>
        {/if}

        {#if flow.phase === 'writing'}
          <div class="row"><button type="button" disabled aria-busy="true">Writing and reading back… keep the instrument connected</button></div>
        {/if}

        {#if flow.canUndo && !flow.working && flow.phase !== 'armed'}
          <div class="undo">
            {#if confirmUndo}
              <p>
                Put the earlier System settings back? The backup is <span class="dim">{flow.lastBackupPath}</span>. This replaces what is in the instrument now.
              </p>
              <div class="row">
                <button type="button" onclick={() => (confirmUndo = false)}>Keep them</button>
                <button
                  type="button"
                  class="danger"
                  onclick={async () => {
                    confirmUndo = false;
                    await flow.undo();
                  }}>Put the old settings back</button>
              </div>
            {:else}
              <button type="button" onclick={() => (confirmUndo = true)}>Undo the last write…</button>
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
    background-image: var(--focus-fill);
  }
  button:disabled {
    opacity: 0.4;
    cursor: default;
  }
  .panel {
    position: fixed;
    z-index: 900;
    width: 460px;
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
</style>
