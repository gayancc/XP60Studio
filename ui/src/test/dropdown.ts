// Test support for the kit Dropdown, which is a button that opens a list (group headings are bars inside the list), not a native select.
import { fireEvent } from '@testing-library/svelte';
import { tick } from 'svelte';

const options = (): HTMLElement[] => [...document.querySelectorAll<HTMLElement>('[role=listbox] [role=option]')];
const text = (e: HTMLElement): string => e.textContent?.replace('✓', '').trim() ?? '';

async function closeList(): Promise<void> {
  const list = document.querySelector('[role=listbox]');
  if (list) await fireEvent.keyDown(list, { key: 'Escape' });
  await tick();
}

/** The text shown on the closed drop-down. */
export const shown = (trigger: HTMLElement): string => trigger.querySelector('.text')?.textContent ?? '';

/** Open the list and return its entries in order. */
export async function entries(trigger: HTMLElement): Promise<string[]> {
  await fireEvent.click(trigger);
  await tick();
  const out = options().map(text);
  await closeList();
  return out;
}

/** The group headings of a grouped list, in order. */
export async function groupLabels(trigger: HTMLElement): Promise<string[]> {
  await fireEvent.click(trigger);
  await tick();
  const out = [...document.querySelectorAll('[role=listbox] .header')].map((h) => h.textContent?.trim() ?? '');
  await closeList();
  return out;
}

/** The entry carrying the check mark while the list is open. */
export async function checked(trigger: HTMLElement): Promise<string | undefined> {
  await fireEvent.click(trigger);
  await tick();
  const out = options().find((o) => o.getAttribute('aria-selected') === 'true');
  const t = out ? text(out) : undefined;
  await closeList();
  return t;
}

/** Open the list and pick the entry with exactly this text. */
export async function choose(trigger: HTMLElement, label: string): Promise<void> {
  await fireEvent.click(trigger);
  await tick();
  const hit = options().find((o) => text(o) === label);
  if (!hit) throw new Error(`no entry ${label}`);
  await fireEvent.click(hit);
}
