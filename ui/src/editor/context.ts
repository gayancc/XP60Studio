import { getContext, setContext } from 'svelte';
import type { PatchStore } from '../model/store.svelte';

const KEY = Symbol('patch-store');

export function provideStore(store: PatchStore): void {
  setContext(KEY, store);
}

export function useStore(): PatchStore {
  const store = getContext<PatchStore | undefined>(KEY);
  if (!store) throw new Error('PatchStore was not provided');
  return store;
}
