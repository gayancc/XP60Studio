// Small pure helpers around MIDI frames and document snapshots.

import type { BlockName, Catalog, DocKindName } from './catalog';
import { blockLayout } from './kinds';

const SYSEX_END = 0xf7;

/** Split concatenated SysEx frames (each ending in F7) into separate frames. Trailing bytes without an F7 are dropped. */
export function splitFrames(bytes: Uint8Array): Uint8Array[] {
  const out: Uint8Array[] = [];
  let start = 0;
  bytes.forEach((b, i) => {
    if (b === SYSEX_END) {
      out.push(bytes.slice(start, i + 1));
      start = i + 1;
    }
  });
  return out;
}

export interface ChangedParam {
  block: BlockName;
  id: string;
}

/** Parameters whose bytes differ between two snapshots of a document of `kind`. */
export function changedParams(
  catalog: Catalog,
  kind: DocKindName,
  before: Uint8Array,
  after: Uint8Array,
): ChangedParam[] {
  const out: ChangedParam[] = [];
  for (const slot of blockLayout(catalog, kind)) {
    for (const p of slot.info.params) {
      for (let i = 0; i < p.bytes; i++) {
        if (before[slot.start + p.offset + i] !== after[slot.start + p.offset + i]) {
          out.push({ block: slot.block, id: p.id });
          break;
        }
      }
    }
  }
  return out;
}

export function sameBytes(a: Uint8Array, b: Uint8Array): boolean {
  return a.length === b.length && a.every((v, i) => v === b[i]);
}
