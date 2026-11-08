import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import { cleanup, fireEvent, render } from '@testing-library/svelte';
import { clearHint, showHint } from './hint.svelte';
import { messages, reportError, reportInfo, reportSuccess, reportWarning } from '../model/messages.svelte';
import HintBar from './HintBar.svelte';

beforeEach(() => {
  messages.clear();
  clearHint();
});
afterEach(() => {
  cleanup();
  messages.clear();
  clearHint();
});

describe('the hint bar', () => {
  it('shows every kind in the status line, one at a time, with a queue count', async () => {
    const { container } = render(HintBar);
    reportError('not connected to an instrument');
    reportWarning('Careful');
    reportSuccess('Saved');
    await Promise.resolve();
    const row = container.querySelector('.status-msg');
    expect(row?.getAttribute('data-kind')).toBe('error');
    expect(row?.getAttribute('role')).toBe('alert');
    expect(row?.textContent).toContain('Error');
    expect(row?.textContent).toContain('Not connected to an instrument.');
    expect(row?.textContent).toContain('+2');
    expect(container.querySelectorAll('.status-msg')).toHaveLength(1);
  });

  it('replaces the status line when the next queued message becomes current', async () => {
    const { container } = render(HintBar);
    reportInfo('Verified: the instrument\'s temporary copy matches this sound.');
    await Promise.resolve();
    expect(container.querySelector('.msg-text')?.textContent).toBe(
      "Verified: the instrument's temporary copy matches this sound.",
    );
    messages.advance();
    reportInfo('Sent the open sound to the instrument (Live edit is on).');
    await Promise.resolve();
    expect(container.querySelector('.msg-text')?.textContent).toBe(
      'Sent the open sound to the instrument (Live edit is on).',
    );
    expect(container.querySelectorAll('.status-msg')).toHaveLength(1);
  });

  it('keeps a hovered control ahead of the status message and pauses its clock', async () => {
    const { container } = render(HintBar);
    reportWarning('System settings live in permanent memory.');
    showHint({ name: 'Cutoff', value: '64', raw: '64' });
    await Promise.resolve();
    expect(container.querySelector('.status-msg')).toBeNull();
    expect(container.textContent).toContain('Cutoff');
    expect(container.textContent).toContain('64');
    expect(container.textContent).toContain('Warning waiting');
    expect(messages.isPaused).toBe(true);
    clearHint();
    await Promise.resolve();
    expect(messages.isPaused).toBe(false);
    expect(container.querySelector('.msg-text')?.textContent).toBe('System settings live in permanent memory.');
  });

  it('dismisses the current message from the bar and shows the next', async () => {
    const { container, getByRole } = render(HintBar);
    reportError('One');
    reportWarning('Two');
    await Promise.resolve();
    await fireEvent.click(getByRole('button', { name: 'Dismiss message' }));
    expect(container.querySelector('.msg-text')?.textContent).toBe('Two');
    expect(container.querySelector('.more')).toBeNull();
  });

  it('ignores a second rapid click after the current message has gone', async () => {
    const { getByRole } = render(HintBar);
    reportInfo('One');
    await Promise.resolve();
    const dismiss = getByRole('button', { name: 'Dismiss message' });
    expect(() => { dismiss.click(); dismiss.click(); }).not.toThrow();
    expect(messages.list).toHaveLength(0);
  });
});
