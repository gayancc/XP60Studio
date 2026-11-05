<script lang="ts">
  import { onMount } from 'svelte';
  import type { Backend, StorageInfo } from '../model/backend';

  interface Props {
    backend: Backend;
  }

  let { backend }: Props = $props();

  let info = $state<StorageInfo | null>(null);
  let path = $state('');
  let busy = $state(false);
  let message = $state('');
  let failed = $state(false);

  const pending = $derived(info !== null && info.configured !== info.in_use);

  async function load(): Promise<void> {
    try {
      info = await backend.storageInfo();
      path = info.custom ? info.configured : '';
    } catch (e) {
      failed = true;
      message = e instanceof Error ? e.message : String(e);
    }
  }

  onMount(() => void load());

  async function apply(dir: string | null): Promise<void> {
    busy = true;
    failed = false;
    message = '';
    try {
      info = await backend.storageSet(dir);
      path = info.custom ? info.configured : '';
      message = dir === null ? 'Back to the default folder from the next start.' : 'Saved. The library and backups were copied there; nothing was removed from the old folder.';
    } catch (e) {
      failed = true;
      message = typeof e === 'string' ? e : e instanceof Error ? e.message : String(e);
    } finally {
      busy = false;
    }
  }
</script>

<div class="storage">
  <span class="caps">Library and backups folder</span>
  {#if info}
    <p class="now" title={info.in_use}>In use: <b>{info.in_use}</b></p>
    <input
      type="text"
      spellcheck="false"
      placeholder={info.default}
      bind:value={path}
      aria-label="Folder for the library and backups"
      onkeydown={(e) => e.key === 'Enter' && path.trim() && apply(path)}
    />
    <div class="row">
      <button type="button" disabled={busy || !path.trim() || path.trim() === info.configured} onclick={() => apply(path)}>Use this folder</button>
      <button type="button" disabled={busy || !info.custom} onclick={() => apply(null)}>Use the default</button>
    </div>
    {#if pending}<p class="note">Applies when the app is next started.</p>{/if}
  {:else if !failed}
    <p class="note">Loading…</p>
  {/if}
  {#if message}<p class:err={failed} class="note" role={failed ? 'alert' : 'status'}>{message}</p>{/if}
  <p class="note">Choosing a folder copies the library and the backup files into it (an existing library there is kept as it is). Nothing is moved or deleted.</p>
</div>

<style>
  .storage {
    display: flex;
    flex-direction: column;
    gap: 6px;
    width: 340px;
  }
  .now {
    margin: 0;
    color: var(--text-dim);
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }
  .now b {
    color: var(--text);
    font-weight: normal;
    font-family: var(--font-mono);
    font-size: 11px;
  }
  input {
    background: var(--track);
    color: var(--text);
    border: 1px solid var(--edge);
    border-radius: var(--radius);
    padding: 4px 6px;
    font-family: var(--font-mono);
    font-size: 11px;
  }
  input:focus {
    outline: none;
    border-color: var(--accent);
  }
  .row {
    display: flex;
    gap: 6px;
  }
  button {
    background: var(--well-deep);
    border: 1px solid var(--edge);
    border-radius: var(--radius);
    padding: 3px 10px;
    cursor: pointer;
    color: inherit;
    font: inherit;
  }
  button:hover:not(:disabled) {
    background: var(--well);
  }
  button:disabled {
    opacity: 0.4;
    cursor: default;
  }
  .note {
    margin: 0;
    color: var(--text-dim);
    font-size: 11px;
  }
  .err {
    color: var(--bad);
  }
</style>
