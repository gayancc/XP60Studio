import { afterEach, describe, expect, it } from 'vitest';
import { installDesktopChrome, isBrowserShortcut } from './desktopChrome';

let off: (() => void) | undefined;
afterEach(() => {
  off?.();
  off = undefined;
  document.body.innerHTML = '';
});

const fire = (el: Element, type: string, init: EventInit = {}): boolean => el.dispatchEvent(new MouseEvent(type, { bubbles: true, cancelable: true, ...init }));

describe('desktop behaviour', () => {
  it('removes the browser right-click menu from the page', () => {
    off = installDesktopChrome();
    document.body.innerHTML = '<div id="a">area</div><button id="b">button</button>';
    expect(fire(document.getElementById('a')!, 'contextmenu')).toBe(false); // default prevented
    expect(fire(document.getElementById('b')!, 'contextmenu')).toBe(false);
  });

  it('keeps the ordinary edit menu in text fields', () => {
    off = installDesktopChrome();
    document.body.innerHTML = '<input id="t" type="text"><textarea id="x"></textarea><input id="r" type="range"><select id="s"></select>';
    expect(fire(document.getElementById('t')!, 'contextmenu')).toBe(true);
    expect(fire(document.getElementById('x')!, 'contextmenu')).toBe(true);
    expect(fire(document.getElementById('r')!, 'contextmenu')).toBe(false);
    expect(fire(document.getElementById('s')!, 'contextmenu')).toBe(false);
  });

  it('leaves the app’s own right-click menus alone: it never stops the event reaching them', () => {
    off = installDesktopChrome();
    document.body.innerHTML = '<div id="knob"></div>';
    let reached = 0;
    document.getElementById('knob')!.addEventListener('contextmenu', () => (reached += 1));
    fire(document.getElementById('knob')!, 'contextmenu');
    expect(reached).toBe(1);
  });

  it('recognises reload, print, save and view-source shortcuts, and nothing the app uses', () => {
    const key = (k: string, mods: Partial<KeyboardEvent> = {}) => isBrowserShortcut({ key: k, ctrlKey: false, metaKey: false, shiftKey: false, altKey: false, ...mods });
    expect(key('F5')).toBe(true);
    expect(key('r', { ctrlKey: true })).toBe(true);
    expect(key('R', { ctrlKey: true, shiftKey: true })).toBe(true);
    expect(key('p', { metaKey: true })).toBe(true);
    expect(key('u', { ctrlKey: true })).toBe(true);
    expect(key('z', { ctrlKey: true })).toBe(false); // undo
    expect(key('c', { ctrlKey: true })).toBe(false); // copy
    expect(key('k', { ctrlKey: true })).toBe(false); // the command palette
    expect(key('r')).toBe(false);
  });

  it('stops the browser acting on a shortcut but still lets the app see the key (Ctrl+R is Randomize, F5 opens the Library)', () => {
    off = installDesktopChrome();
    let seen = 0;
    const app = (): void => void (seen += 1);
    window.addEventListener('keydown', app);
    const ev = new KeyboardEvent('keydown', { key: 'F5', bubbles: true, cancelable: true });
    window.dispatchEvent(ev);
    window.removeEventListener('keydown', app);
    expect(ev.defaultPrevented).toBe(true);
    expect(seen).toBe(1);
  });

  it('blocks ctrl-wheel page zoom but not the plain wheel', () => {
    off = installDesktopChrome();
    document.body.innerHTML = '<div id="a"></div>';
    const a = document.getElementById('a')!;
    expect(a.dispatchEvent(new WheelEvent('wheel', { bubbles: true, cancelable: true, ctrlKey: true }))).toBe(false);
    expect(a.dispatchEvent(new WheelEvent('wheel', { bubbles: true, cancelable: true }))).toBe(true);
  });

  it('can be removed again', () => {
    const remove = installDesktopChrome();
    remove();
    document.body.innerHTML = '<div id="a"></div>';
    expect(fire(document.getElementById('a')!, 'contextmenu')).toBe(true);
  });
});
