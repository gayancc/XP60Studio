// One context menu for the whole app. Controls open it with `openMenu`; `ContextMenu.svelte` (mounted once) renders it.

export interface MenuItem {
  label: string;
  action?: () => void;
  disabled?: boolean;
  /** Renders a text box instead of a plain row; Enter submits. */
  input?: { initial: string; onSubmit: (text: string) => void };
  separator?: boolean;
}

export const menu: { open: boolean; x: number; y: number; items: MenuItem[] } = $state({
  open: false,
  x: 0,
  y: 0,
  items: [],
});

export function openMenu(x: number, y: number, items: MenuItem[]): void {
  menu.x = x;
  menu.y = y;
  menu.items = items;
  menu.open = true;
}

export function closeMenu(): void {
  menu.open = false;
}

/** The "copy value / paste value" clipboard. Kept in memory so it never needs a browser permission. */
export const valueClipboard: { value: number | null } = $state({ value: null });
