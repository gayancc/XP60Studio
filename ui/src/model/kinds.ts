// One interface over the three kinds of document the Rust codec offers (patch, performance, rhythm set).

import { Doc, PatchDoc, docParamAddress, paramAddress } from '../wasm/pkg/xppatch.js';
import {
  PART_BLOCKS,
  RHYTHM_NOTE_BLOCKS,
  SCALE_TUNE_BLOCKS,
  TONE_BLOCKS,
  blockInfo,
  type BlockInfo,
  type BlockName,
  type Catalog,
  type DocKindName,
} from './catalog';

/** What the editor needs from a document; both wasm classes provide it. */
export interface DocApi {
  name(): string;
  setName(name: string): void;
  get(block: string, id: string): number;
  set(block: string, id: string, value: number): void;
  format(block: string, id: string): string;
  formatValue(block: string, id: string, value: number): string;
  toBlocks(): Uint8Array;
  paramFrame(device: number, block: string, id: string): Uint8Array;
  patchFrames(device: number): Uint8Array;
  free(): void;
}

export function newDoc(kind: DocKindName): DocApi {
  return kind === 'patch' ? new PatchDoc() : new Doc(kind);
}

/** Open slot `slot` of the Performances or Rhythm Sets in a `.syx` file (whole-memory backups hold 32 and 2). */
export function docFromSyxSlot(kind: 'performance' | 'rhythm', bytes: Uint8Array, slot: number): DocApi {
  return Doc.fromSyxSlot(kind, bytes, slot);
}

/** A Performance, Rhythm Set or the System settings as `.syx` bytes (a Performance or Rhythm Set in User slot 1). */
export function docToSyx(kind: 'performance' | 'rhythm' | 'system', blocks: Uint8Array, device: number): Uint8Array {
  const doc = Doc.fromBlocks(kind, blocks);
  try {
    return doc.toSyx(device);
  } finally {
    doc.free();
  }
}

export function docFromBlocks(kind: DocKindName, bytes: Uint8Array): DocApi {
  return kind === 'patch' ? PatchDoc.fromBlocks(bytes) : Doc.fromBlocks(kind, bytes);
}

/** Address of a parameter inside the instrument's temporary copy, used to coalesce live edits. */
export function paramAddressOf(kind: DocKindName, block: BlockName, id: string): number {
  return kind === 'patch' ? paramAddress(block, id) : docParamAddress(kind, block, id);
}

/** Every block of a document, in transmission order. */
export function blockNames(kind: DocKindName): readonly BlockName[] {
  const items =
    kind === 'patch'
      ? TONE_BLOCKS
      : kind === 'performance'
        ? PART_BLOCKS
        : kind === 'rhythm'
          ? RHYTHM_NOTE_BLOCKS
          : SCALE_TUNE_BLOCKS;
  return ['common', ...items];
}

export interface BlockSlot {
  block: BlockName;
  /** Byte offset of the block in the concatenated snapshot. */
  start: number;
  info: BlockInfo;
}

/** Where each block sits in the concatenated snapshot produced by `toBlocks()`. */
export function blockLayout(catalog: Catalog, kind: DocKindName): BlockSlot[] {
  let start = 0;
  return blockNames(kind).map((block) => {
    const info = blockInfo(catalog, kind, block);
    const slot = { block, start, info };
    start += info.size;
    return slot;
  });
}

export function snapshotLength(catalog: Catalog, kind: DocKindName): number {
  return blockLayout(catalog, kind).reduce((n, s) => n + s.info.size, 0);
}

export const KIND_TITLES: Record<DocKindName, string> = {
  patch: 'Patch',
  performance: 'Performance',
  rhythm: 'Rhythm set',
  system: 'System',
};
