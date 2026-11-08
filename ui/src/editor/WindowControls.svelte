<script lang="ts">
  import { onMount } from 'svelte';

  // The app draws its own title bar (the native one is off), so the window's minimize, maximize and close live here. They are
  // only offered inside the desktop app; in a browser there is no window to drive.
  const desktop = typeof window !== 'undefined' && '__TAURI_INTERNALS__' in window;
  let maximized = $state(false);
  let api: Awaited<ReturnType<typeof load>> | undefined;

  async function load() {
    const { getCurrentWindow } = await import('@tauri-apps/api/window');
    return getCurrentWindow();
  }

  async function refresh(): Promise<void> {
    try {
      maximized = (await api?.isMaximized()) ?? false;
    } catch {
      maximized = false;
    }
  }

  onMount(() => {
    if (!desktop) return;
    let off: (() => void) | undefined;
    void load().then(async (w) => {
      api = w;
      await refresh();
      off = await w.onResized(() => void refresh());
    });
    return () => off?.();
  });

  const run = (what: 'minimize' | 'toggleMaximize' | 'close') => (): void => {
    void api?.[what]();
  };
</script>

{#if desktop}
  <div class="wc" role="group" aria-label="Window">
    <button type="button" aria-label="Minimize" title="Minimize" onclick={run('minimize')}>
      <svg viewBox="0 0 10 10" aria-hidden="true"><path d="M1 5.5h8" /></svg>
    </button>
    <button type="button" aria-label={maximized ? 'Restore' : 'Maximize'} title={maximized ? 'Restore' : 'Maximize'} onclick={run('toggleMaximize')}>
      <svg viewBox="0 0 10 10" aria-hidden="true">
        {#if maximized}<path d="M2.5 3.5h5v5h-5zM3.5 3.5v-2h5v5h-1" />{:else}<path d="M1.5 1.5h7v7h-7z" />{/if}
      </svg>
    </button>
    <button type="button" aria-label="Close" title="Close" onclick={run('close')}>
      <svg viewBox="0 0 10 10" aria-hidden="true"><path d="M1.5 1.5l7 7M8.5 1.5l-7 7" /></svg>
    </button>
  </div>
{/if}

<style>
  .wc {
    flex: none;
    align-self: flex-start;
    display: flex;
    flex-direction: row;
    gap: 1px;
    margin-top: 2px;
  }
  button {
    /* FL main-window caption buttons, measured 2026-11-08: 19 px convex bevel, min then max then close. */
    width: 19px;
    height: 19px;
    padding: 0;
    display: grid;
    place-items: center;
    background: var(--face-off);
    border: 1px solid var(--face-rim);
    border-radius: 0;
    color: var(--face-ink);
    cursor: pointer;
  }
  button:hover {
    background: var(--face-off);
    color: var(--face-ink);
  }
  button:focus-visible {
    outline: none;
    background-image: var(--focus-fill);
  }
  svg {
    width: 9px;
    height: 9px;
    fill: none;
    stroke: currentColor;
    stroke-width: 1.2;
    stroke-linecap: square;
  }
</style>
