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
  let o = initial;
  let drag: DragState | null = null;
  let wheelCarry = 0;

  const coord = (e: PointerEvent): number => (o.axis === 'x' ? -e.clientX : e.clientY);
  const isFine = (e: { ctrlKey: boolean; shiftKey: boolean }): boolean => e.ctrlKey || e.shiftKey;

  /** Apply a change as one begin/set/end group, and only if the value really changes. */
  const commitOnce = (next: number): void => {
    if (next === o.get()) return;
    o.begin?.();
    o.set(next);
    o.end?.();
  };

  const finishDrag = (): void => {
    if (drag?.begun) o.end?.();
    drag = null;
  };

  const reset = (): void => {
    if (o.disabled || o.defaultValue === undefined) return;
    commitOnce(o.defaultValue);
  };

  const down = (e: PointerEvent): void => {
    if (o.disabled || drag || !e.isPrimary || e.button !== 0) return;
    if (e.altKey) {
      e.preventDefault();
      reset();
      return;
    }
    node.setPointerCapture(e.pointerId);
    node.focus();
    drag = { id: e.pointerId, origin: coord(e), start: o.get(), fine: isFine(e), begun: false };
  };

  const move = (e: PointerEvent): void => {
    if (!drag || e.pointerId !== drag.id) return;
    if (o.disabled) {
      finishDrag();
      return;
    }
    // Changing the modifier mid-drag re-bases the drag so the value does not jump.
    if (isFine(e) !== drag.fine) drag = { ...drag, origin: coord(e), start: o.get(), fine: isFine(e) };
    const next = dragValue(drag.start, coord(e) - drag.origin, o.range, drag.fine, o.pxFull);
    if (next === o.get()) return;
    if (!drag.begun) {
      drag.begun = true;
      o.begin?.();
    }
    o.set(next);
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
    commitOnce(wheelValue(o.get(), notches, o.range, isFine(e)));
  };

  const key = (e: KeyboardEvent): void => {
    if (o.disabled || e.altKey || e.ctrlKey || e.metaKey) return;
    const intent = intentFromKey(e.key);
    if (!intent) return;
    e.preventDefault();
    commitOnce(keyValue(o.get(), intent, o.range));
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
      if (o.disabled) finishDrag();
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
