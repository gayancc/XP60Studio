<script lang="ts">
  import { reportWarning } from '../model/messages.svelte';
  import { tick } from 'svelte';
  import { conflicts, keyOf, search } from '../model/shortcuts';
  import { isAcceptableKey, type KeyMap } from '../model/shortcuts';

  export interface PaletteItem {
    id: string;
    label: string;
    hint: string;
    run: () => void;
  }

  export interface CommandInfo {
    id: string;
    label: string;
  }

  interface Props {
    open: boolean;
    items: readonly PaletteItem[];
    commands: readonly CommandInfo[];
    keys: KeyMap;
    onclose: () => void;
    onkeys: (map: KeyMap) => void;
  }

  let { open, items, commands, keys, onclose, onkeys }: Props = $props();

  let query = $state('');
  let index = $state(0);
  let editing = $state(false);
  let capturing = $state<string | null>(null);
  let input: HTMLInputElement | undefined = $state();
  let opener: HTMLElement | null = null;

  const results = $derived(search(query, items, (i) => `${i.label} ${i.hint}`));
  const clashes = $derived(new Set(conflicts(keys).flatMap(([, ids]) => ids)));

  // Two commands on one key: neither runs. Say so in the corner when a key is shared (the rows stay tinted).
  $effect(() => {
    for (const [key, ids] of conflicts(keys)) {
      const names = ids.map((id) => commands.find((c) => c.id === id)?.label ?? id);
      reportWarning(`The shortcut ${key} is shared by ${names.join(' and ')}: neither works.`);
    }
  });

  $effect(() => {
    if (open) {
      opener = document.activeElement instanceof HTMLElement ? document.activeElement : null;
      query = '';
      index = 0;
      editing = false;
      capturing = null;
      void tick().then(() => input?.focus());
    }
  });

  $effect(() => {
    // Keep the highlighted row inside the list when the results shrink.
    if (index >= results.length) index = Math.max(0, results.length - 1);
  });

  function close(): void {
    onclose();
    opener?.focus();
    opener = null;
  }

  function run(item: PaletteItem | undefined): void {
    if (!item) return;
    onclose();
    opener = null; // the command decides where focus goes (a jump focuses its control)
    item.run();
  }

  function dialogKey(e: KeyboardEvent): void {
    if (e.key === 'Escape' && !capturing) {
      e.preventDefault();
      close();
    }
  }

  function key(e: KeyboardEvent): void {
    if (e.key === 'ArrowDown') {
      e.preventDefault();
      index = Math.min(results.length - 1, index + 1);
    } else if (e.key === 'ArrowUp') {
      e.preventDefault();
      index = Math.max(0, index - 1);
    } else if (e.key === 'Enter') {
      e.preventDefault();
      run(results[index]);
    }
  }

  function capture(e: KeyboardEvent): void {
    const id = capturing;
    if (!id) return;
    e.preventDefault();
    e.stopPropagation();
    if (e.key === 'Escape') {
      capturing = null;
      return;
    }
    if (e.key === 'Backspace' || e.key === 'Delete') {
      onkeys({ ...keys, [id]: '' });
      capturing = null;
      return;
    }
    const k = keyOf(e);
    if (!k || !isAcceptableKey(k)) return; // wait for a usable key
    onkeys({ ...keys, [id]: k });
    capturing = null;
  }
</script>

{#if open}
  <div class="backdrop" role="presentation" onpointerdown={close}></div>
  <div class="palette" role="dialog" aria-modal="true" aria-label="Command palette" tabindex="-1" onkeydown={dialogKey}>
    {#if !editing}
      <input
        bind:this={input}
        type="text"
        placeholder="Type a command or a parameter, for example “tvf cutoff”"
        aria-label="Search commands and parameters"
        aria-controls="palette-list"
        bind:value={query}
        oninput={() => (index = 0)}
        onkeydown={key}
      />
      <ul id="palette-list" role="listbox" aria-label="Results">
        {#each results as r, i (r.id)}
          <li role="option" aria-selected={i === index} class:sel={i === index}>
            <button type="button" tabindex="-1" onclick={() => run(r)} onpointerenter={() => (index = i)}>
              <span class="label">{r.label}</span>
              <span class="hint">{r.hint}</span>
            </button>
          </li>
        {:else}
          <li class="none">Nothing matches.</li>
        {/each}
      </ul>
      <div class="foot">
        <button type="button" onclick={() => (editing = true)}>Edit shortcuts…</button>
        <span>Enter runs · Esc closes</span>
      </div>
    {:else}
      <div class="shortcuts">
        <h2>Shortcuts</h2>
        <p class="note">Click a key, then press the new one (Backspace clears, Esc cancels). A shortcut needs Ctrl or Alt, or is a function key.</p>
        <table>
          <tbody>
            {#each commands as c (c.id)}
              <tr class:clash={clashes.has(c.id)}>
                <td>{c.label}</td>
                <td>
                  <button
                    type="button"
                    class="keybtn"
                    onclick={() => (capturing = c.id)}
                    onkeydown={capture}
                    onblur={() => capturing === c.id && (capturing = null)}
                    aria-label="Shortcut for {c.label}: {keys[c.id] || 'none'}"
                  >
                    {capturing === c.id ? 'Press a key…' : keys[c.id] || 'none'}
                  </button>
                </td>
              </tr>
            {/each}
          </tbody>
        </table>
        <div class="foot">
          <button type="button" onclick={() => onkeys({})}>Reset all</button>
          <button type="button" onclick={() => (editing = false)}>Back</button>
        </div>
      </div>
    {/if}
  </div>
{/if}

<style>
  .backdrop {
    position: fixed;
    inset: 0;
    z-index: 1100;
    background: rgba(0, 0, 0, 0.45);
  }
  .palette {
    position: fixed;
    z-index: 1101;
    top: 12vh;
    left: 50%;
    transform: translateX(-50%);
    width: min(640px, 92vw);
    background: var(--caption);
    border: 1px solid var(--edge-light);
    border-radius: var(--radius-lg);
    box-shadow: 0 12px 36px rgba(0, 0, 0, 0.55);
    padding: 8px;
  }
  input {
    width: 100%;
    background: var(--track);
    color: var(--text);
    border: 1px solid var(--edge);
    border-radius: var(--radius);
    padding: 8px 10px;
    font: inherit;
    font-size: 14px;
  }
  input:focus {
    outline: none;
    border-color: var(--accent);
  }
  ul {
    list-style: none;
    margin: 6px 0;
    padding: 0;
    max-height: 46vh;
    overflow: auto;
  }
  li button {
    display: flex;
    justify-content: space-between;
    gap: 12px;
    width: 100%;
    padding: 5px 8px;
    background: transparent;
    border: 0;
    border-radius: 2px;
    text-align: left;
    color: inherit;
    cursor: pointer;
  }
  li.sel button {
    background: var(--accent);
    color: var(--accent-ink);
  }
  .hint {
    color: var(--text-dim);
    font-size: 11px;
  }
  li.sel .hint {
    color: var(--accent-ink);
  }
  .none {
    padding: 8px;
    color: var(--text-dim);
  }
  .foot {
    display: flex;
    justify-content: space-between;
    align-items: center;
    color: var(--text-dim);
    font-size: 11px;
  }
  .foot button,
  .keybtn {
    background: var(--well-deep);
    border: 1px solid var(--edge);
    border-radius: var(--radius);
    padding: 3px 10px;
    cursor: pointer;
    color: var(--text);
  }
  .keybtn {
    min-width: 130px;
    font-family: var(--font-mono);
  }
  .keybtn:focus-visible,
  .foot button:focus-visible {
    outline: none;
    background-image: var(--focus-fill);
  }
  h2 {
    margin: 4px 0;
    font: 600 13px var(--font-caps);
  }
  .note {
    margin: 0 0 6px;
    color: var(--text-dim);
  }
  table {
    width: 100%;
    border-collapse: collapse;
    max-height: 50vh;
    display: block;
    overflow: auto;
  }
  td {
    padding: 2px 6px;
  }
  tr.clash td {
    background: rgba(239, 83, 80, 0.15);
  }
</style>
