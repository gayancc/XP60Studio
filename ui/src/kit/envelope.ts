// Envelope geometry. Each stage owns an equal horizontal slot; inside its slot the stage's time is a fraction of the
// slot width, so editing one stage's time can never push another stage around.

export interface Stage {
  /** 0 to 127: time to reach `level`. */
  time: number;
  /** 0 to 127. */
  level: number;
}

export interface EnvelopeBox {
  width: number;
  height: number;
  pad: number;
  /** Largest raw value for both axes. */
  max: number;
}

export interface Pt {
  x: number;
  y: number;
}

const clamp = (v: number, lo: number, hi: number): number => Math.min(hi, Math.max(lo, v));

function slot(box: EnvelopeBox, count: number): number {
  return (box.width - box.pad * 2) / Math.max(1, count);
}

/** Pixel positions of the start point and every stage's end point. */
export function layoutPoints(stages: readonly Stage[], startLevel: number, box: EnvelopeBox): Pt[] {
  const w = slot(box, stages.length);
  const inner = box.height - box.pad * 2;
  const y = (level: number): number => box.pad + inner * (1 - clamp(level, 0, box.max) / box.max);
  const pts: Pt[] = [{ x: box.pad, y: y(startLevel) }];
  stages.forEach((s, i) => {
    pts.push({ x: box.pad + w * i + w * (clamp(s.time, 0, box.max) / box.max), y: y(s.level) });
  });
  return pts;
}

/** SVG polyline path through the points. */
export function pathOf(pts: readonly Pt[]): string {
  return pts.map((p, i) => `${i === 0 ? 'M' : 'L'} ${p.x.toFixed(1)} ${p.y.toFixed(1)}`).join(' ');
}

/** Raw time and level for stage `index` when the pointer is at pixel (px, py). Integers, clamped. */
export function stageFromPointer(
  index: number,
  count: number,
  px: number,
  py: number,
  box: EnvelopeBox,
): Stage {
  const w = slot(box, count);
  const inner = box.height - box.pad * 2;
  const time = ((px - box.pad - w * index) / w) * box.max;
  const level = (1 - (py - box.pad) / inner) * box.max;
  return { time: Math.round(clamp(time, 0, box.max)), level: Math.round(clamp(level, 0, box.max)) };
}

/** Index of the stage end point nearest the pointer within `radius`, or -1. The start point is not draggable. */
export function hitStage(pts: readonly Pt[], px: number, py: number, radius: number): number {
  let best = -1;
  let bestD = radius * radius;
  for (let i = 1; i < pts.length; i++) {
    const p = pts[i];
    if (!p) continue;
    const d = (p.x - px) ** 2 + (p.y - py) ** 2;
    if (d <= bestD) {
      bestD = d;
      best = i - 1;
    }
  }
  return best;
}
