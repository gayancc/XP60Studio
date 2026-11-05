import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { MAX_VISIBLE, MESSAGE_LIFETIME, errorText, messages, reportError, reportInfo, reportSuccess, reportWarning } from './messages.svelte';

beforeEach(() => {
  vi.useFakeTimers();
  messages.clear();
});
afterEach(() => {
  messages.clear();
  vi.useRealTimers();
});

describe('the message centre', () => {
  it('shows each kind and trims the text; empty text shows nothing', () => {
    reportError('  Could not save  ');
    reportWarning('Careful');
    reportInfo('Copied');
    reportSuccess('Saved');
    expect(messages.list.map((m) => [m.kind, m.text])).toEqual([
      ['error', 'Could not save'],
      ['warning', 'Careful'],
      ['info', 'Copied'],
      ['success', 'Saved'],
    ]);
    expect(messages.report('info', '   ')).toBe(0);
    expect(messages.list).toHaveLength(4);
  });

  it('renews an identical message instead of stacking a copy', () => {
    const a = reportError('Not connected');
    const b = reportError('Not connected');
    expect(b).toBe(a);
    expect(messages.list).toHaveLength(1);
    expect(messages.list[0]?.count).toBe(2);
    reportWarning('Not connected'); // another kind is another message
    expect(messages.list).toHaveLength(2);
  });

  it('removes a message after its lifetime, errors last longest, and renewing restarts the clock', () => {
    reportInfo('Copied');
    reportError('Failed');
    vi.advanceTimersByTime(MESSAGE_LIFETIME.info + 1);
    expect(messages.list.map((m) => m.text)).toEqual(['Failed']);
    vi.advanceTimersByTime(MESSAGE_LIFETIME.error - MESSAGE_LIFETIME.info - 100);
    reportError('Failed');
    vi.advanceTimersByTime(MESSAGE_LIFETIME.error - 100);
    expect(messages.list).toHaveLength(1);
    vi.advanceTimersByTime(200);
    expect(messages.list).toHaveLength(0);
    expect(MESSAGE_LIFETIME.error).toBeGreaterThan(MESSAGE_LIFETIME.warning);
    expect(MESSAGE_LIFETIME.warning).toBeGreaterThan(MESSAGE_LIFETIME.info);
  });

  it('keeps a held message until it is dismissed', () => {
    const id = messages.hold('info', 'Move a controller to link it');
    vi.advanceTimersByTime(10 * 60_000);
    expect(messages.list).toHaveLength(1);
    messages.dismiss(id);
    expect(messages.list).toHaveLength(0);
  });

  it('shows at most four at once, dropping the oldest', () => {
    for (let i = 1; i <= MAX_VISIBLE + 2; i++) reportInfo(`Message ${i}`);
    expect(messages.list).toHaveLength(MAX_VISIBLE);
    expect(messages.list[0]?.text).toBe('Message 3');
  });

  it('turns a thrown value into text', () => {
    expect(errorText('plain')).toBe('plain');
    expect(errorText(new Error('boom'))).toBe('boom');
    expect(errorText(42)).toBe('42');
  });
});
