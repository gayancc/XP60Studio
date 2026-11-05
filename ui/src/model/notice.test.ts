import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { NOTICE_MS, SynthService, TauriTransport } from './synth.svelte';

beforeEach(() => vi.useFakeTimers());
afterEach(() => vi.useRealTimers());

describe('completion notices', () => {
  it('shows what finished, then clears it', () => {
    const s = new SynthService(new TauriTransport());
    s.say('Fetched "Init Tone" from the instrument.');
    expect(s.notice).toContain('Fetched');
    vi.advanceTimersByTime(NOTICE_MS - 1);
    expect(s.notice).not.toBe('');
    vi.advanceTimersByTime(2);
    expect(s.notice).toBe('');
  });

  it('a newer notice replaces an older one and restarts the time', () => {
    const s = new SynthService(new TauriTransport());
    s.say('first');
    vi.advanceTimersByTime(NOTICE_MS - 100);
    s.say('second');
    vi.advanceTimersByTime(200);
    expect(s.notice).toBe('second');
    vi.advanceTimersByTime(NOTICE_MS);
    expect(s.notice).toBe('');
  });
});
