import { describe, expect, it } from 'vitest';
import {
  DRAG_PX_FULL,
  FINE_RATIO,
  WHEEL_NOTCH_PX,
  dragValue,
  fromNorm,
  intentFromKey,
  keyValue,
  snap,
  toNorm,
  wheelValue,
  type Range,
} from './gesture';
import { arcPath, normToAngle, polar, valueArc } from './geometry';

const r127: Range = { min: 0, max: 127 };
const r15: Range = { min: 0, max: 15 };
const signed: Range = { min: -50, max: 50 };

describe('normalisation', () => {
  it('maps the ends and the middle', () => {
    expect(toNorm(0, r127)).toBe(0);
    expect(toNorm(127, r127)).toBe(1);
    expect(toNorm(0, signed)).toBe(0.5);
  });
  it('round-trips every integer of a 0..127 range', () => {
    for (let v = 0; v <= 127; v++) expect(fromNorm(toNorm(v, r127), r127)).toBe(v);
  });
  it('clamps out-of-range input and survives a zero-width range', () => {
    expect(toNorm(500, r127)).toBe(1);
    expect(toNorm(5, { min: 5, max: 5 })).toBe(0);
    expect(snap(-9, r127)).toBe(0);
  });
  it('snaps to a step grid anchored at min', () => {
    expect(snap(7, { min: 1, max: 100, step: 5 })).toBe(6);
    expect(snap(0.3, { min: 0, max: 1, step: 0.1 })).toBe(0.3);
  });
});

describe('vertical drag', () => {
  it('dragging up by the full distance sweeps the whole range', () => {
    expect(dragValue(0, -DRAG_PX_FULL, r127)).toBe(127);
    expect(dragValue(127, DRAG_PX_FULL, r127)).toBe(0);
  });
  it('is computed from the drag start, so returning to the start restores the value', () => {
    expect(dragValue(64, 0, r127)).toBe(64);
    expect(dragValue(64, -40, r127)).toBeGreaterThan(64);
    expect(dragValue(64, 0, r127)).toBe(64);
  });
  it('fine mode moves about 0.28 as far', () => {
    const normal = dragValue(0, -80, r127);
    const fine = dragValue(0, -80, r127, true);
    expect(fine / normal).toBeCloseTo(FINE_RATIO, 1);
  });
  it('never leaves the range', () => {
    expect(dragValue(120, -10_000, r127)).toBe(127);
    expect(dragValue(5, 10_000, r127)).toBe(0);
  });
  it('small integer ranges move in whole steps', () => {
    expect(Number.isInteger(dragValue(7, -20, r15))).toBe(true);
  });
});

describe('wheel', () => {
  it('one notch moves at least one step, in the right direction', () => {
    expect(wheelValue(7, 1, r15)).toBeGreaterThan(7);
    expect(wheelValue(7, -1, r15)).toBeLessThan(7);
  });
  it('matches the drag equivalent of one notch on a wide range', () => {
    const viaWheel = wheelValue(0, 1, r127);
    const viaDrag = dragValue(0, -WHEEL_NOTCH_PX, r127);
    expect(Math.abs(viaWheel - viaDrag)).toBeLessThanOrEqual(1);
  });
  it('fine mode never goes below one step', () => {
    expect(wheelValue(7, 1, r15, true)).toBe(8);
  });
  it('zero notches change nothing; the ends clamp', () => {
    expect(wheelValue(7, 0, r15)).toBe(7);
    expect(wheelValue(15, 3, r15)).toBe(15);
    expect(wheelValue(0, -3, r15)).toBe(0);
  });
});

describe('keyboard', () => {
  it('maps keys to intents', () => {
    expect(intentFromKey('ArrowUp')).toBe('up');
    expect(intentFromKey('ArrowLeft')).toBe('down');
    expect(intentFromKey('PageUp')).toBe('pageUp');
    expect(intentFromKey('x')).toBeNull();
  });
  it('steps, pages and jumps to the ends', () => {
    expect(keyValue(10, 'up', r127)).toBe(11);
    expect(keyValue(10, 'down', r127)).toBe(9);
    expect(keyValue(10, 'pageUp', r127)).toBe(23);
    expect(keyValue(10, 'home', r127)).toBe(0);
    expect(keyValue(10, 'end', r127)).toBe(127);
    expect(keyValue(127, 'up', r127)).toBe(127);
  });
});

describe('knob geometry', () => {
  it('puts 0 degrees at twelve o-clock and 90 at three', () => {
    const top = polar(50, 50, 10, 0);
    expect(top.x).toBeCloseTo(50);
    expect(top.y).toBeCloseTo(40);
    const right = polar(50, 50, 10, 90);
    expect(right.x).toBeCloseTo(60);
    expect(right.y).toBeCloseTo(50);
  });
  it('maps the sweep ends', () => {
    expect(normToAngle(0)).toBe(-135);
    expect(normToAngle(1)).toBe(135);
    expect(normToAngle(0.5)).toBe(0);
    expect(normToAngle(9)).toBe(135);
  });
  it('unipolar arcs start at the left end, bipolar at the origin', () => {
    expect(valueArc(0.75, null)).toEqual({ from: -135, to: 67.5 });
    expect(valueArc(0.75, 0.5)).toEqual({ from: 0, to: 67.5 });
  });
  it('produces a path only when there is something to draw', () => {
    expect(arcPath(20, 20, 10, 0, 0)).toBe('');
    expect(arcPath(20, 20, 10, -135, 135)).toMatch(/^M .* A 10 10 0 1 1 /);
    expect(arcPath(20, 20, 10, 0, 90)).toMatch(/ 0 0 1 /);
  });
});

describe('bad numbers', () => {
  it('NaN never reaches a control value', () => {
    expect(Number.isNaN(snap(Number.NaN, r127))).toBe(false);
    expect(Number.isNaN(dragValue(10, -5, r127, false, 0))).toBe(false);
    expect(Number.isNaN(dragValue(10, -5, r127, false, Number.NaN))).toBe(false);
    expect(toNorm(Number.NaN, r127)).toBe(0);
  });
});
