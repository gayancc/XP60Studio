import { messages } from '../model/messages.svelte';
import { cleanup, fireEvent, render, screen, waitFor } from '@testing-library/svelte';
import { afterEach, describe, expect, it, vi } from 'vitest';
import type { Backend, StorageInfo } from '../model/backend';
import StoragePanel from './StoragePanel.svelte';

afterEach(cleanup);

const base: StorageInfo = { in_use: 'C:/data', configured: 'C:/data', default: 'C:/data', custom: false };

function backend(info: StorageInfo, set?: (dir: string | null) => Promise<StorageInfo>): Backend {
  return { storageInfo: vi.fn(async () => info), storageSet: vi.fn(set ?? (async () => info)) } as unknown as Backend;
}

describe('storage location panel', () => {
  it('shows where the library is and offers no reset while the default is in use', async () => {
    render(StoragePanel, { props: { backend: backend(base) } });
    await screen.findByText('C:/data');
    expect((screen.getByRole('button', { name: 'Use the default' }) as HTMLButtonElement).disabled).toBe(true);
    expect((screen.getByRole('button', { name: 'Use this folder' }) as HTMLButtonElement).disabled).toBe(true);
  });

  it('sends the typed folder, and says when it will apply', async () => {
    messages.clear();
    const after: StorageInfo = { ...base, configured: 'D:/xp', custom: true };
    const b = backend(base, async () => after);
    render(StoragePanel, { props: { backend: b } });
    const box = (await screen.findByLabelText('Folder for the library and backups')) as HTMLInputElement;
    await fireEvent.input(box, { target: { value: 'D:/xp' } });
    await fireEvent.click(screen.getByRole('button', { name: 'Use this folder' }));
    await waitFor(() => expect(b.storageSet).toHaveBeenCalledWith('D:/xp'));
    await screen.findByText('Applies when the app is next started.');
    expect(messages.list.map((m) => m.text).join(' | ')).toContain('nothing was removed');
    expect(screen.queryByRole('status')).toBeNull();
  });

  it('puts a refusal in front of the user and keeps the old setting', async () => {
    messages.clear();
    const b = backend(base, async () => {
      throw 'Give the full path of the folder, for example D:/XP60 data.';
    });
    render(StoragePanel, { props: { backend: b } });
    const box = (await screen.findByLabelText('Folder for the library and backups')) as HTMLInputElement;
    await fireEvent.input(box, { target: { value: 'relative' } });
    await fireEvent.click(screen.getByRole('button', { name: 'Use this folder' }));
    await waitFor(() => expect(messages.list.map((m) => m.text).join(' | ')).toContain('full path'));
    expect(messages.list.at(-1)?.kind).toBe('error');
    expect(screen.queryByRole('alert')).toBeNull();
  });

  it('can go back to the default folder', async () => {
    const custom: StorageInfo = { ...base, configured: 'D:/xp', custom: true };
    const b = backend(custom, async () => base);
    render(StoragePanel, { props: { backend: b } });
    await screen.findByDisplayValue('D:/xp');
    await fireEvent.click(screen.getByRole('button', { name: 'Use the default' }));
    await waitFor(() => expect(b.storageSet).toHaveBeenCalledWith(null));
  });
});
