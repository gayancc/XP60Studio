// Writing what is open in the editor to a file (the toolbar's export function). A patch goes out as a User Patch bank holding it in slot 1, built by the same Rust code
// as bank and library exports; a Performance or Rhythm Set the same way in its own slot 1; the System settings as their own messages. Each opens again with File > Open
// (not the System settings, which have no slots) and can be sent to the instrument as it is.

import type { Backend } from './backend';
import { docToSyx } from './kinds';
import { toMidiFile } from './midiFile';
import { saveFile } from './saveFile';
import type { PatchStore } from './store.svelte';

export type ExportFormat = 'syx' | 'mid';

/** A file name made from the patch name: letters, digits, space, dash and underscore only. */
export function exportFileName(patchName: string, format: ExportFormat, fallback = 'patch'): string {
  const base = patchName.trim().replace(/[^A-Za-z0-9 _-]/g, '').replace(/\s+/g, ' ').trim();
  return `${base || fallback}.${format}`;
}

/** What the open thing is called in messages and in the file name when it has no name of its own. */
export const EXPORT_NOUN = { patch: 'patch', performance: 'performance', rhythm: 'rhythm set', system: 'System settings' } as const;

/** The bytes of the open patch as a one-patch bank, in the chosen format. */
export async function exportPatchBytes(store: PatchStore, backend: Pick<Backend, 'bankToSyx'>, format: ExportFormat): Promise<Uint8Array> {
  const syx = store.kind === 'patch' ? await backend.bankToSyx([[0, store.snapshot()]], store.deviceId) : docToSyx(store.kind, store.snapshot(), store.deviceId);
  return format === 'mid' ? toMidiFile(syx) : syx;
}

/** Export and offer as a download; the result is the message to show the user. */
export async function exportPatch(store: PatchStore, backend: Pick<Backend, 'bankToSyx'>, format: ExportFormat): Promise<string> {
  try {
    const bytes = await exportPatchBytes(store, backend, format);
    const name = exportFileName(store.kind === 'system' ? '' : store.name, format, EXPORT_NOUN[store.kind].replace(' ', '-'));
    return await saveFile(name, bytes);
  } catch (e) {
    return `Export failed: ${e instanceof Error ? e.message : String(e)}`;
  }
}
