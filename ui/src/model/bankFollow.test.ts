import { describe, expect, it, vi } from 'vitest';
import { followBankPad } from './bankFollow';
import type { BankSlot } from './bank.svelte';

function slot(origin: BankSlot['origin'], bytes: Uint8Array | null = new Uint8Array([1])): BankSlot {
  return { bytes, name: 'Pad', origin };
}

describe('followBankPad', () => {
  it('does nothing on an empty pad before the keyboard bank has been synced into the app', async () => {
    const selectSound = vi.fn(async () => true);
    const auditionBlocks = vi.fn();
    const playPreviewNote = vi.fn(async () => {});
    const cancelPendingLive = vi.fn();
    await followBankPad(
      7,
      slot(null, null),
      {
        store: { auditionBlocks, whenQueueIdle: vi.fn(async () => 'idle' as const), cancelPendingLive },
        synth: { connected: true, selectSound, note: vi.fn(), allNotesOff: vi.fn() },
        playPreviewNote,
      },
      () => true,
    );
    expect(selectSound).not.toHaveBeenCalled();
    expect(auditionBlocks).not.toHaveBeenCalled();
    expect(playPreviewNote).not.toHaveBeenCalled();
    expect(cancelPendingLive).not.toHaveBeenCalled();
  });

  it('after synth sync: Program-Changes the User slot, auditions the synced bytes, then previews', async () => {
    const selectSound = vi.fn(async () => true);
    const whenQueueIdle = vi.fn(async () => 'idle' as const);
    const cancelPendingLive = vi.fn();
    const auditionBlocks = vi.fn();
    const bytes = new Uint8Array([2, 2, 2]);
    const playPreviewNote = vi.fn(async () => {});
    await followBankPad(
      4,
      slot('synth', bytes),
      {
        store: { auditionBlocks, whenQueueIdle, cancelPendingLive },
        synth: { connected: true, selectSound, note: vi.fn(), allNotesOff: vi.fn() },
        playPreviewNote,
        settleMs: 50,
      },
      () => true,
    );
    expect(cancelPendingLive).toHaveBeenCalledTimes(1);
    expect(selectSound).toHaveBeenCalledWith(0, 80, 0, 4);
    expect(auditionBlocks).toHaveBeenCalledWith(bytes);
    expect(whenQueueIdle).toHaveBeenCalledTimes(2);
    expect(playPreviewNote).toHaveBeenCalledTimes(1);
  });

  it('still auditions synced synth-pad bytes when an idle wait times out', async () => {
    const selectSound = vi.fn(async () => true);
    const auditionBlocks = vi.fn();
    const whenQueueIdle = vi.fn(async () => 'timeout' as const);
    const playPreviewNote = vi.fn(async () => {});
    const bytes = new Uint8Array([3, 3, 3]);
    await followBankPad(
      3,
      slot('synth', bytes),
      {
        store: { auditionBlocks, whenQueueIdle, cancelPendingLive: vi.fn() },
        synth: { connected: true, selectSound, note: vi.fn(), allNotesOff: vi.fn() },
        playPreviewNote,
        settleMs: 50,
      },
      () => true,
    );
    expect(selectSound).toHaveBeenCalledWith(0, 80, 0, 3);
    expect(auditionBlocks).toHaveBeenCalledWith(bytes);
    expect(playPreviewNote).toHaveBeenCalledTimes(1);
  });

  it('auditions a library pad to temporary memory without Program Change', async () => {
    const bytes = new Uint8Array([9, 9, 9]);
    const auditionBlocks = vi.fn();
    const whenQueueIdle = vi.fn(async () => 'idle' as const);
    const playPreviewNote = vi.fn(async () => {});
    const selectSound = vi.fn(async () => true);
    await followBankPad(
      2,
      slot('library', bytes),
      {
        store: { auditionBlocks, whenQueueIdle, cancelPendingLive: vi.fn() },
        synth: { connected: true, selectSound, note: vi.fn(), allNotesOff: vi.fn() },
        playPreviewNote,
        settleMs: 100,
      },
      () => true,
    );
    expect(auditionBlocks).toHaveBeenCalledWith(bytes);
    expect(selectSound).not.toHaveBeenCalled();
    expect(playPreviewNote).toHaveBeenCalledTimes(1);
  });

  it('skips work when a newer pad has already superseded this follow', async () => {
    const selectSound = vi.fn(async () => true);
    const auditionBlocks = vi.fn();
    await followBankPad(
      1,
      slot('synth', new Uint8Array([1])),
      {
        store: { auditionBlocks, whenQueueIdle: vi.fn(async () => 'idle' as const), cancelPendingLive: vi.fn() },
        synth: { connected: true, selectSound, note: vi.fn(), allNotesOff: vi.fn() },
        playPreviewNote: vi.fn(),
      },
      () => false,
    );
    expect(selectSound).not.toHaveBeenCalled();
    expect(auditionBlocks).not.toHaveBeenCalled();
  });
});
