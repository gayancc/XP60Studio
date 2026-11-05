// The parameter catalogue, produced by the Rust codec (WebAssembly) from data/xp60-parameters.json.
// The UI reads ranges, names and display kinds from here; nothing about parameters is written out by hand in the UI.

export type DisplayInfo =
  | { kind: 'raw' }
  | { kind: 'character' }
  | { kind: 'numeric'; first: number; step: number; min: number; pad: number; signed: boolean }
  | { kind: 'pan'; centre: number }
  | { kind: 'note' }
  | { kind: 'options'; min: number; labels: string[] };

export interface ParamInfo {
  id: string;
  name: string;
  offset: number;
  bytes: number;
  min: number;
  max: number;
  display: DisplayInfo;
}

export interface BlockInfo {
  id: string;
  name: string;
  size: number;
  params: ParamInfo[];
}

/** What is being edited. */
export type DocKindName = 'patch' | 'performance' | 'rhythm' | 'system';

/** The workspaces: one per kind of document, plus the library. */
export type Mode = DocKindName | 'library' | 'bank';

export interface Catalog {
  common: BlockInfo;
  tone: BlockInfo;
  performanceCommon: BlockInfo;
  performancePart: BlockInfo;
  rhythmCommon: BlockInfo;
  rhythmNote: BlockInfo;
  systemCommon: BlockInfo;
  scaleTune: BlockInfo;
}

/** A block label: `common`; for patches `tone1` to `tone4`; performances `part1` to `part16`; rhythm sets `note35` to `note98`. */
export type BlockName = string;

export const TONE_BLOCKS: readonly BlockName[] = ['tone1', 'tone2', 'tone3', 'tone4'];
export const PART_BLOCKS: readonly BlockName[] = Array.from({ length: 16 }, (_, i) => `part${i + 1}`);
/** Scale Tune blocks: one per Part (1 to 16) and one for Patch mode (17). */
export const SCALE_TUNE_BLOCKS: readonly BlockName[] = Array.from({ length: 17 }, (_, i) => `tune${i + 1}`);
export const RHYTHM_FIRST_NOTE = 35;
export const RHYTHM_NOTE_BLOCKS: readonly BlockName[] = Array.from(
  { length: 64 },
  (_, i) => `note${RHYTHM_FIRST_NOTE + i}`,
);

export function parseCatalog(json: string): Catalog {
  const parsed = JSON.parse(json) as { blocks: BlockInfo[] };
  const find = (id: string): BlockInfo => {
    const b = parsed.blocks.find((x) => x.id === id);
    if (!b) throw new Error(`catalogue is missing block ${id}`);
    return b;
  };
  return {
    common: find('patch_common'),
    tone: find('patch_tone'),
    performanceCommon: find('performance_common'),
    performancePart: find('performance_part'),
    rhythmCommon: find('rhythm_common'),
    rhythmNote: find('rhythm_note'),
    systemCommon: find('system_common'),
    scaleTune: find('scale_tune'),
  };
}

/** The definition of block `block` in a document of `kind`. */
export function blockInfo(catalog: Catalog, kind: DocKindName, block: BlockName): BlockInfo {
  if (kind === 'patch') return block === 'common' ? catalog.common : catalog.tone;
  if (kind === 'performance') return block === 'common' ? catalog.performanceCommon : catalog.performancePart;
  if (kind === 'system') return block === 'common' ? catalog.systemCommon : catalog.scaleTune;
  return block === 'common' ? catalog.rhythmCommon : catalog.rhythmNote;
}

export function paramInfo(catalog: Catalog, kind: DocKindName, block: BlockName, id: string): ParamInfo | undefined {
  return blockInfo(catalog, kind, block).params.find((p) => p.id === id);
}

/**
 * One character of a name. Patch names come with a character display; performance and rhythm-set names are plain numbers
 * in the catalogue, so they are recognised by their ids (`performance_name_3`, `rhythm_name_12`).
 */
export function isNameCharacter(p: ParamInfo): boolean {
  return p.display.kind === 'character' || /_name_\d+$/.test(p.id);
}
