import { cleanup, fireEvent, render, screen, waitFor } from '@testing-library/svelte';
import { afterEach, beforeAll, describe, expect, it, vi } from 'vitest';
import { readFileSync } from 'node:fs';
import { catalogJson, initSync } from '../wasm/pkg/xppatch.js';
import type { Backend, LibraryRow } from '../model/backend';
import { parseCatalog } from '../model/catalog';
import { LibraryService } from '../model/library.svelte';
import { PatchStore } from '../model/store.svelte';
import { MemoryTransport } from '../model/transport';
import LibraryHarness from './LibraryHarness.svelte';
import type { Services } from './services';

afterEach(cleanup);

const row = (id: number, name: string, tags: string[]): LibraryRow => ({ id, name, tags, favourite: false, source: 't', added_at: 1 });
const TAGS: Array<[string, number]> = [['Bass', 1], ['Pads/Bright', 1], ['Pads/Warm', 2], ['Pads/Warm/Dark', 1]];

beforeAll(() => initSync({ module: readFileSync('src/wasm/pkg/xppatch_bg.wasm') }));

function mount(): { backend: Backend; lib: LibraryService } {
  const backend = {
    libraryList: vi.fn(async () => [row(1, 'Alpha', ['Pads/Warm'])]),
    libraryCount: vi.fn(async () => 1),
    libraryTags: vi.fn(async () => TAGS),
  } as unknown as Backend;
  const lib = new LibraryService(backend);
  const store = new PatchStore(parseCatalog(catalogJson()), new MemoryTransport());
  render(LibraryHarness, { props: { services: { library: lib } as unknown as Services, store } });
  return { backend, lib };
}

const lastTag = (b: Backend): unknown => {
  const calls = (b.libraryList as ReturnType<typeof vi.fn>).mock.calls;
  return (calls.at(-1)?.[0] as { tag?: string } | undefined)?.tag;
};

describe('library folders', () => {
  it('shows the top-level folders as tabs, not every tag', async () => {
    mount();
    await screen.findByRole('tab', { name: 'Pads' });
    expect(screen.getByRole('tab', { name: 'Bass' })).toBeTruthy();
    expect(screen.queryByRole('tab', { name: 'Pads/Warm' })).toBeNull();
    expect(screen.queryByRole('tab', { name: 'Warm' })).toBeNull();
  });

  it('opening a folder filters by its path and offers the folders inside it', async () => {
    const { backend } = mount();
    await fireEvent.click(await screen.findByRole('tab', { name: 'Pads' }));
    await waitFor(() => expect(lastTag(backend)).toBe('Pads'));
    const row = await screen.findByRole('group', { name: 'Folders in Pads' });
    expect(row.textContent).toContain('Bright');
    expect(row.textContent).toContain('Warm');
  });

  it('going into a sub-folder narrows the filter, and a deeper folder can be reached', async () => {
    const { backend } = mount();
    await fireEvent.click(await screen.findByRole('tab', { name: 'Pads' }));
    await fireEvent.click(await screen.findByRole('button', { name: /^Warm/ }));
    await waitFor(() => expect(lastTag(backend)).toBe('Pads/Warm'));
    const row = await screen.findByRole('group', { name: 'Folders in Warm' });
    expect(row.textContent).toContain('Dark');
    expect(row.textContent).toContain('Pads'); // the way back up is shown
    await fireEvent.click(screen.getByRole('button', { name: 'Dark' }));
    await waitFor(() => expect(lastTag(backend)).toBe('Pads/Warm/Dark'));
  });

  it('the trail goes back up a level, and the All tab clears the folder', async () => {
    const { backend } = mount();
    await fireEvent.click(await screen.findByRole('tab', { name: 'Pads' }));
    await fireEvent.click(await screen.findByRole('button', { name: /^Warm/ }));
    const group = await screen.findByRole('group', { name: 'Folders in Warm' });
    const up = [...group.querySelectorAll('button')].find((b) => b.textContent?.trim() === 'Pads');
    expect(up).toBeDefined();
    await fireEvent.click(up as HTMLElement);
    await waitFor(() => expect(lastTag(backend)).toBe('Pads'));
    await fireEvent.click(screen.getByRole('tab', { name: 'All' }));
    await waitFor(() => expect(lastTag(backend)).toBe(''));
  });

  it('a folder with nothing inside it shows no folder row', async () => {
    mount();
    await fireEvent.click(await screen.findByRole('tab', { name: 'Bass' }));
    await waitFor(() => expect(screen.getByRole('tab', { name: 'Bass', selected: true })).toBeTruthy());
    expect(screen.queryByRole('group', { name: /^Folders in/ })).toBeNull();
  });
});
