import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { DWELL_MS, LEAVE_MS, PreviewService, type PreviewDeps } from './preview.svelte';

const own = new Uint8Array([1, 1, 1]);
const stored = (id: number): Uint8Array => new Uint8Array([id, id]);

function setup(over: Partial<PreviewDeps> = {}) {
  const sent: Uint8Array[] = [];
  const state = { connected: true };
  const deps: PreviewDeps = {
    connected: () => state.connected,
    readTemp: vi.fn(async () => own as Uint8Array | null),
    readStored: vi.fn(async (id: number) => stored(id) as Uint8Array | null),
    sendTemp: vi.fn(async (b: Uint8Array) => {
      sent.push(b);
    }),
    markStale: vi.fn(),
    ...over,
  };
  const p = new PreviewService();
  p.attach(deps);
  p.setEnabled(true);
  return { p, deps, sent, state };
}

beforeEach(() => vi.useFakeTimers());
afterEach(() => vi.useRealTimers());

describe('hover audition', () => {
  it('sends nothing until the pointer has rested on a row', async () => {
    const { p, sent } = setup();
    p.hover(5);
    await vi.advanceTimersByTimeAsync(DWELL_MS - 50);
    expect(sent).toHaveLength(0);
    await vi.advanceTimersByTimeAsync(100);
    expect(sent).toEqual([stored(5)]);
    expect(p.playing).toBe(5);
  });

  it('sweeping across rows sends nothing for the rows passed over', async () => {
    const { p, sent } = setup();
    for (const id of [1, 2, 3, 4]) {
      p.hover(id);
      await vi.advanceTimersByTimeAsync(100);
    }
    await vi.advanceTimersByTimeAsync(DWELL_MS);
    expect(sent).toEqual([stored(4)]);
  });

  it("puts back the instrument's own sound when the pointer leaves", async () => {
    const { p, sent, deps } = setup();
    p.hover(7);
    await vi.advanceTimersByTimeAsync(DWELL_MS + 10);
    p.unhover();
    await vi.advanceTimersByTimeAsync(LEAVE_MS + 10);
    expect(sent).toEqual([stored(7), own]);
    expect(p.playing).toBeNull();
    expect(deps.readTemp).toHaveBeenCalledTimes(1);
    expect(deps.markStale).not.toHaveBeenCalled();
  });

  it('moving from row to row keeps the original and restores it once, not the previewed row', async () => {
    const { p, sent, deps } = setup();
    p.hover(1);
    await vi.advanceTimersByTimeAsync(DWELL_MS + 10);
    p.unhover();
    await vi.advanceTimersByTimeAsync(LEAVE_MS - 100);
    p.hover(2); // entered the next row before the restore fired
    await vi.advanceTimersByTimeAsync(DWELL_MS + 10);
    expect(sent).toEqual([stored(1), stored(2)]);
    expect(deps.readTemp).toHaveBeenCalledTimes(1);
    await p.stop();
    expect(sent.at(-1)).toEqual(own);
    expect(sent).toHaveLength(3);
  });

  it('does nothing while switched off or not connected', async () => {
    const { p, sent, state } = setup();
    p.setEnabled(false);
    p.hover(1);
    await vi.advanceTimersByTimeAsync(DWELL_MS * 2);
    expect(sent).toHaveLength(0);
    p.setEnabled(true);
    state.connected = false;
    p.hover(1);
    await vi.advanceTimersByTimeAsync(DWELL_MS * 2);
    expect(sent).toHaveLength(0);
  });

  it("never sends a preview when the instrument's current sound could not be read first", async () => {
    const { p, sent } = setup({ readTemp: vi.fn(async () => null) });
    p.hover(3);
    await vi.advanceTimersByTimeAsync(DWELL_MS + 10);
    expect(sent).toHaveLength(0);
    expect(p.error).toContain('could not be read');
    expect(p.playing).toBeNull();
  });

  it('says so and marks the sound stale when the original cannot be put back', async () => {
    let fail = false;
    const { p, deps } = setup({
      sendTemp: vi.fn(async () => {
        if (fail) throw new Error('no reply');
      }),
    });
    p.hover(9);
    await vi.advanceTimersByTimeAsync(DWELL_MS + 10);
    fail = true;
    await p.stop();
    expect(deps.markStale).toHaveBeenCalled();
    expect(p.error).toContain('could not be restored');
    expect(p.playing).toBeNull();
  });

  it('switching the feature off while a preview sounds restores straight away', async () => {
    const { p, sent } = setup();
    p.hover(4);
    await vi.advanceTimersByTimeAsync(DWELL_MS + 10);
    p.setEnabled(false);
    await vi.advanceTimersByTimeAsync(10);
    expect(sent).toEqual([stored(4), own]);
  });

  it('a stored patch that cannot be read leaves the instrument alone', async () => {
    const { p, sent } = setup({ readStored: vi.fn(async () => null) });
    p.hover(1);
    await vi.advanceTimersByTimeAsync(DWELL_MS + 10);
    expect(sent).toHaveLength(0);
    await p.stop();
    // The original was read but never replaced, so putting it back is at worst a harmless repeat.
    expect(sent.length).toBeLessThanOrEqual(1);
  });
});
