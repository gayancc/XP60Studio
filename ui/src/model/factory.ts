// The XP-60's own reference lists: factory patch names, internal wave names and the SR-JV80 expansion boards. They are names
// only (no sound data), generated from Roland's published documents by scripts/build_factory_data.py into data/factory.

import raw from '../../../data/factory/xp60-factory.json';

export interface ExpansionBoard {
  readonly number: number;
  readonly name: string;
  /** Wave names, 1 first, when Roland's waveform list for the board is held; otherwise null. An entry may be '' where Roland's list is cut off. */
  readonly waves: readonly string[] | null;
}

interface FactoryData {
  readonly patchBanks: Readonly<Record<string, readonly string[]>>;
  readonly internalWaves: Readonly<Record<string, readonly string[]>>;
  readonly expansionBoards: readonly ExpansionBoard[];
}

const data = raw as FactoryData;

/** The banks of factory patches, in the order Roland lists them. `PR-D` is the General MIDI bank. */
export const FACTORY_BANKS = ['USER', 'PR-A', 'PR-B', 'PR-C', 'PR-D'] as const;
export type FactoryBank = (typeof FACTORY_BANKS)[number];

export const BANK_TITLES: Readonly<Record<FactoryBank, string>> = {
  USER: 'User bank, as shipped',
  'PR-A': 'Preset A',
  'PR-B': 'Preset B',
  'PR-C': 'Preset C',
  'PR-D': 'Preset D (GM)',
};

/**
 * Preset memory is read-only and the same in every XP-60 (it is ROM, not user data), so these lists describe every instrument.
 * The User bank is the opposite: it is rewritable memory that the owner fills with their own sounds, so its list is only how it
 * left the factory and says nothing about what a given instrument holds now. (Owner's Manual p.45: temporary and rewritable
 * memory are System and User; Preset and Wave Expansion memory cannot be rewritten.)
 */
export const isFixedBank = (bank: FactoryBank): boolean => bank !== 'USER';

/** One sentence on what a bank's list means, for the panel under it. */
export function bankNote(bank: FactoryBank): string {
  return isFixedBank(bank)
    ? 'Built into every XP-60 and read-only: the same on every instrument, and it cannot be overwritten. Load one into the editor, change it, and save the copy to User memory.'
    : 'The User bank as it left the factory. User memory is rewritable and yours to fill, so your own instrument may hold something else.';
}

/** Expansion boards are read-only too: their waves and patches are fixed once the board is installed, and only sound while it is. */
export const EXPANSION_NOTE =
  'Wave Expansion boards are fixed: their waves and patches are read-only, and they only sound while the board is in its slot (EXP-A to EXP-D).';

/**
 * Where a Part's Patch lives: Patch Group Type and Patch Group ID as the instrument stores them. Roland prints the table in the XP-30 and JV-1010 Owner's
 * Manuals (MIDI Implementation, System Common, next to Patch Group ID): USER is type 0, ID 1; PR-A to PR-D (GM) are type 0, IDs 3 to 6 (ID 2 is the CARD bank,
 * which those models mark as not present and whose absence does not move the others up); an expansion board is type 2 with the SR-JV80 board number as the ID
 * (XP-A "Session" 9, XP-B "Orchestral" 2, XP-C "Techno" 11). The XP-60/80 manual prints the Group Type names and its Bank Select table (p.214; its fourth preset group is called GM, "PR-D" is the
 * XP-30's label) but no Group ID table, so the IDs are the same family's documented scheme, still to be confirmed on the XP-60 itself (ledger H-058).
 */
export interface PatchGroup {
  readonly key: string;
  readonly label: string;
  readonly type: number;
  readonly id: number;
  /** The bank in the factory lists when it is fixed ROM; null for User memory, which is the owner's own. */
  readonly fixedBank: FactoryBank | null;
}

export const PATCH_GROUPS: readonly PatchGroup[] = [
  { key: 'USER', label: 'USER', type: 0, id: 1, fixedBank: null },
  { key: 'PR-A', label: 'PR-A', type: 0, id: 3, fixedBank: 'PR-A' },
  { key: 'PR-B', label: 'PR-B', type: 0, id: 4, fixedBank: 'PR-B' },
  { key: 'PR-C', label: 'PR-C', type: 0, id: 5, fixedBank: 'PR-C' },
  { key: 'PR-D', label: 'PR-D (GM)', type: 0, id: 6, fixedBank: 'PR-D' },
];

/** The group a type and ID stand for, if it is one of the five on the XP-60 (an expansion board is type 2 and is looked up with `boardName`). */
export function patchGroupOf(type: number, id: number): PatchGroup | undefined {
  return PATCH_GROUPS.find((g) => g.type === type && g.id === id);
}

/** Roland's factory name for a Patch in a fixed bank (`raw` is the stored number, 0 to 127); undefined for User memory and for expansion boards. */
export function presetPatchName(type: number, id: number, raw: number): string | undefined {
  const bank = patchGroupOf(type, id)?.fixedBank;
  return bank ? factoryPatchName(bank, raw + 1) : undefined;
}

/** Name of a factory patch, number 1 to 128, or undefined. */
export function factoryPatchName(bank: FactoryBank, number: number): string | undefined {
  return data.patchBanks[bank]?.[number - 1];
}

/** All 128 names of a factory bank. */
export function factoryBank(bank: FactoryBank): readonly string[] {
  return data.patchBanks[bank] ?? [];
}

export const expansionBoards: readonly ExpansionBoard[] = data.expansionBoards;

export function boardName(number: number): string | undefined {
  return data.expansionBoards.find((b) => b.number === number)?.name;
}

/**
 * The name of the wave a Tone points at. Group type 0 is the instrument's own waves (group ID 1 is INT-A, 2 is INT-B, confirmed on
 * the instrument by the earlier project); group type 2 is an SR-JV80 board whose number is the group ID (a corroborated
 * reading, not yet confirmed on this project's hardware run, see the acceptance ledger). Anything else, and any wave outside a
 * list we hold, has no name here: it is shown by number only, never guessed.
 */
export function waveName(groupType: number, groupId: number, waveNumber: number): string | undefined {
  if (groupType === 0) {
    const bank = groupId === 1 ? 'INT-A' : groupId === 2 ? 'INT-B' : undefined;
    return bank ? data.internalWaves[bank]?.[waveNumber - 1] : undefined;
  }
  if (groupType === 2) return data.expansionBoards.find((b) => b.number === groupId)?.waves?.[waveNumber - 1] || undefined;
  return undefined;
}

/**
 * Something readable to put on a Tone for any wave: Roland's name for it when the lists hold one, otherwise where it lives and its
 * number ("Piano · 012", "Internal B · 012"). Never blank and never invented.
 */
export function waveLabel(groupType: number, groupId: number, waveNumber: number): string {
  const name = waveName(groupType, groupId, waveNumber);
  if (name) return name;
  const n = String(waveNumber).padStart(3, '0');
  if (groupType === 0) return `${groupId === 1 ? 'Internal A' : groupId === 2 ? 'Internal B' : `Group ${groupId}`} · ${n}`;
  if (groupType === 2) return `${boardName(groupId) ?? `Board ${groupId}`} · ${n}`;
  return `Wave ${n}`;
}

/** The names of the instrument's own waves in INT-A or INT-B, wave 1 first. */
export function internalWaveNames(bank: 'INT-A' | 'INT-B'): readonly string[] {
  return data.internalWaves[bank] ?? [];
}

/** Number of waves held per internal bank, for display and tests. */
export const internalWaveCounts: Readonly<Record<string, number>> = Object.fromEntries(
  Object.entries(data.internalWaves).map(([k, v]) => [k, v.length]),
);
