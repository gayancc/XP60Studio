import { afterEach, describe, expect, it, vi } from 'vitest';
import { cleanup, fireEvent, render } from '@testing-library/svelte';
import MenuBar from './MenuBar.svelte';

afterEach(cleanup);

const menus = (run = vi.fn()) => [
  { title: 'File', items: [{ label: 'New', run }, { separator: true }, { label: 'Open', run }] },
  { title: 'Edit', items: [{ label: 'Undo', hint: 'Ctrl+Z', run }, { label: 'Redo', disabled: true }] },
];

describe('the menu bar', () => {
  it('opens a menu placed against its title by coordinates (fixed to the window; the real window is checked by the end-to-end run)', async () => {
    const { getByRole, getByText } = render(MenuBar, { props: { menus: menus() } });
    await fireEvent.click(getByRole('button', { name: 'Edit' }));
    const list = getByRole('menu', { name: 'Edit' });
    expect(list.style.left).toMatch(/px$/);
    expect(list.style.top).toMatch(/px$/);
    expect(getByText('Undo')).toBeTruthy();
  });

  it('runs an item and closes; a disabled item does nothing', async () => {
    const run = vi.fn();
    const { getByRole, queryByRole } = render(MenuBar, { props: { menus: menus(run) } });
    await fireEvent.click(getByRole('button', { name: 'Edit' }));
    await fireEvent.click(getByRole('menuitem', { name: /Redo/ }));
    expect(run).not.toHaveBeenCalled();
    await fireEvent.click(getByRole('menuitem', { name: /Undo/ }));
    expect(run).toHaveBeenCalledTimes(1);
    expect(queryByRole('menu')).toBeNull();
  });

  it('moves to the next menu when the pointer slides across the bar, and closes on Escape', async () => {
    const { getByRole, queryByRole } = render(MenuBar, { props: { menus: menus() } });
    await fireEvent.click(getByRole('button', { name: 'File' }));
    await fireEvent.pointerEnter(getByRole('button', { name: 'Edit' }));
    expect(getByRole('menu', { name: 'Edit' })).toBeTruthy();
    await fireEvent.keyDown(window, { key: 'Escape' });
    expect(queryByRole('menu')).toBeNull();
  });
});
