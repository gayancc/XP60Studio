// Find and replace in patch names (parity row E3). Names are at most 12 printable ASCII characters, so a replacement that
// would not fit or would leave an unusable name is reported and skipped rather than cut or stored.

export const NAME_LENGTH = 12;

export interface NameRow {
  id: number;
  name: string;
}

export type Skip = 'too long' | 'empty' | 'not plain text';

export interface RenamePlan {
  id: number;
  from: string;
  to: string;
  /** Why this rename will not be done, or null when it will. */
  skip: Skip | null;
}

const printable = (s: string): boolean => [...s].every((c) => c >= ' ' && c <= '~');

/** Every shown name that contains `find`, with what it would become. Whole-name matching is not assumed: any part is replaced. */
export function planRename(rows: readonly NameRow[], find: string, replace: string, matchCase = false): RenamePlan[] {
  if (find === '') return [];
  const needle = matchCase ? find : find.toLowerCase();
  const plans: RenamePlan[] = [];
  for (const r of rows) {
    const from = r.name.trimEnd();
    const hay = matchCase ? from : from.toLowerCase();
    if (!hay.includes(needle)) continue;
    // Replace every occurrence, keeping the text between them as it was (case-insensitive search must not change it).
    let to = '';
    let at = 0;
    for (let i = hay.indexOf(needle); i !== -1; i = hay.indexOf(needle, at)) {
      to += from.slice(at, i) + replace;
      at = i + needle.length;
    }
    to += from.slice(at);
    to = to.trimEnd();
    const skip: Skip | null = !printable(to) ? 'not plain text' : to.trim() === '' ? 'empty' : to.length > NAME_LENGTH ? 'too long' : null;
    if (to !== from) plans.push({ id: r.id, from, to, skip });
  }
  return plans;
}

/** A summary line for the preview: "3 names will change; 1 is skipped (too long)". */
export function summarise(plans: readonly RenamePlan[]): string {
  if (plans.length === 0) return 'No shown name would change.';
  const ok = plans.filter((p) => p.skip === null).length;
  const skipped = plans.length - ok;
  const reasons = [...new Set(plans.flatMap((p) => (p.skip ? [p.skip] : [])))].join(', ');
  const base = `${ok} name${ok === 1 ? '' : 's'} will change`;
  return skipped > 0 ? `${base}; ${skipped} skipped (${reasons}).` : `${base}.`;
}
