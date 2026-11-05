// `.mid` support: a Standard MIDI File that carries the same SysEx messages as a `.syx` (parity row D7). The conversion
// is done in Rust (xppatch-core::smf); this module only decides which files need it.

import { smfToSyx, syxToSmf } from '../wasm/pkg/xppatch.js';

/** True when the bytes start with a Standard MIDI File header. */
export function isMidiFile(bytes: Uint8Array): boolean {
  return bytes.length >= 4 && bytes[0] === 0x4d && bytes[1] === 0x54 && bytes[2] === 0x68 && bytes[3] === 0x64;
}

/** The SysEx data of a file the user picked: a `.mid` is converted, anything else is passed on unchanged. */
export function asSyx(bytes: Uint8Array): Uint8Array {
  return isMidiFile(bytes) ? smfToSyx(bytes) : bytes;
}

/** A `.syx` bank or patch as a Standard MIDI File. */
export function toMidiFile(syx: Uint8Array): Uint8Array {
  return syxToSmf(syx);
}

/** Offer bytes to the user as a download. */
export function download(fileName: string, bytes: Uint8Array): void {
  const url = URL.createObjectURL(new Blob([bytes as BlobPart], { type: 'application/octet-stream' }));
  const a = document.createElement('a');
  a.href = url;
  a.download = fileName;
  a.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}
