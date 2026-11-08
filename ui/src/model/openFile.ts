// Getting a file the user picked or dropped into the right place (parity rows C4 and D9). One function per destination,
// shared by the menu, the buttons and drag and drop, so every route treats a file the same way.

import { asSyx } from './midiFile';
import { errorText, reportWarning } from './messages.svelte';
import { PatchStore } from './store.svelte';
import { ui } from './ui.svelte';

/** Largest file taken in; a whole User bank is about 112 KB. */
export const MAX_FILE_BYTES = 16 * 1024 * 1024;

export type Destination = 'patch' | 'performance' | 'rhythm' | 'system' | 'library' | 'bank';

/**
 * The keyboard skips a damaged message and carries on; so does the app. Say so, so a patch that came in with a part at its initial values is no surprise.
 * Nothing is said for a clean file (or for one that is not a patch bank: those fail with their own message).
 */
export function damageNotice(bytes: Uint8Array, fileName: string): string | null {
  try {
    const { skipped, filled } = PatchStore.syxReport(bytes);
    if (skipped === 0) return null;
    const parts = filled.map(([slot, part]) => `patch ${String(slot + 1).padStart(3, '0')} ${part}`);
    const skippedText = `${skipped} message${skipped === 1 ? '' : 's'} in ${fileName} ${skipped === 1 ? 'was' : 'were'} damaged or not XP-60 data and skipped, as the keyboard does`;
    return parts.length > 0 ? `${skippedText}. Reset to initial values: ${parts.join(', ')}.` : `${skippedText}.`;
  } catch {
    return null;
  }
}

/** Open the first patch (or Performance, or Rhythm Set, in those workspaces) of a `.syx` or `.mid` into the editor; the others come from the bank picker. */
export function openPatchFile(store: PatchStore, raw: Uint8Array, fileName: string): void {
  try {
    if (raw.length > MAX_FILE_BYTES) throw new Error(`That file is larger than ${MAX_FILE_BYTES / 1_048_576} MB; a .syx bank is far smaller.`);
    const bytes = asSyx(raw);
    const kind = store.kind === 'performance' || store.kind === 'rhythm' ? store.kind : 'patch';
    const names = PatchStore.listSyx(bytes, kind);
    if (names.length === 0) throw new Error(`That file holds no ${kind === 'patch' ? 'patches' : kind === 'performance' ? 'Performances' : 'Rhythm Sets'}.`);
    if (kind === 'patch') {
      const note = damageNotice(bytes, fileName);
      if (note) reportWarning(note);
    }
    ui.bank = { kind, file: fileName, bytes, names };
    ui.bankSlot = 0;
    store.loadSyx(bytes, names[0]?.[0] ?? 0, fileName);
  } catch (err) {
    ui.bank = null;
    store.error = errorText(err);
  }
}

/** Names of the patches a file holds, for showing before anything is imported or loaded. Throws a readable error. */
export function previewFile(raw: Uint8Array, fileName = 'the file'): { bytes: Uint8Array; names: Array<[number, string]> } {
  if (raw.length > MAX_FILE_BYTES) throw new Error(`That file is larger than ${MAX_FILE_BYTES / 1_048_576} MB; a .syx bank is far smaller.`);
  const bytes = asSyx(raw);
  const names = PatchStore.listSyx(bytes);
  const note = damageNotice(bytes, fileName);
  if (note) reportWarning(note);
  return { bytes, names };
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
