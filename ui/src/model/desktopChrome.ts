// A desktop application does not behave like a web page. Take away what only a browser offers: its right-click menu (Back, Reload,
// View source, Inspect...), reload, print and view-source shortcuts, and pinch or ctrl-wheel page zoom. The app's own right-click
// menus (on the knobs and sliders) are unaffected, and a text field keeps the ordinary Cut/Copy/Paste menu.

const EDITABLE = 'input, textarea, select, [contenteditable=""], [contenteditable="true"]';

/** True where the system's edit menu is useful: a text box, not a button or a knob. */
function isTextEntry(target: EventTarget | null): boolean {
  if (!(target instanceof Element)) return false;
  const el = target.closest(EDITABLE);
  if (!el) return false;
  if (el instanceof HTMLInputElement) return !['button', 'checkbox', 'radio', 'range', 'submit', 'reset', 'file', 'color'].includes(el.type);
  if (el instanceof HTMLSelectElement) return false;
  return true;
}

/** The browser shortcuts a desktop app has no use for: reload, print, view source. */
export function isBrowserShortcut(e: Pick<KeyboardEvent, 'key' | 'ctrlKey' | 'metaKey' | 'shiftKey' | 'altKey'>): boolean {
  const mod = e.ctrlKey || e.metaKey;
  const k = e.key.toLowerCase();
  if (e.key === 'F5') return true;
  if (mod && !e.altKey && (k === 'r' || k === 'p' || k === 'u' || k === 's' || k === 'f5')) return true;
  return false;
}

/** Install the desktop behaviour on a window. Returns a function that removes it again. */
export function installDesktopChrome(win: Window = window): () => void {
  const onContext = (e: Event): void => {
    if (!isTextEntry(e.target)) e.preventDefault();
  };
  const onKey = (e: KeyboardEvent): void => {
    if (isBrowserShortcut(e)) e.preventDefault();
  };
  const onWheel = (e: WheelEvent): void => {
    if (e.ctrlKey) e.preventDefault(); // page zoom; the knobs use plain wheel
  };
  win.addEventListener('contextmenu', onContext);
  win.addEventListener('keydown', onKey);
  win.addEventListener('wheel', onWheel, { passive: false });
  return () => {
    win.removeEventListener('contextmenu', onContext);
    win.removeEventListener('keydown', onKey);
    win.removeEventListener('wheel', onWheel);
  };
}
