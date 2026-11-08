<script lang="ts">
  import { onDestroy } from 'svelte';
  import { Stepper } from '../kit';
  import { useStore } from './context';
  import { useServices } from './services';

  const store = useStore();
  const services = useServices();
  const flow = $derived(services?.save);
  const bank = $derived(services?.bank);

  let open = $state(false);
  let trigger: HTMLButtonElement | undefined = $state();
  let pos = $state({ top: 0, right: 0 });

  // The panel is placed against the window (fixed), not inside the top bar, which clips what hangs below it.
  function place(): void {
    const r = trigger?.getBoundingClientRect();
    pos = { top: (r?.bottom ?? 0) + 6, right: Math.max(8, window.innerWidth - (r?.right ?? window.innerWidth)) };
  }

  /** When the Patch editor was opened from a Bank pad, that pad is the only safe write target. */
  const linkedSlot = $derived(bank?.editorSlot ?? null);
  let slot = $state(0);
  /** Only when there is no bank link, or the musician explicitly unlocks a different slot. */
  let pickOther = $state(false);
  let confirmUndo = $state(false);
  let cancelButton: HTMLButtonElement | undefined = $state();
  const label = (n: number): string => `USER:${String(n + 1).padStart(3, '0')}`;

  const targetSlot = $derived(linkedSlot !== null && !pickOther ? linkedSlot : slot);
  const slotLocked = $derived(linkedSlot !== null && !pickOther);

  function openPanel(): void {
    place();
    pickOther = false;
    confirmUndo = false;
    if (linkedSlot !== null) slot = linkedSlot;
    else if (flow) slot = flow.slot;
    open = true;
  }

  // Leaving this workspace while a save is armed must not leave it armed.
  onDestroy(() => {
    void flow?.cancel();
  });

  async function close(): Promise<void> {
    if (flow?.working) return; // an arm or a write in progress is not interrupted
    await flow?.cancel();
    flow?.reset();
    confirmUndo = false;
    pickOther = false;
    open = false;
  }

  async function arm(): Promise<void> {
    if (!flow) return;
    confirmUndo = false;
    // The patch is frozen now: what is confirmed below is exactly what gets written.
    const ok = await flow.arm(targetSlot, store.snapshot(), store.name.trimEnd());
    // Keep the bank grid in sync with what we are about to write (and wrote after commit via assign on success).
    if (ok && bank && linkedSlot === targetSlot) bank.pullEditor(store.snapshot());
  }

  async function commit(): Promise<void> {
    if (!flow) return;
    const ok = await flow.commit();
    if (ok && bank && flow.report) bank.markWritten(flow.report.slot, store.snapshot());
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
      bind:this={trigger}
      aria-expanded={open}
      aria-controls="save-panel"
      disabled={flow.working}
      onclick={() => (open ? close() : openPanel())}>Save to synth…</button>
    {#if open}
      <div
        id="save-panel"
        class="panel"
        style:top="{pos.top}px"
        style:right="{pos.right}px"
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
          This <b>replaces</b> a patch inside the XP-60 for good. Live editing never does this.
        </p>

        {#if flow.phase === 'idle' || flow.phase === 'failed' || flow.phase === 'done'}
          {#if slotLocked}
            <p class="target">
              Write to <b>{label(targetSlot)}</b>
              <span class="dim"> — the Bank pad this patch was opened from</span>
            </p>
            <button type="button" class="linkish" onclick={() => (pickOther = true)}>Choose a different User slot…</button>
          {:else}
            {#if linkedSlot !== null && pickOther}
              <p class="dim warn-soft">
                This patch was opened from <b>{label(linkedSlot)}</b>. Picking another slot can overwrite the wrong User patch.
              </p>
              <button type="button" class="linkish" onclick={() => { pickOther = false; slot = linkedSlot; }}>Use Bank pad {label(linkedSlot)} again</button>
            {/if}
            <div class="row">
              <Stepper bind:value={slot} min={0} max={127} label="User slot" name="User slot" format={(v) => String(v + 1)} />
            </div>
          {/if}
          <label class="backup-choice" title="When on, the patch currently in that User slot is saved to a .syx before writing. When off, the slot is still read first (no backup file).">
            <input type="checkbox" bind:checked={flow.backupBeforeWrite} disabled={flow.working} />
            Back up the existing patch in that slot to a file first
          </label>
          <div class="row">
            <button type="button" onclick={arm}>
              {flow.backupBeforeWrite ? `1. Read ${label(targetSlot)} and back it up` : `1. Prepare ${label(targetSlot)}`}
            </button>
          </div>
        {/if}

        {#if flow.phase === 'arming'}
          <div class="row"><button type="button" disabled aria-busy="true">{flow.backupBeforeWrite ? `Reading ${label(flow.slot)} and backing it up…` : `Preparing ${label(flow.slot)}…`}</button></div>
        {/if}

        {#if flow.phase === 'armed' && flow.armed}
          <div class="confirm" role="alert">
            <p>
              <b>{label(flow.armed.slot)}</b> holds <b>“{flow.armed.occupant_name.trimEnd() || '(unnamed)'}”</b>.
            </p>
            {#if flow.armed.backup_path}
              <p class="dim">Backed up to {flow.armed.backup_path}</p>
            {:else}
              <p class="dim">No backup file (you chose to skip it).</p>
            {/if}
            <p>
              Replace it with <b>“{flow.frozenName || '(unnamed)'}”</b> (the patch as it is now; later edits are not included)?
            </p>
            <div class="row">
              <button type="button" bind:this={cancelButton} onclick={() => flow.cancel()}>Cancel</button>
              <button type="button" class="danger" onclick={() => void commit()}>2. Replace {label(flow.armed.slot)}</button>
            </div>
          </div>
        {/if}

        {#if flow.phase === 'writing'}
          <div class="row"><button type="button" disabled aria-busy="true">Writing and reading back… keep the instrument connected</button></div>
        {/if}

        {#if flow.last && !flow.working && flow.phase !== 'armed'}
          <div class="undo">
            {#if confirmUndo}
              <p>
                Put <b>“{flow.last.oldName.trimEnd() || '(unnamed)'}”</b> back into <b>{label(flow.last.slot)}</b>? The backup is
                <span class="dim">{flow.last.backupPath || '(none)'}</span>. This replaces what is there now.
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
              <button type="button" onclick={() => (confirmUndo = true)} disabled={!flow.last.backupPath}>Undo the last save…</button>
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
    height: var(--face-h);
    padding: 0 12px;
    border-radius: 2px;
    border: 1px solid var(--face-rim);
    background: var(--face-off);
    color: var(--face-ink);
    cursor: pointer;
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
  .warn-soft {
    margin: 0 0 8px;
    color: var(--warn);
  }
  .target {
    margin: 0 0 6px;
    font-size: 13px;
  }
  .linkish {
    display: inline-block;
    margin: 0 0 10px;
    padding: 0;
    background: none;
    border: none;
    color: var(--accent, #e8a04a);
    text-decoration: underline;
    cursor: pointer;
    font-size: 12px;
  }
  .backup-choice {
    display: flex;
    align-items: flex-start;
    gap: 8px;
    margin: 0 0 10px;
    font-size: 12px;
    line-height: 1.35;
    cursor: pointer;
  }
  .backup-choice input {
    margin-top: 2px;
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
