import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import { cleanup, fireEvent, render } from '@testing-library/svelte';
import { messages, reportError, reportInfo, reportSuccess, reportWarning } from '../model/messages.svelte';
import MessageStack from './MessageStack.svelte';

beforeEach(() => messages.clear());
afterEach(() => {
  cleanup();
  messages.clear();
});

describe('the message stack', () => {
  it('shows every message with its kind, errors as alerts and the rest as polite status', async () => {
    const { container } = render(MessageStack);
    reportError('Could not save');
    reportWarning('Careful');
    reportInfo('Copied');
    reportSuccess('Saved');
    await Promise.resolve();
    const rows = [...container.querySelectorAll('.msg')];
    expect(rows.map((r) => r.getAttribute('data-kind'))).toEqual(['error', 'warning', 'info', 'success']);
    expect(rows.map((r) => r.getAttribute('role'))).toEqual(['alert', 'status', 'status', 'status']);
    expect(rows[0]?.textContent).toContain('Error');
    expect(rows[0]?.textContent).toContain('Could not save');
    expect(rows[3]?.textContent).toContain('Done');
  });

  it('dismisses a message with its button', async () => {
    const { container, getAllByRole } = render(MessageStack);
    reportInfo('One');
    reportInfo('Two');
    await Promise.resolve();
    await fireEvent.click(getAllByRole('button', { name: 'Dismiss message' })[0] as HTMLElement);
    expect([...container.querySelectorAll('.text')].map((e) => e.textContent)).toEqual(['Two']);
  });

  it('shows nothing when there is nothing to say', () => {
    const { container } = render(MessageStack);
    expect(container.querySelectorAll('.msg')).toHaveLength(0);
  });
});
