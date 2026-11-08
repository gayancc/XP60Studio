// Path-stable Tone identity helpers (Patch Base SynthPath equivalent) and diagnostic checkpoints used to prove that
// edits to Tone 1–4 never bleed into another Tone's state or SysEx address.

import { paramAddress } from '../wasm/pkg/xppatch.js';
import { TONE_BLOCKS, type BlockName, type Catalog, type DocKindName } from './catalog';
import { blockLayout } from './kinds';

/**
 * Compact relative bases of Tone 1–4 inside a patch. Roland addresses pack 7-bit lanes, so lane `0x10 00` is compact
 * `0x10 << 7` (not `0x1000`). Matches Patch Base `xp80-voice.syx` tone DT1 starts at `10/12/14/16 00`.
 */
export const TONE_COMPACT_OFFSETS = [0x10 << 7, 0x12 << 7, 0x14 << 7, 0x16 << 7] as const;
/** Compact span reserved per Tone (next tone base − this base). */
export const TONE_COMPACT_SPAN = (0x12 << 7) - (0x10 << 7);

export interface ToneMutation {
  at: number;
  block: BlockName;
  id: string;
  before: number;
  after: number;
  /** Coalesce key / relative compact address inside the temporary patch map. */
  address: number;
}

/** Ring buffer of recent per-parameter Tone writes for before/after audits. */
export class ToneMutationLog {
  readonly entries: ToneMutation[] = [];
  constructor(private readonly capacity = 256) {}

  record(entry: ToneMutation): void {
    this.entries.push(entry);
    if (this.entries.length > this.capacity) this.entries.splice(0, this.entries.length - this.capacity);
  }

  clear(): void {
    this.entries.length = 0;
  }

  forBlock(block: BlockName): ToneMutation[] {
    return this.entries.filter((e) => e.block === block);
  }
}

/** Byte slice of one Tone block inside a patch `toBlocks()` snapshot. */
export function toneBytes(catalog: Catalog, snapshot: Uint8Array, block: BlockName): Uint8Array {
  const slot = blockLayout(catalog, 'patch').find((s) => s.block === block);
  if (!slot) throw new Error(`no layout for ${block}`);
  return snapshot.subarray(slot.start, slot.start + slot.info.size);
}

/** True when every Tone other than `edited` has identical bytes in `before` and `after`. */
export function otherTonesUnchanged(
  catalog: Catalog,
  before: Uint8Array,
  after: Uint8Array,
  edited: BlockName,
): boolean {
  for (const block of TONE_BLOCKS) {
    if (block === edited) continue;
    const a = toneBytes(catalog, before, block);
    const b = toneBytes(catalog, after, block);
    if (a.length !== b.length) return false;
    for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
  }
  return true;
}

/** Prove the four Tone addresses for one parameter id are distinct and land in the expected tone windows. */
export function assertToneAddressMap(id: string): { block: BlockName; address: number; toneIndex: number }[] {
  const rows = TONE_BLOCKS.map((block, toneIndex) => ({
    block,
    toneIndex,
    address: paramAddress(block, id),
  }));
  const seen = new Set<number>();
  for (const row of rows) {
    if (seen.has(row.address)) {
      throw new Error(`address collision for ${id}: ${row.block} shares ${row.address}`);
    }
    seen.add(row.address);
    const base = TONE_COMPACT_OFFSETS[row.toneIndex];
    if (base === undefined || row.address < base || row.address >= base + TONE_COMPACT_SPAN) {
      throw new Error(
        `${row.block}:${id} address 0x${row.address.toString(16)} is outside Tone window 0x${(base ?? 0).toString(16)}`,
      );
    }
  }
  return rows;
}

export function isToneBlock(block: BlockName, kind: DocKindName): boolean {
  return kind === 'patch' && (TONE_BLOCKS as readonly string[]).includes(block);
}
