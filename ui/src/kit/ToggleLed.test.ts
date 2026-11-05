import { cleanup, fireEvent, render } from '@testing-library/svelte';
import { afterEach, describe, expect, it, vi } from 'vitest';
import ToggleLed from './ToggleLed.svelte';

afterEach(cleanup);

describe('the LED switch', () => {
  it('is named by its label', () => {
    const { getByRole } = render(ToggleLed, { props: { checked: false, label: 'Live edit' } });
    expect(getByRole('switch', { name: 'Live edit' })).toBeTruthy();
  });

  it('still has a name when it is drawn without a label, as in a dense row', () => {
    const { getByRole } = render(ToggleLed, { props: { checked: true, label: '', name: 'Receive Switch' } });
    const sw = getByRole('switch', { name: 'Receive Switch' });
    expect(sw.getAttribute('title')).toBe('Receive Switch');
    expect(sw.getAttribute('aria-checked')).toBe('true');
  });

  it('reports the new state when clicked', async () => {
    const onchange = vi.fn();
    const { getByRole } = render(ToggleLed, { props: { checked: false, label: 'On', onchange } });
    await fireEvent.click(getByRole('switch'));
    expect(onchange).toHaveBeenCalledWith(true);
  });
});
