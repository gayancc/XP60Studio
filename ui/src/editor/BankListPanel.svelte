<script lang="ts">
  import StepFace from '../kit/StepFace.svelte';
  import { bankOf, canStepBank, openBankSlot, stepBankSlot } from '../model/bankNav';
  import { ui } from '../model/ui.svelte';
  import { useStore } from './context';

  // The patches of the bank that is open in the editor, as a list to jump around in. The bank is whatever .syx file was opened or dropped; nothing here talks to the instrument.
  const store = useStore();
  const bank = $derived(bankOf(store.kind));
  let list: HTMLOListElement | undefined = $state();

  $effect(() => {
    // keep the open patch in view when it changes from elsewhere (the picker, the buttons)
    void ui.bankSlot;
    (list?.querySelector('.on') as HTMLElement | null)?.scrollIntoView?.({ block: 'nearest' });
  });
</script>

{#if bank}
  <div class="bl">
    <div class="bar">
      <StepFace
        canPrev={canStepBank(store.kind, -1)}
        canNext={canStepBank(store.kind, 1)}
        onstep={(d) => stepBankSlot(store, d)}
        prevLabel="Previous patch"
        nextLabel="Next patch"
        wheel
        strip={bank.names.length > 1 ? ui.bankSlot / (bank.names.length - 1) : 0}
      >
        <div class="title"><span class="file" title={bank.file}>{bank.file}</span><span class="count">{ui.bankSlot + 1} / {bank.names.length}</span></div>
      </StepFace>
    </div>
    <ol bind:this={list} aria-label="Patches in {bank.file}">
      {#each bank.names as [slot, name], i (slot)}
        <li>
          <button type="button" class:on={i === ui.bankSlot} aria-current={i === ui.bankSlot ? 'true' : undefined} onclick={() => openBankSlot(store, i)}>
            <span class="n">{String(slot + 1).padStart(3, '0')}</span><span class="nm">{name.trimEnd() || '(unnamed)'}</span>
          </button>
        </li>
      {/each}
    </ol>
  </div>
{:else}
  <p class="none">No bank is open. Open a .syx bank with File &gt; Open, or drop one on the window, and its patches are listed here.</p>
{/if}

<style>
  .bl {
    display: flex;
    flex-direction: column;
    gap: 6px;
    height: 100%;
    min-height: 0;
  }
  .bar {
    display: flex;
    align-items: center;
    gap: 6px;
    flex: none;
  }
  .bar > :global(.face) {
    flex: 1;
  }
  .title {
    display: flex;
    align-items: center;
    gap: 8px;
    padding: 0 8px;
    min-width: 0;
  }
  .file {
    flex: 1;
    min-width: 0;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
    color: var(--text-dim);
    font-size: 11px;
  }
  .count {
    color: var(--text-faint);
    font: 10.5px var(--font-mono);
  }
  ol {
    flex: 1;
    min-height: 0;
    margin: 0;
    padding: 0;
    list-style: none;
    overflow-y: auto;
    background: var(--body-deep);
    border: 1px solid var(--edge);
    border-radius: var(--radius);
    scrollbar-width: thin;
    scrollbar-color: var(--edge-light) transparent;
  }
  li button {
    display: flex;
    gap: 10px;
    width: 100%;
    padding: 3px 8px;
    background: transparent;
    border: 0;
    color: #899093;
    cursor: pointer;
    text-align: left;
    font: inherit;
    font-size: 12px;
  }
  li button:hover {
    background: var(--well-deep);
  }
  li button.on {
    background: #2f3037;
    color: #b6b7b5;
    font-weight: 600;
  }
  li button:focus-visible {
    outline: none;
    background-image: var(--focus-fill);
  }
  li button.on .n {
    color: #b6b7b5;
  }
  .n {
    color: var(--text-faint);
    font: 10.5px var(--font-mono);
    width: 26px;
    flex: none;
  }
  .nm {
    min-width: 0;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }
  .none {
    margin: 0;
    color: var(--text-dim);
    font-size: 11.5px;
    line-height: 1.45;
  }
</style>
