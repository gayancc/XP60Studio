import { messages } from '../model/messages.svelte';
import { tick } from 'svelte';
import { afterEach, describe, expect, it } from 'vitest';
import { cleanup, fireEvent, render } from '@testing-library/svelte';
import { expansions } from '../model/expansions.svelte';
import ExpansionPanel from './ExpansionPanel.svelte';

afterEach(() => {
  cleanup();
  for (const n of [...expansions.installed]) expansions.set(n, false);
});

describe('the installed expansion boards setting', () => {
  it('lists every known board, the main run first and the promotional ones apart', () => {
    const { getAllByRole, getByText } = render(ExpansionPanel);
    expect(getAllByRole('switch')).toHaveLength(23);
    expect(getByText('Promotional and regional boards')).toBeTruthy();
    expect(getByText('Asia')).toBeTruthy();
  });

  it('ticks and unticks a board and counts the slots used', async () => {
    const { getAllByRole, getByText } = render(ExpansionPanel);
    const [pop] = getAllByRole('switch');
    await fireEvent.click(pop!);
    expect(expansions.has(1)).toBe(true);
    expect(pop!.getAttribute('aria-checked')).toBe('true');
    expect(getByText('1 of 4 slots')).toBeTruthy();
    await fireEvent.click(pop!);
    expect(expansions.has(1)).toBe(false);
  });

  it('stops at four boards and disables the rest, saying why', async () => {
    messages.clear();
    const { getAllByRole } = render(ExpansionPanel);
    const boards = getAllByRole('switch');
    for (const b of boards.slice(0, 4)) await fireEvent.click(b);
    expect(expansions.installed).toHaveLength(4);
    expect((boards[4] as HTMLButtonElement).disabled).toBe(true);
    expansions.set(9, true);
    await tick();
    expect(expansions.notice).toContain('4 expansion slots');
    expect(messages.list.map((m) => m.text).join(' | ')).toContain('4 expansion slots');
    expect(messages.list.at(-1)?.kind).toBe('warning');
  });
});
