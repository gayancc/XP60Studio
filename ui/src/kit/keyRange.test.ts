import { cleanup, fireEvent, render } from '@testing-library/svelte';
import { afterEach, describe, expect, it, vi } from 'vitest';
import { noteName, octaveMarks } from './keyRange';
import KeyRangeBar from './KeyRangeBar.svelte';

afterEach(cleanup);

describe('octave ruler marks', () => {
  it('has one band per octave from C-1 to G9, tiling the whole width with no gaps', () => {
    const marks = octaveMarks(200);
    expect(marks).toHaveLength(11);
    expect(marks[0]?.x).toBe(0);
    marks.forEach((m, i) => {
      if (i > 0) expect(m.x).toBeCloseTo((marks[i - 1] as { x: number; w: number }).x + (marks[i - 1] as { w: number }).w, 6);
    });
    const last = marks[marks.length - 1] as { x: number; w: number };
    expect(last.x + last.w).toBeCloseTo(200, 6);
    // G9 is note 127, so the last band is the short one: only the notes up to G.
    expect(last.w).toBeCloseTo((8 / 128) * 200, 6);
  });

  it('alternates band shading so octaves can be told apart without labels', () => {
    expect(octaveMarks(200).map((m) => m.alt)).toEqual([false, true, false, true, false, true, false, true, false, true, false]);
  });

  it('labels every octave when there is room, every second or fourth when there is not', () => {
    const labelled = (w: number): string[] => octaveMarks(w).flatMap((m) => (m.label ? [m.label] : []));
    expect(labelled(600)).toHaveLength(11); // about 56 px per octave
    expect(labelled(300)).toHaveLength(11); // 28 px per octave is still room for every label
    expect(labelled(200)).toEqual(['C-1', 'C1', 'C3', 'C5', 'C7', 'C9']); // 19 px: every second
    expect(labelled(100)).toEqual(['C-1', 'C3', 'C7']); // 9 px: every fourth
  });

  it('never lets two labels sit closer than a label is wide', () => {
    for (const w of [120, 200, 260, 420, 640]) {
      const marks = octaveMarks(w).filter((m) => m.label);
      for (let i = 1; i < marks.length; i++) {
        expect((marks[i] as { x: number }).x - (marks[i - 1] as { x: number }).x, `width ${w}`).toBeGreaterThanOrEqual(23.9);
      }
    }
  });

  it('names each label as the C that starts its band', () => {
    for (const m of octaveMarks(600)) expect(m.label).toBe(noteName(Math.round((m.x / 600) * 128)));
  });
});

describe('the ruler display of a key range', () => {
  const make = (props: Record<string, unknown> = {}) =>
    render(KeyRangeBar, { props: { low: 36, high: 60, width: 200, height: 42, display: 'ruler', color: '#ff8a1f', ...props } });

  it('draws bands, not a crowd of piano keys, and writes the range on the block', () => {
    const { container } = make();
    expect(container.querySelector('svg.piano')).toBeNull();
    expect(container.querySelectorAll('svg.lane rect').length).toBeGreaterThan(11);
    expect(container.textContent).toContain('C2 – C4');
  });

  it('puts the text beside a narrow block instead of squeezing it in', () => {
    const { container } = make({ low: 60, high: 62 });
    const text = container.querySelector('text.rng');
    expect(text?.getAttribute('class')).not.toContain('dark');
    expect(container.textContent).toContain('C4 – D4');
  });

  it('shows a single note as one name', () => {
    const { container } = make({ low: 60, high: 60 });
    expect(container.textContent).toContain('C4');
    expect(container.textContent).not.toContain('C4 – C4');
  });

  it('moves an end with the arrow keys, by an octave with Shift, and reports the change', async () => {
    const onchange = vi.fn();
    const { getByRole } = make({ onchange });
    const high = getByRole('slider', { name: 'Key range high' });
    await fireEvent.keyDown(high, { key: 'ArrowRight' });
    expect(onchange).toHaveBeenLastCalledWith({ low: 36, high: 61 });
    await fireEvent.keyDown(high, { key: 'ArrowRight', shiftKey: true });
    expect(onchange).toHaveBeenLastCalledWith({ low: 36, high: 73 });
  });

  it('keeps the keys display for places with room for it', () => {
    const { container } = render(KeyRangeBar, { props: { low: 36, high: 60, width: 420, height: 40 } });
    expect(container.querySelector('svg.piano')).not.toBeNull();
  });
});
