// What the sound-design buttons, the command palette and the shortcuts do. One place, so they cannot drift apart.

import { PART_BLOCKS, RHYTHM_NOTE_BLOCKS, SCALE_TUNE_BLOCKS, TONE_BLOCKS, type BlockName, type DocKindName } from '../model/catalog';
import { blockNames } from '../model/kinds';
import { blockTitle } from '../model/labels';
import type { PatchStore } from '../model/store.svelte';
import { copyBlock, mutate, pasteClip, randomize, resetChanges } from '../model/tools';
import { ui } from '../model/ui.svelte';

/** The Tone, Part, Key or Scale Tune currently selected in the workspace. */
export function selectedBlock(kind: DocKindName): BlockName {
  switch (kind) {
    case 'patch':
      return TONE_BLOCKS[ui.tone] ?? 'tone1';
    case 'performance':
      return PART_BLOCKS[ui.part] ?? 'part1';
    case 'rhythm':
      return RHYTHM_NOTE_BLOCKS[ui.key] ?? 'note35';
    case 'system':
      return SCALE_TUNE_BLOCKS[ui.tune] ?? 'tune17';
  }
}

function nextSeed(): number {
  ui.tools.seed = (ui.tools.seed + 0x9e3779b9) >>> 0;
  return ui.tools.seed;
}

const percent = (x: number): string => `${Math.round(x * 100)}%`;

export function runInit(store: PatchStore): void {
  // Every value is written out, including those whose neutral setting is the bottom of the range: a sound being initialised is not a blank document, so a
  // value left out would keep whatever the sound had (an EFX type, a Tone switched on).
  store.applyMany(blockNames(store.kind).flatMap((b) => resetChanges(store.catalog, store.kind, b)), 'Init');
}

/** Put one block (a Tone, Part or Key), or only some of its parameters, back at neutral. One undo step. */
export function resetBlock(store: PatchStore, block: BlockName, label: string, ids?: readonly string[]): void {
  store.applyMany(resetChanges(store.catalog, store.kind, block, ids), `Reset ${label}`);
}

export function runRandomize(store: PatchStore): void {
  const scope = ui.tools.allBlocks ? 'all' : selectedBlock(store.kind);
  const changes = randomize(store.catalog, store.kind, scope, (b, id) => store.get(b, id), ui.tools.strength, nextSeed());
  store.applyMany(changes, `Randomize ${percent(ui.tools.strength)} (${scope === 'all' ? 'everything' : blockTitle(scope, store.kind)})`);
}

export function runMutate(store: PatchStore): void {
  const scope = ui.tools.allBlocks ? 'all' : selectedBlock(store.kind);
  const changes = mutate(store.catalog, store.kind, scope, (b, id) => store.get(b, id), ui.tools.amount, nextSeed());
  store.applyMany(changes, `Mutate ${percent(ui.tools.amount)} (${scope === 'all' ? 'everything' : blockTitle(scope, store.kind)})`);
}

/** Copy the selected block, or only `ids` of it (one section). */
export function copySelected(store: PatchStore, label?: string, ids?: readonly string[]): void {
  const block = selectedBlock(store.kind);
  ui.clip = copyBlock(
    store.catalog,
    store.kind,
    block,
    (b, id) => store.get(b, id),
    label ?? blockTitle(block, store.kind),
    ids,
  );
}

/** Paste the clip into the selected block. Returns how many parameters changed. */
export function pasteSelected(store: PatchStore): number {
  const clip = ui.clip;
  if (!clip) return 0;
  const block = selectedBlock(store.kind);
  const changes = pasteClip(store.catalog, store.kind, clip, block, (b, id) => store.get(b, id));
  store.applyMany(changes, `Paste ${clip.label} into ${blockTitle(block, store.kind)}`);
  return changes.length;
}

/** Show a parameter: select its Tone, Part, Key or tune block, then scroll to it and focus it. */
export function jumpToParam(kind: DocKindName, block: BlockName, id: string): void {
  if (kind === 'patch') {
    const i = TONE_BLOCKS.indexOf(block);
    if (i >= 0) ui.tone = i;
  } else if (kind === 'performance') {
    const i = PART_BLOCKS.indexOf(block);
    if (i >= 0) ui.part = i;
  } else if (kind === 'rhythm') {
    const i = RHYTHM_NOTE_BLOCKS.indexOf(block);
    if (i >= 0) ui.key = i;
  } else {
    const i = SCALE_TUNE_BLOCKS.indexOf(block);
    if (i >= 0) ui.tune = i;
  }
  // Give the selected block a moment to render before looking for the control.
  setTimeout(() => {
    const host = document.querySelector<HTMLElement>(`[data-param="${CSS.escape(`${block}:${id}`)}"]`);
    host?.scrollIntoView({ block: 'center', behavior: 'smooth' });
    host?.querySelector<HTMLElement>('[tabindex="0"], button, select, input')?.focus();
  }, 80);
}
