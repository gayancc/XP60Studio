// DOM wiring for the value gestures. The maths lives in gesture.ts; this only translates events.

import { clearHint } from './hint.svelte';
import { dragValue, intentFromKey, keyValue, wheelValue, type Range } from './gesture';

export interface GestureOptions {
  range: Range;
  get: () => number;
  set: (value: number) => void;
  defaultValue?: number;
  /**
   * Bracket one user action (a whole drag, one wheel notch, one key press). `begin` is called lazily, just before the
   * first change, so a click that changes nothing never opens an undo group; `end` is called only after a `begin`.
   */
  begin?: () => void;
  end?: () => void;
  /** Right-click. */
  onMenu?: (x: number, y: number) => void;
  /** Pixels that sweep the whole range for this control. */
  pxFull?: number;
  /** Drag direction: 'y' (up increases, knobs) or 'x' (right increases, horizontal sliders). */
  axis?: 'x' | 'y';
  disabled?: boolean;
}

interface DragState {
  id: number;
  origin: number;
  start: number;
  fine: boolean;
  begun: boolean;
}

/** Wheel pixels in one notch for a classic mouse; smaller deltas (trackpads) accumulate. */
const WHEEL_PIXELS_PER_NOTCH = 100;
const WHEEL_LINES_PER_NOTCH = 3;

export function valueGestures(node: HTMLElement, initial: GestureOptions) {
  /** Latest options from the component (may retarget if the parent rebinds the control). */
  let o = initial;
  /**
   * Options frozen for the open gesture. Patch Base keeps SynthPath identity fixed for a controller while an edit is
   * in flight; without this, a Selected-Tone panel that receives a new `block` mid-drag would write into another Tone.
   */
  let gesture: GestureOptions | null = null;
  let drag: DragState | null = null;
  let wheelCarry = 0;

  const live = (): GestureOptions => gesture ?? o;
  const coord = (e: PointerEvent): number => (live().axis === 'x' ? -e.clientX : e.clientY);
  const isFine = (e: { ctrlKey: boolean; shiftKey: boolean }): boolean => e.ctrlKey || e.shiftKey;

  /** Apply a change as one begin/set/end group, and only if the value really changes. */
  const commitOnce = (next: number): void => {
    const g = live();
    if (next === g.get()) return;
    gesture = g;
    try {
      g.begin?.();
      g.set(next);
      g.end?.();
    } finally {
      gesture = null;
    }
  };

  const finishDrag = (): void => {
    const g = gesture ?? o;
    if (drag?.begun) g.end?.();
    drag = null;
    gesture = null;
  };

  const reset = (): void => {
    if (live().disabled || live().defaultValue === undefined) return;
    commitOnce(live().defaultValue as number);
  };

  const down = (e: PointerEvent): void => {
    if (o.disabled || drag || !e.isPrimary || e.button !== 0) return;
    if (e.altKey) {
      e.preventDefault();
      reset();
      return;
    }
    gesture = o;
    node.setPointerCapture(e.pointerId);
    node.focus();
    drag = { id: e.pointerId, origin: coord(e), start: gesture.get(), fine: isFine(e), begun: false };
  };

  const move = (e: PointerEvent): void => {
    if (!drag || e.pointerId !== drag.id) return;
    const g = live();
    if (g.disabled) {
      finishDrag();
      return;
    }
    // Changing the modifier mid-drag re-bases the drag so the value does not jump.
    if (isFine(e) !== drag.fine) drag = { ...drag, origin: coord(e), start: g.get(), fine: isFine(e) };
    const next = dragValue(drag.start, coord(e) - drag.origin, g.range, drag.fine, g.pxFull);
    if (next === g.get()) return;
    if (!drag.begun) {
      drag.begun = true;
      g.begin?.();
    }
    g.set(next);
  };

  const up = (e: PointerEvent): void => {
    if (!drag || e.pointerId !== drag.id) return;
    if (node.hasPointerCapture(e.pointerId)) node.releasePointerCapture(e.pointerId);
    finishDrag();
  };

  const wheel = (e: WheelEvent): void => {
    if (o.disabled) return;
    e.preventDefault();
    // Shift+wheel is reported as horizontal scroll by some browsers (WebView2 on Windows).
    const delta = e.deltaY || e.deltaX;
    const unit = e.deltaMode === 1 ? WHEEL_LINES_PER_NOTCH : e.deltaMode === 2 ? 1 : WHEEL_PIXELS_PER_NOTCH;
    wheelCarry += -delta / unit;
    const notches = Math.trunc(wheelCarry);
    if (notches === 0) return;
    wheelCarry -= notches;
    commitOnce(wheelValue(live().get(), notches, live().range, isFine(e)));
  };

  const key = (e: KeyboardEvent): void => {
    if (o.disabled || e.altKey || e.ctrlKey || e.metaKey) return;
    const intent = intentFromKey(e.key);
    if (!intent) return;
    e.preventDefault();
    commitOnce(keyValue(live().get(), intent, live().range));
  };

  const menu = (e: MouseEvent): void => {
    e.preventDefault();
    if (!o.disabled) o.onMenu?.(e.clientX, e.clientY);
  };

  node.addEventListener('pointerdown', down);
  node.addEventListener('pointermove', move);
  node.addEventListener('pointerup', up);
  node.addEventListener('pointercancel', up);
  node.addEventListener('wheel', wheel, { passive: false });
  node.addEventListener('keydown', key);
  node.addEventListener('dblclick', reset);
  node.addEventListener('contextmenu', menu);

  return {
    update(next: GestureOptions): void {
      o = next;
      // Never swap the in-flight path; only honour a disable that ends the gesture.
      if (gesture === null && o.disabled) finishDrag();
      else if (gesture !== null && next.disabled) finishDrag();
    },
    destroy(): void {
      // A control removed mid-drag must still close its undo group, and must not leave its hint on screen.
      finishDrag();
      clearHint();
      node.removeEventListener('pointerdown', down);
      node.removeEventListener('pointermove', move);
      node.removeEventListener('pointerup', up);
      node.removeEventListener('pointercancel', up);
      node.removeEventListener('wheel', wheel);
      node.removeEventListener('keydown', key);
      node.removeEventListener('dblclick', reset);
      node.removeEventListener('contextmenu', menu);
    },
  };
}
