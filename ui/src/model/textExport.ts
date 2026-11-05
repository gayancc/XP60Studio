// A plain-text description of the open sound, for pasting into notes, a forum post or a bug report (parity row B13).

import { blockInfo, isNameCharacter } from './catalog';
import { blockNames } from './kinds';
import { blockTitle } from './labels';
import type { PatchStore } from './store.svelte';

/** "Name\n\nTone 1\n  Cutoff Frequency: 64\n ..." with every parameter as the user sees it. */
export function describeSound(store: PatchStore): string {
  const kind = store.kind;
  const lines: string[] = [`${store.name.trimEnd() || '(unnamed)'}  [${kind}, ${store.origin}]`];
  for (const block of blockNames(kind)) {
    lines.push('', blockTitle(block, kind));
    for (const p of blockInfo(store.catalog, kind, block).params) {
      // Name characters are shown in the heading; printing them as numbers would be noise.
      if (isNameCharacter(p)) continue;
      lines.push(`  ${p.name}: ${store.format(block, p.id)}`);
    }
  }
  return lines.join('\n') + '\n';
}
