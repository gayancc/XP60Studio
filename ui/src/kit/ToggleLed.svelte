<script lang="ts">
  import { clearHint, showHint } from './hint.svelte';
  import Led from './Led.svelte';

  interface Props {
    checked: boolean;
    label: string;
    name?: string;
    context?: string;
    color?: string;
    disabled?: boolean;
    onchange?: (checked: boolean) => void;
  }

  let {
    checked = $bindable(),
    label,
    name = label,
    context = '',
    color = 'var(--accent)',
    disabled = false,
    onchange,
  }: Props = $props();

  const publish = (): void =>
    showHint({ context, name, value: checked ? 'On' : 'Off', shortcut: 'Click or Space to toggle' });

  function toggle(): void {
    if (disabled) return;
    checked = !checked;
    onchange?.(checked);
    publish();
  }
</script>

<button
  type="button"
  class="toggle"
  role="switch"
  aria-checked={checked}
  aria-label={label === '' ? name : undefined}
  title={label === '' ? name : undefined}
  {disabled}
  onclick={toggle}
  onpointerenter={publish}
  onpointerleave={clearHint}
  onfocus={publish}
  onblur={clearHint}
>
  <Led on={checked} {color} />
  <span class="caps">{label}</span>
</button>

<style>
  .toggle {
    display: inline-flex;
    align-items: center;
    gap: 5px;
    padding: 2px 6px;
    background: transparent;
    border: 1px solid transparent;
    border-radius: var(--radius);
    cursor: pointer;
  }
  .toggle:hover:not(:disabled) {
    background: var(--caption);
  }
  .toggle:focus-visible {
    outline: none;
    box-shadow: var(--focus);
  }
  .toggle:disabled {
    opacity: 0.4;
    cursor: default;
  }
  .toggle[aria-checked='true'] .caps {
    color: var(--text);
  }
</style>
