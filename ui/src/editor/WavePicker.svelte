<script lang="ts">
  import { tick } from 'svelte';
  import type { BlockName } from '../model/catalog';
  import { expansionBoards, internalWaveNames, waveLabel } from '../model/factory';
  import { expansions, SLOT_NAMES } from '../model/expansions.svelte';
  import { blockTitle } from '../model/labels';
  import { useStore } from './context';

  // The wave of a Tone, chosen from a browser instead of a number: the instrument's own waves and the waves of the expansion boards
  // ticked as installed in Settings. A board whose wave names are not held is chosen by number; nothing is guessed.
  interface Props {
    block: BlockName;
    color: string;
    /** Called when the browser opens, so the card can take focus as the selected Tone. */
    onopen?: () => void;
  }
  let { block, color, onopen }: Props = $props();
  const store = useStore();

  interface Source {
    key: string;
    label: string;
    sub: string;
    /** Wave group type and ID as the instrument stores them. */
    type: number;
    id: number;
    names: readonly string[] | null;
    /** Marked installed, or part of the instrument. False for a source the Tone points at but Settings does not list. */
    listed: boolean;
  }

  const type = $derived(store.get(block, 'wave_group_type'));
  const gid = $derived(store.get(block, 'wave_group_id'));
  const num = $derived(store.get(block, 'wave_number') + 1);

  const board = (n: number) => expansionBoards.find((b) => b.number === n);

  const sources = $derived.by((): Source[] => {
    const list: Source[] = [
      { key: 'A', label: 'Internal A', sub: 'XP-60', type: 0, id: 1, names: internalWaveNames('INT-A'), listed: true },
      { key: 'B', label: 'Internal B', sub: 'XP-60', type: 0, id: 2, names: internalWaveNames('INT-B'), listed: true },
    ];
    for (const n of expansions.installed) {
      const b = board(n);
      const slot = expansions.slotOf(n);
      const where = slot >= 0 ? `${SLOT_NAMES[slot]} · ` : '';
      list.push({ key: `E${n}`, label: b?.name ?? `Board ${n}`, sub: `${where}SR-JV80-${String(n).padStart(2, '0')}`, type: 2, id: n, names: b?.waves ?? null, listed: true });
    }
    // A Tone can point at a board that is not in the keyboard. The instrument cannot select such a group and does not play its waves, so it is shown as unavailable
    // (nothing here can be chosen from it). Any other pointer the instrument has no name for is not offered as a group at all.
    if (type === 2 && !list.some((s) => s.type === type && s.id === gid)) {
      const b = board(gid);
      list.push({ key: `X${type}-${gid}`, label: b?.name ?? `Board ${gid}`, sub: `SR-JV80-${String(gid).padStart(2, '0')} · not installed`, type, id: gid, names: b?.waves ?? null, listed: false });
    }
    return list;
  });

  const currentName = $derived(waveLabel(type, gid, num));
  const code = $derived(`${store.format(block, 'wave_group_type')} ${store.format(block, 'wave_number')}`);

  let open = $state(false);
  let trigger: HTMLButtonElement | undefined = $state();
  let popup: HTMLDivElement | undefined = $state();
  let search: HTMLInputElement | undefined = $state();
  let pos = $state({ left: 0, top: 0 });
  let activeKey = $state('A');
  let query = $state('');
  let hi = $state(0);
  let typed = $state(1);

  const active = $derived(sources.find((s) => s.key === activeKey) ?? sources[0]);
  const rows = $derived.by(() => {
    const names = active?.names ?? [];
    const q = query.trim().toLowerCase();
    return names.map((name, i) => ({ number: i + 1, name })).filter((r) => q === '' || r.name.toLowerCase().includes(q) || String(r.number) === q);
  });
  const isCurrent = (s: Source): boolean => s.type === type && s.id === gid;

  async function show(): Promise<void> {
    const cur = sources.find(isCurrent) ?? sources[0];
    activeKey = cur?.key ?? 'A';
    query = '';
    typed = num;
    const r = trigger?.getBoundingClientRect();
    const W = 420;
    const H = 330;
    const left = Math.max(8, Math.min((r?.left ?? 8), window.innerWidth - W - 8));
    const below = (r?.bottom ?? 0) + 4;
    pos = { left, top: below + H > window.innerHeight - 8 ? Math.max(8, (r?.top ?? 0) - H - 4) : below };
    open = true;
    onopen?.();
    await tick();
    hi = Math.max(0, rows.findIndex((x) => x.number === num));
    (popup?.querySelector('li.cur') as HTMLElement | null)?.scrollIntoView?.({ block: 'center' });
    search?.focus();
  }

  function close(refocus = true): void {
    open = false;
    if (refocus) trigger?.focus();
  }

  function choose(s: Source, n: number): void {
    if (!s.listed) return; // the instrument cannot select a group of a board that is not installed
    // One undo step and one whole-patch live send: type/id/number must not leave the instrument on a missing board mid-update.
    store.applyMany(
      [
        { block, id: 'wave_group_type', value: s.type },
        { block, id: 'wave_group_id', value: s.id },
        { block, id: 'wave_number', value: n - 1 },
      ],
      `${blockTitle(block)} · Wave`,
    );
    close();
  }

  async function move(delta: number): Promise<void> {
    if (rows.length === 0) return;
    hi = Math.max(0, Math.min(rows.length - 1, hi + delta));
    await tick();
    (popup?.querySelector('.hi') as HTMLElement | null)?.scrollIntoView?.({ block: 'nearest' });
  }

  function keydown(e: KeyboardEvent): void {
    if (e.key === 'Escape') {
      e.preventDefault();
      e.stopPropagation();
      close();
    } else if (e.key === 'ArrowDown') {
      e.preventDefault();
      void move(1);
    } else if (e.key === 'ArrowUp') {
      e.preventDefault();
      void move(-1);
    } else if (e.key === 'PageDown') {
      e.preventDefault();
      void move(8);
    } else if (e.key === 'PageUp') {
      e.preventDefault();
      void move(-8);
    } else if (e.key === 'Enter') {
      e.preventDefault();
      const r = rows[hi];
      if (active && r) choose(active, r.number);
    }
  }

  function outside(e: PointerEvent): void {
    if (!open) return;
    const t = e.target as Node;
    if (popup?.contains(t) || trigger?.contains(t)) return;
    close(false);
  }
</script>

<svelte:window onpointerdown={outside} />

<button
  type="button"
  class="trig"
  class:open
  bind:this={trigger}
  aria-haspopup="listbox"
  aria-expanded={open}
  aria-label="Wave of {blockTitle(block)}: {currentName ?? code}. Choose another."
  title={currentName ? `${code} · ${currentName}` : `${code} · name not in the lists held`}
  onclick={() => (open ? close() : void show())}
>
  <span class="nm">{currentName ?? code}</span>
  <svg viewBox="0 0 8 5" aria-hidden="true"><path d="M0 0h8L4 5z" /></svg>
</button>

{#if open}
  <div class="pop" bind:this={popup} style:left="{pos.left}px" style:top="{pos.top}px" style:--tone={color} role="dialog" aria-label="Choose a wave for {blockTitle(block)}" tabindex="-1" onkeydown={keydown}>
    <header>
      <span class="who"><i></i>{blockTitle(block)}</span>
      <span class="now">{currentName ? `${currentName}` : code}</span>
    </header>
    <div class="cols">
      <nav aria-label="Wave sources">
        {#each sources as s (s.key)}
          <button type="button" class="src" class:on={s.key === activeKey} class:cur={isCurrent(s)} onclick={() => { activeKey = s.key; query = ''; hi = 0; search?.focus(); }}>
            <span class="l">{s.label}</span>
            <span class="s">{s.sub}{s.names ? ` · ${s.names.length}` : ''}</span>
          </button>
        {/each}
        <p class="hint">Boards appear here when ticked in Settings › Installed expansion boards.</p>
      </nav>
      <div class="list">
        {#if active?.names}
          {#if !active.listed}<p class="gone">This board is not installed, so the keyboard cannot select its waves and will not play them. Tick it in Settings › Installed expansion boards.</p>{/if}
          <input bind:this={search} type="text" placeholder="Search {active.label}…" aria-label="Search waves" bind:value={query} oninput={() => (hi = 0)} />
          <ul role="listbox" aria-label="{active.label} waves">
            {#each rows as r, i (r.number)}
              {@const here = isCurrent(active) && r.number === num}
              <li role="option" aria-selected={here} class:hi={i === hi} class:cur={here}>
                <button type="button" tabindex="-1" disabled={!active.listed} onpointerenter={() => (hi = i)} onclick={() => choose(active, r.number)}>
                  <span class="num">{String(r.number).padStart(3, '0')}</span>
                  <span class="wn" class:blank={r.name === ''}>{r.name || 'Not printed in Roland’s list'}</span>
                </button>
              </li>
            {:else}
              <li class="none">No wave matches.</li>
            {/each}
          </ul>
        {:else if active && !active.listed}
          <div class="byno"><p>This board is not installed, so the keyboard cannot select its waves and will not play them. Tick it in Settings › Installed expansion boards.</p></div>
        {:else if active}
          <div class="byno">
            <p>Roland publishes no waveform list for <b>{active.label}</b>, so its waves are chosen by number and shown as “{active.label} · number”.</p>
            <label>
              <span>Wave number</span>
              <input
                bind:this={search}
                type="number"
                min="1"
                max="255"
                bind:value={typed}
                aria-label="Wave number"
                onkeydown={(e) => {
                  if (e.key === 'Enter') {
                    e.preventDefault();
                    e.stopPropagation();
                    if (active && Number.isInteger(typed) && typed >= 1 && typed <= 255) choose(active, typed);
                  }
                }}
              />
            </label>
            <button type="button" class="go" disabled={!Number.isInteger(typed) || typed < 1 || typed > 255} onclick={() => active && choose(active, typed)}>Use wave {Number.isInteger(typed) ? String(typed).padStart(3, '0') : '…'}</button>
          </div>
        {/if}
      </div>
    </div>
  </div>
{/if}

<style>
  .gone {
    margin: 0 0 6px;
    color: var(--text-faint);
    font-size: 11px;
  }
  /* No box: the name of the wave is the title of the card, and it is the control. The chevron appears when the pointer is on it. */
  .trig {
    display: flex;
    align-items: center;
    gap: 6px;
    width: 100%;
    min-width: 0;
    padding: 2px 4px;
    background: transparent;
    color: inherit;
    border: 0;
    border-radius: var(--radius);
    cursor: pointer;
    font: 600 13px var(--font-caps);
    letter-spacing: 0.03em;
    text-align: left;
  }
  .trig:hover,
  .trig.open {
    background: color-mix(in srgb, var(--text) 8%, transparent);
    color: var(--accent);
  }
  .trig:focus-visible {
    outline: none;
    background-image: var(--focus-fill);
  }
  .nm {
    flex: 1;
    min-width: 0;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }
  svg {
    width: 8px;
    height: 5px;
    fill: currentColor;
    flex: none;
    opacity: 0;
  }
  .trig:hover svg,
  .trig.open svg,
  .trig:focus-visible svg {
    opacity: 0.8;
  }
  .pop {
    position: fixed;
    z-index: 950;
    width: 420px;
    height: 330px;
    display: flex;
    flex-direction: column;
    background: var(--body);
    border: 1px solid var(--edge-light);
    border-top: 3px solid var(--tone);
    border-radius: var(--radius-lg);
    box-shadow: 0 12px 32px rgba(0, 0, 0, 0.55);
    outline: none;
  }
  header {
    display: flex;
    align-items: baseline;
    justify-content: space-between;
    gap: 10px;
    padding: 6px 10px;
    background: var(--caption);
    border-bottom: 1px solid var(--edge);
  }
  .who {
    display: flex;
    align-items: center;
    gap: 6px;
    font: 600 10.5px var(--font-caps);
    letter-spacing: 0.08em;
    text-transform: uppercase;
    color: var(--text-dim);
  }
  .who i {
    width: 8px;
    height: 8px;
    border-radius: 50%;
    background: var(--tone);
  }
  .now {
    font: 600 12px var(--font-caps);
    color: var(--accent);
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }
  .cols {
    flex: 1;
    min-height: 0;
    display: grid;
    grid-template-columns: 138px 1fr;
  }
  nav {
    display: flex;
    flex-direction: column;
    gap: 2px;
    padding: 6px;
    background: var(--body-deep);
    border-right: 1px solid var(--edge);
    overflow-y: auto;
  }
  .src {
    display: flex;
    flex-direction: column;
    align-items: flex-start;
    gap: 1px;
    padding: 5px 8px;
    background: transparent;
    border: 0;
    border-left: 3px solid transparent;
    border-radius: var(--radius);
    color: var(--text-dim);
    cursor: pointer;
    text-align: left;
    font: inherit;
  }
  .src:hover {
    background: var(--well-deep);
    color: var(--text);
  }
  .src.on {
    background: color-mix(in srgb, var(--tone) 16%, var(--body-deep));
    color: var(--text);
  }
  .src.cur {
    border-left-color: var(--tone);
  }
  .src:focus-visible {
    outline: none;
    background-image: var(--focus-fill);
  }
  .l {
    font: 600 11.5px var(--font-caps);
    letter-spacing: 0.03em;
    max-width: 100%;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }
  .s {
    font: 9.5px var(--font-mono);
    color: var(--text-faint);
  }
  .hint {
    margin: auto 0 0;
    padding: 6px 4px 0;
    font-size: 9.5px;
    line-height: 1.35;
    color: var(--text-faint);
  }
  .list {
    display: flex;
    flex-direction: column;
    gap: 6px;
    padding: 6px;
    min-height: 0;
  }
  .list > input,
  .byno input {
    background: var(--track);
    color: var(--text);
    border: 1px solid var(--edge);
    border-radius: var(--radius);
    padding: 5px 8px;
    font: 12px var(--font);
  }
  .list > input:focus,
  .byno input:focus {
    outline: none;
    border-color: var(--accent);
  }
  ul {
    flex: 1;
    min-height: 0;
    margin: 0;
    padding: 0;
    list-style: none;
    overflow-y: auto;
    background: var(--body-deep);
    border: 1px solid var(--edge);
    border-radius: var(--radius);
  }
  li button {
    display: flex;
    gap: 10px;
    width: 100%;
    padding: 3px 10px;
    background: transparent;
    border: 0;
    border-left: 3px solid transparent;
    color: var(--text);
    cursor: pointer;
    text-align: left;
    font: inherit;
    font-size: 12px;
  }
  li.hi button {
    background: var(--well-deep);
  }
  li.cur button {
    background: color-mix(in srgb, var(--tone) 18%, var(--body-deep));
    border-left-color: var(--tone);
    font-weight: 600;
  }
  .num {
    font: 10.5px var(--font-mono);
    color: var(--text-faint);
    width: 24px;
  }
  .wn {
    min-width: 0;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }
  .wn.blank {
    color: var(--text-faint);
    font-style: italic;
  }
  .none {
    padding: 10px;
    color: var(--text-dim);
    font-size: 11px;
  }
  .byno {
    display: flex;
    flex-direction: column;
    gap: 10px;
    padding: 6px 4px;
    font-size: 11.5px;
    color: var(--text-dim);
  }
  .byno p {
    margin: 0;
    line-height: 1.4;
  }
  .byno b {
    color: var(--text);
  }
  .byno label {
    display: flex;
    flex-direction: column;
    gap: 3px;
    font: 10px var(--font-caps);
    letter-spacing: 0.08em;
    text-transform: uppercase;
  }
  .go {
    align-self: flex-start;
    padding: 5px 12px;
    background: var(--accent);
    color: var(--accent-ink);
    border: 0;
    border-radius: var(--radius);
    cursor: pointer;
    font: 600 11.5px var(--font-caps);
  }
  .go:disabled {
    opacity: 0.4;
    cursor: default;
  }
</style>
