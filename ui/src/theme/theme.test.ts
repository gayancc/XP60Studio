import { describe, expect, it } from 'vitest';
import { applyTheme, storedTheme, toggled } from './theme';

describe('theme', () => {
  it('defaults to dark and toggles', () => {
    expect(toggled('dark')).toBe('light');
    expect(toggled('light')).toBe('dark');
  });

  it('applies and remembers the choice', () => {
    const root = document.createElement('div');
    applyTheme('light', root);
    expect(root.dataset.theme).toBe('light');
    expect(storedTheme()).toBe('light');
    applyTheme('dark', root);
    expect(root.dataset.theme).toBeUndefined();
    expect(storedTheme()).toBe('dark');
  });
});
