import { describe, expect, it, vi } from 'vitest';
import { focusToneEditor } from './focusTone';

describe('focusToneEditor', () => {
  it('scrolls the matching multi-tone editor into view', async () => {
    const root = document.createElement('div');
    const a = document.createElement('section');
    a.setAttribute('data-tone-focus', '0');
    const b = document.createElement('section');
    b.setAttribute('data-tone-focus', '2');
    b.scrollIntoView = vi.fn();
    root.append(a, b);
    await focusToneEditor(root, 2);
    expect(b.scrollIntoView).toHaveBeenCalledWith({ block: 'nearest', inline: 'nearest', behavior: 'smooth' });
  });

  it('does nothing when the tone editor is missing or scroll is unavailable', async () => {
    await expect(focusToneEditor(document.createElement('div'), 1)).resolves.toBeUndefined();
    await expect(focusToneEditor(null, 0)).resolves.toBeUndefined();
  });

  it('skips scroll when the selection was superseded', async () => {
    const root = document.createElement('div');
    const el = document.createElement('section');
    el.setAttribute('data-tone-focus', '1');
    el.scrollIntoView = vi.fn();
    root.append(el);
    const signal = { cancelled: true };
    await focusToneEditor(root, 1, signal);
    expect(el.scrollIntoView).not.toHaveBeenCalled();
  });
});
