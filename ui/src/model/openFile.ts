// Getting a file the user picked or dropped into the right place (parity rows C4 and D9). One function per destination,
// shared by the menu, the buttons and drag and drop, so every route treats a file the same way.

import { asSyx } from './midiFile';
import { PatchStore } from './store.svelte';
import { ui } from './ui.svelte';

/** Largest file taken in; a whole User bank is about 112 KB. */
export const MAX_FILE_BYTES = 16 * 1024 * 1024;

export type Destination = 'patch' | 'performance' | 'rhythm' | 'system' | 'library' | 'bank';

/** Open a patch from a `.syx` or `.mid` into the editor (the first patch of a bank; the others via the Bank menu). */
export function openPatchFile(store: PatchStore, raw: Uint8Array, fileName: string): void {
  try {
    if (raw.length > MAX_FILE_BYTES) throw new Error(`That file is larger than ${MAX_FILE_BYTES / 1_048_576} MB; a .syx bank is far smaller.`);
    const bytes = asSyx(raw);
    const names = PatchStore.listSyx(bytes);
    ui.bank = { file: fileName, bytes, names };
    ui.bankSlot = 0;
    store.loadSyx(bytes, names[0]?.[0] ?? 0, fileName);
  } catch (err) {
    ui.bank = null;
    store.error = err instanceof Error ? err.message : String(err);
  }
}

/** Names of the patches a file holds, for showing before anything is imported or loaded. Throws a readable error. */
export function previewFile(raw: Uint8Array): { bytes: Uint8Array; names: Array<[number, string]> } {
  if (raw.length > MAX_FILE_BYTES) throw new Error(`That file is larger than ${MAX_FILE_BYTES / 1_048_576} MB; a .syx bank is far smaller.`);
  const bytes = asSyx(raw);
  return { bytes, names: PatchStore.listSyx(bytes) };
}

/** What dropping a file means in each workspace, or why it does nothing there. */
export function dropMeaning(where: Destination): string | null {
  return where === 'patch' || where === 'library' || where === 'bank'
    ? null
    : 'Drop a .syx or .mid on the Patch, Library or Bank workspace.';
}

/** Does this drag carry files (and not, say, a library row dragged inside the Bank workspace)? */
export function carriesFiles(types: readonly string[] | undefined): boolean {
  return (types ?? []).includes('Files');
}
