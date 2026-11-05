import { getContext, setContext } from 'svelte';
import type { BankService } from '../model/bank.svelte';
import type { LibraryService } from '../model/library.svelte';
import type { SaveFlow } from '../model/save.svelte';
import type { SystemSaveFlow } from '../model/systemSave.svelte';

/** Services that exist only in the desktop app (the library and the save ceremony need Rust). */
export interface Services {
  library: LibraryService;
  save: SaveFlow;
  systemSave: SystemSaveFlow;
  bank: BankService;
}

const KEY = Symbol('services');

export function provideServices(services: Services | undefined): void {
  setContext(KEY, services);
}

export function useServices(): Services | undefined {
  return getContext<Services | undefined>(KEY);
}
