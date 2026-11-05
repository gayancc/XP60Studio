// Moving through the patches of the bank that is open in the editor: the picker, the previous and next buttons and the bank list all go through here.

import type { PatchStore } from './store.svelte';
import { ui } from './ui.svelte';

/** Open patch `index` of the loaded bank in the editor. Returns false if there is no bank or no such patch. */
export function openBankSlot(store: PatchStore, index: number): boolean {
  const bank = ui.bank;
  const entry = bank?.names[index];
  if (!bank || !entry) return false;
  ui.bankSlot = index;
  store.loadSyx(bank.bytes, entry[0], bank.file);
  return true;
}

/** Step to the previous (-1) or next (+1) patch of the bank, staying inside it. */
export function stepBankSlot(store: PatchStore, delta: -1 | 1): boolean {
  const last = (ui.bank?.names.length ?? 0) - 1;
  const next = ui.bankSlot + delta;
  return next >= 0 && next <= last ? openBankSlot(store, next) : false;
}

export const canStepBank = (delta: -1 | 1): boolean => {
  const last = (ui.bank?.names.length ?? 0) - 1;
  const next = ui.bankSlot + delta;
  return next >= 0 && next <= last;
};
