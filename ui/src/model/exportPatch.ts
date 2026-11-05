// Writing the patch open in the editor to a file (the toolbar's export function). The file is a User Patch bank holding this one patch in slot 1, built by the same
// Rust code as bank and library exports, so it opens again with File > Open and can be sent to the instrument as it is.

import type { Backend } from './backend';
import { download, toMidiFile } from './midiFile';
import type { PatchStore } from './store.svelte';

export type ExportFormat = 'syx' | 'mid';

/** A file name made from the patch name: letters, digits, space, dash and underscore only. */
export function exportFileName(patchName: string, format: ExportFormat): string {
  const base = patchName.trim().replace(/[^A-Za-z0-9 _-]/g, '').replace(/\s+/g, ' ').trim();
  return `${base || 'patch'}.${format}`;
}

/** The bytes of the open patch as a one-patch bank, in the chosen format. */
export async function exportPatchBytes(store: PatchStore, backend: Pick<Backend, 'bankToSyx'>, format: ExportFormat): Promise<Uint8Array> {
  const syx = await backend.bankToSyx([[0, store.snapshot()]], store.deviceId);
  return format === 'mid' ? toMidiFile(syx) : syx;
}

/** Export and offer as a download; the result is the message to show the user. */
export async function exportPatch(store: PatchStore, backend: Pick<Backend, 'bankToSyx'>, format: ExportFormat): Promise<string> {
  try {
    const bytes = await exportPatchBytes(store, backend, format);
    const name = exportFileName(store.name, format);
    download(name, bytes);
    return `Exported ${name}.`;
  } catch (e) {
    return `Export failed: ${e instanceof Error ? e.message : String(e)}`;
  }
}
