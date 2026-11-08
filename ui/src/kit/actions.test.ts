import { describe, expect, it } from 'vitest';
import { valueGestures, type GestureOptions } from './actions';

function pointer(type: string, clientY: number): PointerEvent {
  return new PointerEvent(type, {
    bubbles: true,
    cancelable: true,
    pointerId: 1,
    isPrimary: true,
    button: 0,
    clientX: 0,
    clientY,
  });
}

function mountNode(): HTMLDivElement {
  const node = document.createElement('div');
  // jsdom lacks pointer capture; the gesture action calls these on down/up.
  node.setPointerCapture = () => undefined;
  node.releasePointerCapture = () => undefined;
  node.hasPointerCapture = () => false;
  node.focus = () => undefined;
  document.body.appendChild(node);
  return node;
}

describe('valueGestures path freeze', () => {
  it('keeps writing through the options frozen at pointer-down when update() retargets mid-drag', () => {
    const node = mountNode();
    let tone1 = 40;
    let tone2 = 40;
    const tone1Opts: GestureOptions = {
      range: { min: 0, max: 127, step: 1 },
      get: () => tone1,
      set: (v) => {
        tone1 = v;
      },
      pxFull: 100,
    };
    const tone2Opts: GestureOptions = {
      range: { min: 0, max: 127, step: 1 },
      get: () => tone2,
      set: (v) => {
        tone2 = v;
      },
      pxFull: 100,
    };

    const action = valueGestures(node, tone1Opts);
    node.dispatchEvent(pointer('pointerdown', 100));
    // Parent rebinds the control to Tone 2 while the drag is open (Selected-Tone panel without remount).
    action.update?.(tone2Opts);
    node.dispatchEvent(pointer('pointermove', 20));
    node.dispatchEvent(pointer('pointerup', 20));

    expect(tone1).not.toBe(40);
    expect(tone2).toBe(40);

    action.destroy?.();
    node.remove();
  });

  it('uses the latest options for a new gesture after the previous one ends', () => {
    const node = mountNode();
    let tone1 = 10;
    let tone2 = 10;
    const action = valueGestures(node, {
      range: { min: 0, max: 127, step: 1 },
      get: () => tone1,
      set: (v) => {
        tone1 = v;
      },
      pxFull: 100,
    });
    action.update?.({
      range: { min: 0, max: 127, step: 1 },
      get: () => tone2,
      set: (v) => {
        tone2 = v;
      },
      pxFull: 100,
    });

    node.dispatchEvent(pointer('pointerdown', 100));
    node.dispatchEvent(pointer('pointermove', 20));
    node.dispatchEvent(pointer('pointerup', 20));

    expect(tone1).toBe(10);
    expect(tone2).not.toBe(10);

    action.destroy?.();
    node.remove();
  });
});
