import { cleanup, fireEvent, render, screen } from '@testing-library/svelte';
import { afterEach, describe, expect, it, vi } from 'vitest';
import { hint } from './hint.svelte';
import Knob from './Knob.svelte';
import Segmented from './Segmented.svelte';
import ToggleLed from './ToggleLed.svelte';

afterEach(cleanup);

describe('Knob', () => {
  it('exposes name, value and range to assistive tech', () => {
    render(Knob, { props: { value: 64, name: 'Cutoff Frequency' } });
    const k = screen.getByRole('slider', { name: 'Cutoff Frequency' });
    expect(k.getAttribute('aria-valuenow')).toBe('64');
    expect(k.getAttribute('aria-valuemin')).toBe('0');
    expect(k.getAttribute('aria-valuemax')).toBe('127');
  });

  it('arrow keys step the value and report the change', async () => {
    const onchange = vi.fn();
    render(Knob, { props: { value: 10, name: 'Level', onchange } });
    const k = screen.getByRole('slider', { name: 'Level' });
    await fireEvent.keyDown(k, { key: 'ArrowUp' });
    expect(onchange).toHaveBeenLastCalledWith(11);
    await fireEvent.keyDown(k, { key: 'End' });
    expect(onchange).toHaveBeenLastCalledWith(127);
  });

  it('double-click resets to the default and brackets it as one undo step', async () => {
    const onchange = vi.fn();
    const onbegin = vi.fn();
    const onend = vi.fn();
    render(Knob, { props: { value: 10, defaultValue: 100, name: 'Level', onchange, onbegin, onend } });
    await fireEvent.dblClick(screen.getByRole('slider', { name: 'Level' }));
    expect(onchange).toHaveBeenCalledWith(100);
    expect(onbegin).toHaveBeenCalledTimes(1);
    expect(onend).toHaveBeenCalledTimes(1);
  });

  it('does nothing when disabled', async () => {
    const onchange = vi.fn();
    render(Knob, { props: { value: 10, name: 'Level', disabled: true, onchange } });
    await fireEvent.keyDown(screen.getByRole('slider', { name: 'Level' }), { key: 'ArrowUp' });
    expect(onchange).not.toHaveBeenCalled();
  });

  it('publishes name and meaning to the hint bar on hover and clears it on leave', async () => {
    render(Knob, { props: { value: 1, min: -3, max: 3, name: 'Octave', context: 'Tone 2 > Pitch', format: (v: number) => `+${v} oct` } });
    const k = screen.getByRole('slider', { name: 'Octave' });
    await fireEvent.pointerEnter(k);
    expect(hint.name).toBe('Octave');
    expect(hint.value).toBe('+1 oct');
    expect(hint.raw).toBe('1');
    expect(hint.context).toBe('Tone 2 > Pitch');
    await fireEvent.pointerLeave(k);
    expect(hint.name).toBe('');
  });
});

describe('ToggleLed', () => {
  it('toggles on click and reports the new state', async () => {
    const onchange = vi.fn();
    render(ToggleLed, { props: { checked: false, label: 'Solo', onchange } });
    const sw = screen.getByRole('switch', { name: /solo/i });
    expect(sw.getAttribute('aria-checked')).toBe('false');
    await fireEvent.click(sw);
    expect(onchange).toHaveBeenCalledWith(true);
  });
});

describe('Segmented', () => {
  it('selects with the mouse and with arrow keys, and stays inside the options', async () => {
    const onchange = vi.fn();
    render(Segmented, { props: { options: ['A', 'B', 'C'], selected: 0, name: 'Mode', onchange } });
    await fireEvent.click(screen.getByRole('radio', { name: 'B' }));
    expect(onchange).toHaveBeenLastCalledWith(1);
    await fireEvent.keyDown(screen.getByRole('radiogroup'), { key: 'ArrowRight' });
    expect(onchange).toHaveBeenLastCalledWith(2);
    onchange.mockClear();
    await fireEvent.keyDown(screen.getByRole('radiogroup'), { key: 'ArrowRight' });
    expect(onchange).not.toHaveBeenCalled();
  });
});
