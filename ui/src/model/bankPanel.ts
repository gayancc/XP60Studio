// The XP-60 front panel picks a Patch in a group of 128 by sub-group (a or b: 64 Patches each), then BANK 1-8, then NUMBER 1-8
// (Owner's Manual p.26, "Selecting a Patch with Bank/Number method"). Patch a11 is number 001, a88 is 064, b11 is 065, b88 is 128.
// Panel coordinates are the front-panel way of choosing; the stored identity of a Patch stays its number 0..127.

export const PER_SUBGROUP = 64;
export const PER_BANK = 8;
export const SUBGROUPS = ['a', 'b'] as const;
export const PATCH_COUNT = 128;

export interface PanelPos {
  /** 0 for a, 1 for b. */
  sub: 0 | 1;
  /** 1 to 8. */
  bank: number;
  /** 1 to 8. */
  num: number;
}

/** Zero-based Patch index (0..127) for a panel position. */
export function slotOf(p: PanelPos): number {
  return p.sub * PER_SUBGROUP + (p.bank - 1) * PER_BANK + (p.num - 1);
}

/** Panel position of a zero-based Patch index. Out-of-range indexes are clamped to the first or last Patch. */
export function posOf(slot: number): PanelPos {
  const s = Math.min(PATCH_COUNT - 1, Math.max(0, Math.trunc(slot)));
  const inSub = s % PER_SUBGROUP;
  return { sub: s < PER_SUBGROUP ? 0 : 1, bank: Math.floor(inSub / PER_BANK) + 1, num: (inSub % PER_BANK) + 1 };
}

/** The label the display prints beside the number: "a11" .. "b88". */
export function panelLabel(slot: number): string {
  const p = posOf(slot);
  return `${SUBGROUPS[p.sub]}${p.bank}${p.num}`;
}

/** Number as the display prints it, 001..128. */
export const patchNumber = (slot: number): string => String(slot + 1).padStart(3, '0');

// ---- What the front panel's patch buttons say on MIDI ----
// Owner's Manual p.181, "Changing the Patch": Bank Select MSB/LSB and Program Number select a Patch from outside. Each group
// holds 128 Patches, so the Program Number is the Patch number 1-128 (sent as 0-127). XP-A to XP-D have a second half (129-256)
// at the next LSB; this app handles the first 128 of each only.

export interface BankSelect {
  readonly msb: number;
  readonly lsb: number;
}

export const GROUP_BANK_SELECT: Readonly<Record<string, BankSelect>> = {
  USER: { msb: 80, lsb: 0 },
  'PR-A': { msb: 81, lsb: 0 },
  'PR-B': { msb: 81, lsb: 1 },
  'PR-C': { msb: 81, lsb: 2 },
  'PR-D': { msb: 81, lsb: 3 },
  'XP-A': { msb: 84, lsb: 0 },
  'XP-B': { msb: 84, lsb: 2 },
  'XP-C': { msb: 84, lsb: 4 },
  'XP-D': { msb: 84, lsb: 6 },
};

export interface SoundSelect extends BankSelect {
  /** 0 to 127 as sent on the wire (the display's Patch number minus one). */
  readonly program: number;
}

/** The three messages that select Patch `slot` (0..127) of a group, or null for a group this app does not know. */
export function soundSelect(groupKey: string, slot: number): SoundSelect | null {
  const bank = GROUP_BANK_SELECT[groupKey];
  if (!bank || !Number.isInteger(slot) || slot < 0 || slot >= PATCH_COUNT) return null;
  return { ...bank, program: slot };
}
