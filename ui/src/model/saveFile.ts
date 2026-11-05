// Saving a file the user asked for. In the desktop app that is the system Save dialog (the window's own download would raise a browser permission
// prompt on the second file); the plain download is only the fallback for the developer preview, which has no Rust behind it.

import { tauriBackend } from './backend';
import { download } from './midiFile';
import { runningInDesktopApp } from './synth.svelte';

/** "Saved ALL RIGHT.syx": the file's name only. The folder is not shown in the top bar, where a long path would crowd everything else out. */
export function savedMessage(path: string): string {
  const name = path.split(/[\\/]/).pop() || path;
  return `Saved ${name}`;
}

/** Save `bytes` as `fileName`; the result is the message to show ("Saved ...", or that the dialog was cancelled). */
export async function saveFile(fileName: string, bytes: Uint8Array): Promise<string> {
  if (!runningInDesktopApp()) {
    download(fileName, bytes);
    return `Exported ${fileName}.`;
  }
  try {
    const path = await tauriBackend.saveFile(fileName, bytes);
    return path ? savedMessage(path) : 'Save cancelled.';
  } catch (e) {
    return `Could not save ${fileName}: ${typeof e === 'string' ? e : e instanceof Error ? e.message : String(e)}`;
  }
}
