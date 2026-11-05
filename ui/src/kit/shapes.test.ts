import { describe, expect, it } from 'vitest';
import { hitStage, layoutPoints, pathOf, stageFromPointer, type EnvelopeBox } from './envelope';
import { isBlackKey, moveEnd, nearerEnd, noteName, shiftRange } from './keyRange';
import { SYNC_ORDER, SYNC_STATES } from './syncState';
import { parseTyped } from './controlMenu';

const box: EnvelopeBox = { width: 220, height: 100, pad: 10, max: 127 };

describe('envelope geometry', () => {
  const stages = [
    { time: 127, level: 127 },
    { time: 0, level: 64 },
    { time: 64, level: 0 },
  ];

  it('lays the start at the left edge and each stage inside its own slot', () => {
    const pts = layoutPoints(stages, 0, box);
    expect(pts).toHaveLength(4);
    expect(pts[0]?.x).toBe(10);
    const slotW = 200 / 3;
    expect(pts[1]?.x).toBeCloseTo(10 + slotW);
    expect(pts[2]?.x).toBeCloseTo(10 + slotW); // time 0 sits at its slot's left edge
    expect(pts[3]?.x).toBeCloseTo(10 + slotW * 2 + slotW * (64 / 127));
  });

  it('puts level 0 at the bottom and the maximum at the top', () => {
    const pts = layoutPoints(stages, 0, box);
    expect(pts[0]?.y).toBe(90);
    expect(pts[1]?.y).toBe(10);
  });

  it('pointer positions invert the layout', () => {
    const pts = layoutPoints(stages, 0, box);
    for (const [i, s] of stages.entries()) {
      const p = pts[i + 1];
      expect(p).toBeDefined();
      const back = stageFromPointer(i, stages.length, p!.x, p!.y, box);
      expect(Math.abs(back.time - s.time)).toBeLessThanOrEqual(1);
      expect(Math.abs(back.level - s.level)).toBeLessThanOrEqual(1);
    }
  });

  it('clamps a pointer far outside the box', () => {
    expect(stageFromPointer(1, 3, -999, -999, box)).toEqual({ time: 0, level: 127 });
    expect(stageFromPointer(1, 3, 9999, 9999, box)).toEqual({ time: 127, level: 0 });
  });

  it('hit-tests stage points but never the fixed start point', () => {
    const pts = layoutPoints(stages, 0, box);
    expect(hitStage(pts, pts[0]!.x, pts[0]!.y, 6)).toBe(-1);
    expect(hitStage(pts, pts[3]!.x + 2, pts[3]!.y - 2, 6)).toBe(2);
    expect(hitStage(pts, 500, 500, 6)).toBe(-1);
  });

  it('draws a path through every point', () => {
    expect(pathOf(layoutPoints(stages, 0, box)).match(/[ML]/g)).toHaveLength(4);
  });
});

describe('key ranges', () => {
  it('names notes with middle C as C4', () => {
    expect(noteName(60)).toBe('C4');
    expect(noteName(0)).toBe('C-1');
    expect(noteName(127)).toBe('G9');
    expect(noteName(61)).toBe('C#4');
    expect(noteName(999)).toBe('G9');
  });
  it('knows the black keys', () => {
    expect([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11].filter(isBlackKey)).toEqual([1, 3, 6, 8, 10]);
  });
  it('lets the ends meet but not cross', () => {
    expect(moveEnd({ low: 40, high: 80 }, 'low', 90)).toEqual({ low: 80, high: 80 });
    expect(moveEnd({ low: 40, high: 80 }, 'high', 10)).toEqual({ low: 40, high: 40 });
    expect(moveEnd({ low: 40, high: 80 }, 'high', 200)).toEqual({ low: 40, high: 127 });
  });
  it('shifts a range without changing its width and stops at the ends', () => {
    expect(shiftRange({ low: 40, high: 60 }, 5)).toEqual({ low: 45, high: 65 });
    expect(shiftRange({ low: 100, high: 120 }, 50)).toEqual({ low: 107, high: 127 });
    expect(shiftRange({ low: 5, high: 25 }, -50)).toEqual({ low: 0, high: 20 });
  });
  it('picks the nearer end', () => {
    expect(nearerEnd({ low: 40, high: 80 }, 45)).toBe('low');
    expect(nearerEnd({ low: 40, high: 80 }, 70)).toBe('high');
  });
});

describe('typed values and sync states', () => {
  it('parses typed text into the range', () => {
    expect(parseTyped(' 64 ', { min: 0, max: 127 })).toBe(64);
    expect(parseTyped('999', { min: 0, max: 127 })).toBe(127);
    expect(parseTyped('-3', { min: 0, max: 127 })).toBe(0);
    expect(parseTyped('abc', { min: 0, max: 127 })).toBeNull();
    expect(parseTyped('', { min: 0, max: 127 })).toBeNull();
    expect(parseTyped('Infinity', { min: 0, max: 127 })).toBeNull();
  });
  it('every sync state has words, a colour and a meaning', () => {
    for (const s of SYNC_ORDER) {
      const info = SYNC_STATES[s];
      expect(info.label.length).toBeGreaterThan(2);
      expect(info.color).toMatch(/^var\(--/);
      expect(info.meaning.length).toBeGreaterThan(10);
    }
    expect(new Set(SYNC_ORDER.map((s) => SYNC_STATES[s].label)).size).toBe(SYNC_ORDER.length);
  });
});

describe('review regressions', () => {
  it('a one-note range can be widened to either side', () => {
    expect(nearerEnd({ low: 60, high: 60 }, 80)).toBe('high');
    expect(nearerEnd({ low: 60, high: 60 }, 40)).toBe('low');
  });
});
