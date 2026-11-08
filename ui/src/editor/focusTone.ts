import { tick } from 'svelte';

/**
 * Bring the multi-tone editor for `index` into view inside `root`.
 * Used whenever the left Tone cards (or another picker) change the selection while
 * a tab shows several Tones at once — Zones, and the All 4 Tones sound sections.
 * Pass `signal` from an `$effect` cleanup so a superseded selection does not scroll.
 */
export async function focusToneEditor(
  root: ParentNode | null | undefined,
  index: number,
  signal?: { cancelled: boolean },
): Promise<void> {
  await tick();
  if (signal?.cancelled) return;
  const el = root?.querySelector<HTMLElement>(`[data-tone-focus="${index}"]`);
  el?.scrollIntoView?.({ block: 'nearest', inline: 'nearest', behavior: 'smooth' });
}
