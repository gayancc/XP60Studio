import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { cleanup, fireEvent, render } from '@testing-library/svelte';
import { tick } from 'svelte';
import Stepper from './Stepper.svelte';

const LABELS = ['-100', '-70', '-50', '0', '+10', '+30'];

beforeEach(() => vi.useFakeTimers());
afterEach(() => {
  cleanup();
  vi.useRealTimers();
});

function setup(value = 3, extra: Record<string, unknown> = {}) {
  const calls = { change: [] as number[], begin: 0, end: 0 };
  const r = render(Stepper, {
    props: {
      value,
      min: 0,
      max: LABELS.length - 1,
      label: 'Key follow',
      name: 'Cutoff key follow',
      format: (i: number) => LABELS[i] ?? String(i),
      onchange: (v: number) => calls.change.push(v),
      onbegin: () => calls.begin++,
      onend: () => calls.end++,
      ...extra,
    },
  });
  return { ...r, calls };
}
const dec = (c: HTMLElement) => c.querySelector('button[aria-label^="Decrease"]') as HTMLButtonElement;
const inc = (c: HTMLElement) => c.querySelector('button[aria-label^="Increase"]') as HTMLButtonElement;

describe('the stepper', () => {
  it('shows the list entry for the value, not its index', () => {
    const { getByRole } = setup(2);
    expect(getByRole('spinbutton').textContent?.trim()).toBe('-50');
    expect(getByRole('spinbutton').getAttribute('aria-valuetext')).toBe('-50');
    expect(getByRole('spinbutton').getAttribute('aria-valuenow')).toBe('2');
  });

  it('steps one entry with a keyboard activation of a tile, as one undo step', async () => {
    const { container, calls } = setup(3);
    await fireEvent.click(inc(container));
    expect(calls.change).toEqual([4]);
    expect([calls.begin, calls.end]).toEqual([1, 1]);
  });

  it('steps once on press and repeats while held, faster over time, as one undo step', async () => {
    const { container, calls } = setup(0, { max: 40, format: undefined });
    await fireEvent.pointerDown(inc(container), { button: 0, pointerId: 1 });
    expect(calls.change).toEqual([1]);
    vi.advanceTimersByTime(380 + 10);
    expect(calls.change.length).toBe(2);
    vi.advanceTimersByTime(1500);
    expect(calls.change.length).toBeGreaterThan(8);
    await fireEvent.pointerUp(inc(container), { pointerId: 1 });
    const stopped = calls.change.length;
    vi.advanceTimersByTime(1000);
    expect(calls.change.length, 'releasing stops it').toBe(stopped);
    expect([calls.begin, calls.end]).toEqual([1, 1]);
  });

  it('cannot go past either end, and the tile at the end is disabled', async () => {
    const { container, getByRole } = setup(0);
    expect(dec(container).disabled).toBe(true);
    expect(inc(container).disabled).toBe(false);
    await fireEvent.click(dec(container));
    expect(getByRole('spinbutton').getAttribute('aria-valuenow')).toBe('0');
    const top = setup(LABELS.length - 1);
    expect(inc(top.container).disabled).toBe(true);
  });

  it('steps with the arrow keys and the wheel over the value', async () => {
    const { getByRole, calls } = setup(3);
    const val = getByRole('spinbutton');
    await fireEvent.keyDown(val, { key: 'ArrowUp' });
    expect(calls.change.at(-1)).toBe(4);
    await fireEvent.wheel(val, { deltaY: -100, deltaMode: 0 });
    expect(calls.change.at(-1)).toBe(5);
    await tick();
  });

  it('puts the value back to its default on a double-click', async () => {
    const { getByRole, calls } = setup(5, { defaultValue: 3 });
    await fireEvent.dblClick(getByRole('spinbutton'));
    expect(calls.change.at(-1)).toBe(3);
  });

  it('draws no border: the readout and tiles are told apart by fill', () => {
    const { container } = setup(3);
    const css = [...document.querySelectorAll('style')].map((s) => s.textContent).join('\n');
    expect(css).not.toMatch(/\.row\s*\{[^}]*border:\s*1px/);
    expect(container.querySelector('.row')).not.toBeNull();
  });
});
