<script lang="ts">
  interface Props {
    tabs: readonly string[];
    selected: number;
    /** Accent colour of the selected tab's top edge. */
    color?: string;
    name?: string;
    /** `strip` sits flush on top of a panel; `pill` is a looser row for a workspace switch. */
    variant?: 'strip' | 'pill';
    onchange: (index: number) => void;
  }

  let { tabs, selected, color = 'var(--accent)', name = 'Tabs', variant = 'strip', onchange }: Props = $props();

  function key(e: KeyboardEvent): void {
    const dir = e.key === 'ArrowRight' ? 1 : e.key === 'ArrowLeft' ? -1 : 0;
    if (dir === 0) return;
    e.preventDefault();
    const next = (selected + dir + tabs.length) % tabs.length;
    onchange(next);
    const list = (e.currentTarget as HTMLElement).querySelectorAll<HTMLElement>('[role=tab]');
    list[next]?.focus();
  }
</script>

<div class="tabs {variant}" role="tablist" aria-label={name} style:--c={color} tabindex="-1" onkeydown={key}>
  {#each tabs as t, i (t)}
    <button
      type="button"
      role="tab"
      aria-selected={i === selected}
      tabindex={i === selected ? 0 : -1}
      class:on={i === selected}
      onclick={() => onchange(i)}>{t}</button>
  {/each}
</div>

<style>
  .tabs {
    display: flex;
    gap: 2px;
    min-width: 0;
  }
  button {
    padding: 4px 7px 3px;
    background: var(--well-deep);
    border: 1px solid var(--edge);
    border-bottom: 0;
    border-top: 2px solid transparent;
    border-radius: var(--radius-lg) var(--radius-lg) 0 0;
    color: var(--text-dim);
    font-family: var(--font-caps);
    font-size: 11px;
    letter-spacing: 0.04em;
    cursor: pointer;
    white-space: nowrap;
  }
  button:hover {
    color: var(--text);
    background: var(--well);
  }
  button.on {
    background: var(--body);
    color: var(--text);
    border-top-color: var(--c);
    position: relative;
    top: 1px;
  }
  button:focus-visible {
    outline: none;
    background-image: var(--focus-fill);
  }
  .pill button {
    border: 1px solid var(--edge);
    border-radius: var(--radius);
    padding: 3px 10px;
    top: 0;
  }
  .pill button.on {
    background: var(--accent);
    color: var(--accent-ink);
    border-top-color: var(--edge);
  }
</style>
