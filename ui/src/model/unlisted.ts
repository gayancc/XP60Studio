// Raw values the instrument's parameter table gives a name in angle brackets (for example `<OUTPUT-2>`) but the Owner's Manual's value lists do not offer: Output Assign is
// MIX, EFX or DIR (pp.61-63, 75), EFX Output Assign is MIX or DIR (pp.61, 63, p.199 value column). They are kept readable (a patch that holds one shows it) but are not
// offered as a choice. Whether the XP-60 accepts them is not known: the hardware ledger records this (H-068).
export const UNLISTED: Readonly<Record<string, readonly number[]>> = {
  output_assign: [3],
  efx_output_assign: [2],
};

/** The list entries to offer for a parameter: every entry except an unlisted one, unless the patch currently holds it. */
export function offered(id: string, base: number, count: number, current: number): number[] {
  const hidden = UNLISTED[id] ?? [];
  return Array.from({ length: count }, (_, i) => i).filter((i) => !hidden.includes(base + i) || base + i === current);
}

export const isUnlisted = (id: string, raw: number): boolean => (UNLISTED[id] ?? []).includes(raw);
