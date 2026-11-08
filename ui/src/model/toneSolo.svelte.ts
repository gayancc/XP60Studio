// Solo is not a stored XP-60 parameter. It isolates one of the four Tones by Tone Switch:
// that Tone on, the other three off, as one undo step. Clicking the same Solo again
// restores the four switches from before the solo. Soloing a different Tone moves the
// isolation and still restores that same earlier mix. A new patch drops the session
// without writing. Editing a switch by hand ends the session, so the next Solo starts
// from the switches as they are now.

import { TONE_BLOCKS, type BlockName } from './catalog';
import { blockTitle } from './labels';
import type { PatchStore } from './store.svelte';

let active = $state<BlockName | null>(null);
let saved: number[] | null = null;

export function resetToneSolo(): void {
  active = null;
  saved = null;
}

/** True only while this Tone is the isolated one and the four switches still say so. */
export function toneIsSolo(store: PatchStore, block: BlockName): boolean {
  return active === block && saved !== null && isolated(store, block);
}

function switches(store: PatchStore): number[] {
  return TONE_BLOCKS.map((b) => store.get(b, 'tone_switch'));
}

function isolated(store: PatchStore, block: BlockName): boolean {
  return TONE_BLOCKS.every((b) => store.get(b, 'tone_switch') === (b === block ? 1 : 0));
}

function apply(store: PatchStore, values: readonly number[], label: string): void {
  store.applyMany(
    TONE_BLOCKS.map((block, i) => ({ block, id: 'tone_switch', value: values[i] ?? 0 })),
    label,
  );
}

/** Press Solo on one Tone. See the module note for what a second press does. */
export function toneSoloPress(store: PatchStore, block: BlockName): void {
  if (store.kind !== 'patch') return;
  if (active === block && saved && isolated(store, block)) {
    apply(store, saved, 'Clear Tone solo');
    resetToneSolo();
    return;
  }
  if (active === null || saved === null || !isolated(store, active)) saved = switches(store);
  apply(
    store,
    TONE_BLOCKS.map((b) => (b === block ? 1 : 0)),
    `Solo ${blockTitle(block)}`,
  );
  active = block;
}
