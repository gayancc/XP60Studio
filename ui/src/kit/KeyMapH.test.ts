import { cleanup, fireEvent, render } from '@testing-library/svelte';
import { tick } from 'svelte';
import { afterEach, describe, expect, it, vi } from 'vitest';
import KeyMapH, { type MapLane } from './KeyMapH.svelte';

afterEach(cleanup);

// jsdom has no ResizeObserver, which bind:clientWidth needs.
class NoopObserver {
  observe(): void {}
  unobserve(): void {}
  disconnect(): void {}
}
vi.stubGlobal('ResizeObserver', NoopObserver);

const lane = (id: number, low: number, high: number, extra: Partial<MapLane> = {}): MapLane => ({ id, low, high, color: '#ff8a1f', name: `Part ${id + 1}`, ...extra });

// jsdom has no layout: give the lanes a known size so pointer positions can be turned into notes.
const KEYS_W = 1280; // 10 px per semitone
const LANE_H = 22;

function mount(lanes: MapLane[], handlers: Record<string, unknown> = {}) {
  const view = render(KeyMapH, { props: { lanes, laneHeight: LANE_H, keyboardHeight: 56, ...handlers } });
  const svg = view.container.querySelector('svg.lanes') as SVGSVGElement;
  svg.getBoundingClientRect = () => ({ left: 0, top: 0, width: KEYS_W, height: lanes.length * LANE_H, right: KEYS_W, bottom: lanes.length * LANE_H, x: 0, y: 0, toJSON: () => ({}) });
  return { ...view, svg };
}

/** The map tells the app once per frame, so let a frame pass. */
const frame = (): Promise<void> => new Promise((r) => setTimeout(r, 40));
const at = (note: number, frac = 0.5): number => (note + frac) * 10;
const pointer = (type: string, x: number, y: number): PointerEvent => new PointerEvent(type, { bubbles: true, clientX: x, clientY: y, button: 0, pointerId: 3 });

describe('the horizontal key map', () => {
  it('draws the keyboard and one lane per part on the same scale, with the range written on each block', async () => {
    const { container } = mount([lane(0, 36, 72), lane(1, 60, 60)]);
    expect(container.querySelector('svg.piano')).not.toBeNull();
    expect(container.querySelectorAll('.km .g')).toHaveLength(2);
    expect(container.textContent).toContain('C2 – C5');
    expect(container.textContent).toContain('C4');
  });

  it('marks the selected lane and dims a part that is switched off', async () => {
    const { container } = mount([lane(0, 0, 127, { selected: true }), lane(1, 0, 127, { dim: true })]);
    expect(container.querySelector('.g.sel')).not.toBeNull();
    expect(container.querySelector('.g.off')).not.toBeNull();
  });

  it('dragging the right end of a block moves that end, and is one undo group from start to finish', async () => {
    const onrange = vi.fn();
    const onbegin = vi.fn();
    const onend = vi.fn();
    const { svg } = mount([lane(0, 36, 72)], { onrange, onbegin, onend });
    // The right edge is at (72 + 1) * 10 = 730 px.
    svg.dispatchEvent(pointer('pointerdown', 729, 11));
    expect(onbegin).toHaveBeenCalledTimes(1);
    for (const n of [76, 80, 84]) svg.dispatchEvent(pointer('pointermove', at(n), 11));
    await frame();
    svg.dispatchEvent(pointer('pointerup', at(84), 11));
    expect(onend).toHaveBeenCalledTimes(1);
    expect(onrange).toHaveBeenLastCalledWith(0, { low: 36, high: 84 });
    expect(onbegin).toHaveBeenCalledTimes(1);
  });

  it('dragging the left end moves only the low note and never crosses the high note', async () => {
    const onrange = vi.fn();
    const { svg } = mount([lane(0, 36, 48)], { onrange });
    svg.dispatchEvent(pointer('pointerdown', 361, 11)); // the left edge, at 36 * 10
    svg.dispatchEvent(pointer('pointermove', at(40), 11));
    await frame();
    expect(onrange).toHaveBeenLastCalledWith(0, { low: 40, high: 48 });
    svg.dispatchEvent(pointer('pointermove', at(100), 11));
    await frame();
    expect(onrange).toHaveBeenLastCalledWith(0, { low: 48, high: 48 });
    svg.dispatchEvent(pointer('pointerup', at(100), 11));
  });

  it('dragging the middle of a block moves the whole block and keeps its width', async () => {
    const onrange = vi.fn();
    const { svg } = mount([lane(0, 36, 48)], { onrange });
    svg.dispatchEvent(pointer('pointerdown', at(42), 11));
    svg.dispatchEvent(pointer('pointermove', at(47), 11)); // five semitones up
    await frame();
    expect(onrange).toHaveBeenLastCalledWith(0, { low: 41, high: 53 });
    svg.dispatchEvent(pointer('pointerup', at(47), 11));
  });

  it('a one-note block is moved when grabbed, not just stretched from one edge', async () => {
    const onrange = vi.fn();
    const { svg } = mount([lane(0, 60, 60)], { onrange });
    svg.dispatchEvent(pointer('pointerdown', at(60), 11));
    svg.dispatchEvent(pointer('pointermove', at(65), 11));
    await frame();
    expect(onrange).toHaveBeenLastCalledWith(0, { low: 65, high: 65 });
    svg.dispatchEvent(pointer('pointerup', at(65), 11));
  });

  it('the lane under the pointer decides which part is edited, and selects it', async () => {
    const onrange = vi.fn();
    const onselect = vi.fn();
    const { svg } = mount([lane(3, 0, 20), lane(4, 40, 60), lane(5, 80, 100)], { onrange, onselect });
    svg.dispatchEvent(pointer('pointerdown', at(50), LANE_H + 11)); // the middle lane, inside its block
    expect(onselect).toHaveBeenCalledWith(4);
    svg.dispatchEvent(pointer('pointermove', at(52), LANE_H + 11));
    await frame();
    expect(onrange).toHaveBeenLastCalledWith(4, { low: 42, high: 62 });
    svg.dispatchEvent(pointer('pointerup', at(52), LANE_H + 11));
  });

  it('clicking beside a block pulls the nearer end to that note', async () => {
    const onrange = vi.fn();
    const { svg } = mount([lane(0, 60, 70)], { onrange });
    svg.dispatchEvent(pointer('pointerdown', at(90), 11)); // above the block: the high end follows
    await frame();    expect(onrange).toHaveBeenLastCalledWith(0, { low: 60, high: 90 });
    svg.dispatchEvent(pointer('pointerup', at(90), 11));
  });

  it('follows the pointer at once but tells the app only once per frame', async () => {
    const onrange = vi.fn();
    const { svg, container } = mount([lane(0, 36, 48)], { onrange });
    svg.dispatchEvent(pointer('pointerdown', at(42), 11));
    for (let n = 42; n <= 60; n++) svg.dispatchEvent(pointer('pointermove', at(n), 11));
    // The block is already drawn where the pointer is, before any frame has passed and before the app has heard anything.
    expect(onrange).not.toHaveBeenCalled();
    await tick();
    expect(container.textContent).toContain('F#3 – F#4');
    await frame();
    expect(onrange).toHaveBeenCalledTimes(1);
    expect(onrange).toHaveBeenLastCalledWith(0, { low: 54, high: 66 });
    svg.dispatchEvent(pointer('pointerup', at(60), 11));
  });

  it('hands over the last position when the pointer is released before the frame', () => {
    const onrange = vi.fn();
    const onend = vi.fn();
    const { svg } = mount([lane(0, 36, 48)], { onrange, onend });
    svg.dispatchEvent(pointer('pointerdown', at(42), 11));
    svg.dispatchEvent(pointer('pointermove', at(50), 11));
    svg.dispatchEvent(pointer('pointerup', at(50), 11));
    expect(onrange).toHaveBeenLastCalledWith(0, { low: 44, high: 56 });
    expect(onend).toHaveBeenCalledTimes(1);
  });

  it('survives a pointer the browser cannot capture, and still ends the group', async () => {
    const onend = vi.fn();
    const onbegin = vi.fn();
    const { svg } = mount([lane(0, 36, 72)], { onend, onbegin });
    svg.setPointerCapture = () => {
      throw new Error('NotFoundError');
    };
    svg.dispatchEvent(pointer('pointerdown', at(50), 11));
    svg.dispatchEvent(pointer('pointerup', at(50), 11));
    expect(onbegin).toHaveBeenCalledTimes(1);
    expect(onend).toHaveBeenCalledTimes(1);
  });

  it('ends the undo group if the key map is removed in the middle of a drag', async () => {
    const onend = vi.fn();
    const { svg, unmount } = mount([lane(0, 36, 72)], { onend });
    svg.dispatchEvent(pointer('pointerdown', at(50), 11));
    unmount();
    expect(onend).toHaveBeenCalledTimes(1);
  });
});

void fireEvent;
