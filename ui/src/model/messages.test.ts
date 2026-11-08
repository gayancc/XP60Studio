import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import {
  MAX_QUEUE,
  MESSAGE_LIFETIME,
  MIN_MESSAGE_MS,
  errorText,
  lifetimeFor,
  messages,
  reportError,
  reportInfo,
  reportSuccess,
  reportWarning,
} from './messages.svelte';

beforeEach(() => {
  vi.useFakeTimers();
  messages.clear();
});
afterEach(() => {
  messages.clear();
  vi.useRealTimers();
});

describe('the message centre', () => {
  it('queues each kind and trims the text; empty text shows nothing', () => {
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
    expect(messages.current?.text).toBe('Could not save');
    expect(messages.status).toBe('Could not save');
    expect(messages.queueSize).toBe(4);
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

  it('shows messages one by one and scales each lifetime by queue size', () => {
    reportError('First');
    reportWarning('Second');
    reportInfo('Third');
    expect(messages.current?.text).toBe('First');
    expect(messages.queueSize).toBe(3);

    const firstMs = lifetimeFor('error', 3);
    expect(firstMs).toBe(Math.max(MIN_MESSAGE_MS, Math.round(MESSAGE_LIFETIME.error / 3)));
    vi.advanceTimersByTime(firstMs - 1);
    expect(messages.current?.text).toBe('First');
    vi.advanceTimersByTime(1);
    expect(messages.current?.text).toBe('Second');
    expect(messages.queueSize).toBe(2);

    const secondMs = lifetimeFor('warning', 2);
    vi.advanceTimersByTime(secondMs);
    expect(messages.current?.text).toBe('Third');
    vi.advanceTimersByTime(lifetimeFor('info', 1));
    expect(messages.list).toHaveLength(0);
  });

  it('shortens the active message when more arrive in the queue', () => {
    reportWarning('Only');
    const alone = lifetimeFor('warning', 1);
    vi.advanceTimersByTime(alone / 2);
    reportWarning('Another');
    reportError('Urgent');
    expect(messages.queueSize).toBe(3);
    // Current was re-armed for the new queue size from this moment.
    vi.advanceTimersByTime(lifetimeFor('warning', 3) - 1);
    expect(messages.current?.text).toBe('Only');
    vi.advanceTimersByTime(1);
    expect(messages.current?.text).toBe('Another');
  });

  it('keeps an explicit display time when later messages join the queue', () => {
    messages.report('info', 'Copied', 5000);
    expect(messages.current?.durationMs).toBe(5000);
    reportWarning('Also');
    vi.advanceTimersByTime(4999);
    expect(messages.current?.text).toBe('Copied');
    vi.advanceTimersByTime(1);
    expect(messages.current?.text).toBe('Also');
  });

  it('pauses the clock while a hover covers the status line, then resumes the remainder', () => {
    reportError('Hidden while dragging');
    vi.advanceTimersByTime(2000);
    messages.pause();
    expect(messages.isPaused).toBe(true);
    vi.advanceTimersByTime(60_000);
    expect(messages.current?.text).toBe('Hidden while dragging');
    messages.resume();
    expect(messages.isPaused).toBe(false);
    const left = lifetimeFor('error', 1) - 2000;
    vi.advanceTimersByTime(left - 1);
    expect(messages.current?.text).toBe('Hidden while dragging');
    vi.advanceTimersByTime(1);
    expect(messages.list).toHaveLength(0);
  });

  it('keeps a held message until it is dismissed, then continues the queue', () => {
    const id = messages.hold('info', 'Move a controller to link it');
    reportError('Also failed');
    vi.advanceTimersByTime(10 * 60_000);
    expect(messages.current?.text).toBe('Move a controller to link it');
    expect(messages.queueSize).toBe(2);
    messages.dismiss(id);
    expect(messages.current?.text).toBe('Also failed');
    vi.advanceTimersByTime(lifetimeFor('error', 1));
    expect(messages.list).toHaveLength(0);
  });

  it('caps the queue without dropping a held head', () => {
    const id = messages.hold('info', 'Move a controller to link it');
    for (let i = 1; i <= MAX_QUEUE + 3; i++) reportWarning(`Warn ${i}`);
    expect(messages.current?.id).toBe(id);
    expect(messages.current?.text).toBe('Move a controller to link it');
    expect(messages.queueSize).toBe(MAX_QUEUE);
    expect(messages.list.some((m) => m.text === 'Warn 1')).toBe(false);
    messages.dismiss(id);
    expect(messages.current?.kind).toBe('warning');
  });

  it('ignores dismiss of an id that is no longer in the queue', () => {
    reportError('Alive');
    messages.dismiss(999);
    expect(messages.current?.text).toBe('Alive');
    vi.advanceTimersByTime(lifetimeFor('error', 1) - 1);
    expect(messages.current?.text).toBe('Alive');
  });

  it('advances past the current line on demand', () => {
    reportError('One');
    reportWarning('Two');
    messages.advance();
    expect(messages.current?.text).toBe('Two');
    messages.advance();
    expect(messages.list).toHaveLength(0);
  });

  it('turns a thrown value into text', () => {
    expect(errorText('plain')).toBe('Plain');
    expect(errorText(new Error('boom'))).toBe('Boom');
    expect(errorText(42)).toBe('42');
  });

  it('rewrites Windows MIDI jargon into plain English', () => {
    const text = errorText(
      'MIDI port error: could not create Windows MM MIDI input stream (MMSYSERR_ALLOCATED)',
    );
    expect(text.toLowerCase()).not.toContain('mmsyserr');
    expect(text).toContain('already open');
    expect(errorText('failed (os error 5)').toLowerCase()).not.toContain('os error');
  });

  it('never goes below the minimum display time', () => {
    expect(lifetimeFor('info', 100)).toBe(MIN_MESSAGE_MS);
    expect(MESSAGE_LIFETIME.error).toBeGreaterThan(MESSAGE_LIFETIME.warning);
    expect(MESSAGE_LIFETIME.warning).toBeGreaterThan(MESSAGE_LIFETIME.info);
  });
});
