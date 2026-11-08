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
    /* Effect-picker categories, measured from shot 05: text on the parent ground.
       Selected ink is white. Idle ink is about #76747a. No separate face and no accent bar. */
    padding: 4px 10px 5px;
    background: transparent;
    border: 0;
    border-radius: 0;
    color: #76747a;
    font-family: var(--font-caps);
    font-size: 11px;
    letter-spacing: 0.04em;
    cursor: pointer;
    white-space: nowrap;
  }
  button:hover {
    color: #76747a;
    background: transparent;
  }
  button.on {
    background: transparent;
    color: #ffffff;
    font-weight: 600;
    position: relative;
    top: 0;
  }
  button:focus-visible {
    outline: none;
    background-image: var(--focus-fill);
  }
  .pill button {
    height: var(--face-h);
    padding: 0 12px;
    top: 0;
    border: 1px solid var(--face-rim);
    border-radius: 2px;
    background: var(--face-off);
    color: var(--face-ink);
  }
  .pill button:hover {
    background: var(--face-off);
    color: var(--face-ink);
  }
  .pill button.on {
    background: var(--face-on);
    color: var(--face-ink-on);
    border-color: var(--face-rim-on);
  }
  .pill button:active:not(.on) {
    background: var(--face-on);
    color: var(--face-ink-on);
    border-color: var(--face-rim-on);
  }
  .pill button.on:active {
    background: var(--face-off);
    color: var(--face-ink);
    border-color: var(--face-rim);
  }
</style>
