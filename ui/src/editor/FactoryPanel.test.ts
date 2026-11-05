import { cleanup, fireEvent, render } from '@testing-library/svelte';
import { afterEach, describe, expect, it } from 'vitest';
import { choose, entries } from '../test/dropdown';
import FactoryPanel from './FactoryPanel.svelte';

afterEach(cleanup);

describe('the factory lists panel', () => {
  it('lists the 128 names of the first bank, numbered', () => {
    const { container } = render(FactoryPanel);
    expect(container.querySelectorAll('ol li').length).toBeGreaterThan(120);
    expect(container.querySelector('ol li')?.textContent).toContain('001');
    expect(container.querySelector('ol li')?.textContent).toContain('Temple of XP');
  });

  it('switches bank and narrows the list as you type', async () => {
    const { container, getByLabelText } = render(FactoryPanel);
    const bank = getByLabelText('Factory bank');
    await choose(bank, (await entries(bank))[4] ?? '');
    expect(container.querySelector('ol li')?.textContent).toContain('Piano 1');
    await fireEvent.input(getByLabelText('Find in the factory list'), { target: { value: 'piano 1' } });
    const items = [...container.querySelectorAll('ol li')].map((l) => l.textContent ?? '');
    expect(items.length).toBeGreaterThan(0);
    expect(items.length).toBeLessThan(10);
    expect(items.every((t) => t.toLowerCase().includes('piano 1'))).toBe(true);
  });

  it('says so when nothing matches', async () => {
    const { container, getByLabelText } = render(FactoryPanel);
    await fireEvent.input(getByLabelText('Find in the factory list'), { target: { value: 'zzzzqq' } });
    expect(container.querySelector('ol li')?.textContent).toBe('No name matches.');
  });
});
