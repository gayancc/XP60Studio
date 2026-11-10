import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { cleanup, fireEvent, render } from '@testing-library/svelte';
import StepSelect from './StepSelect.svelte';

beforeEach(() => vi.useFakeTimers());
afterEach(() => {
  cleanup();
  vi.useRealTimers();
});

const NAMES = ['Nice Piano', 'Strings', 'Flute'];

function setup(extra: Record<string, unknown> = {}) {
  const picks: number[] = [];
  const r = render(StepSelect, {
    props: { names: NAMES, numbers: [1, 2, 3], selected: 1, onselect: (i: number) => picks.push(i), name: 'Patch', ...extra },
  });
  const prev = r.getByLabelText('Previous Patch') as HTMLButtonElement;
  const next = r.getByLabelText('Next Patch') as HTMLButtonElement;
  return { ...r, picks, prev, next };
}

describe('StepSelect', () => {
  it('is one face holding both segments and the entry, with its number first', () => {
    const { container } = setup();
    const face = container.querySelector('.face') as HTMLElement;
    expect(face.querySelectorAll('button.seg')).toHaveLength(2);
    expect(face.querySelector('.mid [role=combobox]')).not.toBeNull();
    expect(face.querySelector('.mid .no')?.textContent).toBe('002');
    expect(face.querySelector('.mid .text')?.textContent).toBe('Strings');
  });

  it('with no numbers it shows the entry alone', () => {
    const { container } = setup({ numbers: undefined });
    expect(container.querySelector('.no')).toBeNull();
  });

  it('a press picks the neighbouring entry at once and holding repeats until released', async () => {
    const { next, picks } = setup({ names: ['a', 'b', 'c', 'd', 'e', 'f', 'g'], numbers: undefined, selected: 0 });
    await fireEvent.pointerDown(next, { button: 0 });
    expect(picks).toEqual([1]);
    vi.advanceTimersByTime(380 + 110 * 3);
    expect(picks.length).toBeGreaterThanOrEqual(3);
    await fireEvent.pointerUp(next);
    const n = picks.length;
    vi.advanceTimersByTime(600);
    expect(picks).toHaveLength(n);
  });

  it('an onstep handler replaces the default neighbour pick (the bank steps by its own rules)', async () => {
    const steps: number[] = [];
    const { next, prev, picks } = setup({ onstep: (d: number) => steps.push(d) });
    await fireEvent.pointerDown(next, { button: 0 });
    await fireEvent.pointerUp(next);
    await fireEvent.pointerDown(prev, { button: 0 });
    await fireEvent.pointerUp(prev);
    expect(steps).toEqual([1, -1]);
    expect(picks).toEqual([]);
  });

  it('a pointer press is not stepped a second time by the click that follows, but a keyboard press steps', async () => {
    const { next, picks } = setup();
    await fireEvent.pointerDown(next, { button: 0 });
    await fireEvent.pointerUp(next);
    await fireEvent.click(next, { detail: 1 });
    expect(picks).toEqual([2]);
    await fireEvent.click(next, { detail: 0 });
    expect(picks).toEqual([2, 2]);
  });

  it('the first entry cannot step back and the last cannot step forward', async () => {
    const first = setup({ selected: 0 });
    expect(first.prev.disabled).toBe(true);
    expect(first.next.disabled).toBe(false);
    cleanup();
    const last = setup({ selected: 2 });
    expect(last.next.disabled).toBe(true);
    await fireEvent.pointerDown(last.next, { button: 0 });
    expect(last.picks).toEqual([]);
  });

  it('the wheel over the face steps and stops at the ends', async () => {
    const { container, picks, rerender } = setup();
    const face = container.querySelector('.face') as HTMLElement;
    await fireEvent.wheel(face, { deltaY: 100 });
    await fireEvent.wheel(face, { deltaY: -100 });
    expect(picks).toEqual([2, 0]);
    await rerender({ canNext: false });
    await fireEvent.wheel(face, { deltaY: 100 });
    expect(picks).toEqual([2, 0]);
  });

  it('the strip shows where the entry sits in the list, and is empty for one entry', () => {
    const first = setup({ selected: 0 });
    expect((first.container.querySelector('.fill') as HTMLElement).style.width).toBe('0%');
    cleanup();
    const last = setup({ selected: 2 });
    expect((last.container.querySelector('.fill') as HTMLElement).style.width).toBe('100%');
    cleanup();
    const one = setup({ names: ['Only'], numbers: [1], selected: 0 });
    expect((one.container.querySelector('.fill') as HTMLElement).style.width).toBe('0%');
  });
});
