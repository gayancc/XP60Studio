// Dark graphite is the default; a light theme is available. The choice is remembered when storage allows.

export type Theme = 'dark' | 'light';

const KEY = 'xppatch.theme';

export function storedTheme(): Theme {
  try {
    return globalThis.localStorage?.getItem(KEY) === 'light' ? 'light' : 'dark';
  } catch {
    return 'dark';
  }
}

export function applyTheme(theme: Theme, root: HTMLElement = document.documentElement): void {
  if (theme === 'light') root.dataset.theme = 'light';
  else delete root.dataset.theme;
  try {
    globalThis.localStorage?.setItem(KEY, theme);
  } catch {
    // The choice lasts for this session only.
  }
}

export function toggled(theme: Theme): Theme {
  return theme === 'dark' ? 'light' : 'dark';
}
