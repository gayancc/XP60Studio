// Parameter-Driven Psychoacoustic Estimator (SALE / ELS).
// Bank-wide normalize of loaded User patches using SysEx parameters only — no audio render.
// Tone_ELS = W_base × F_weight × E_sustain × V_level; Patch_ELS = Σ Tone_ELS × (patch_level/127).
// Safe scale: Patch Level primary, proportional Tone Levels secondary. Building blocks frozen.

import { TONE_BLOCKS, type BlockName, type Catalog } from './catalog';
import { blockLayout, docFromBlocks } from './kinds';
import { readParam, type Change } from './tools';
import { patchNameEnergyHint, waveformBaseEnergy } from './waveEnergy';

const EPS = 1e-9;
const DEFAULT_TOLERANCE_DB = 1.0;
const BOOSTER_LINEAR = [1, Math.pow(10, 6 / 20), Math.pow(10, 12 / 20), Math.pow(10, 18 / 20)] as const;

/** Only linear volume parameters may change. Everything else is a building block. */
export const LOUDNESS_ALLOW = new Set(['patch_level', 'tone_level']);

export interface ToneEls {
  block: BlockName;
  index: number;
  on: boolean;
  /** Tone_ELS = W_base × F_weight × E_sustain × V_level × structureBoost */
  els: number;
  /** Intrinsic (no V): W×F×E×boost — used for closed-form gain solve. */
  intrinsic: number;
  wBase: number;
  fWeight: number;
  eSustain: number;
  vLevel: number;
  toneLevel: number;
  waveCategory: string;
}

export interface PatchLoudnessReport {
  name: string;
  patchLevel: number;
  /** Patch_ELS = (sum active Tone_ELS) × (patch_level/127). */
  patchEls: number;
  /** Sum of active tone intrinsics (W×F×E×boost). */
  intrinsicSum: number;
  patchLoudnessDb: number;
  tones: ToneEls[];
  activeToneCount: number;
  toneImbalanceDb: number;
  identity: string;
  toneLevelRatios: number[];
}

export interface LevelEdit {
  block: BlockName;
  id: string;
  from: number;
  to: number;
}

export interface PatchLevelResult {
  bytes: Uint8Array;
  changes: Change[];
  edits: LevelEdit[];
  before: PatchLoudnessReport;
  after: PatchLoudnessReport;
  outlier: boolean;
  outlierReason: string;
  deltaMultiplier: number;
}

export interface SlotLevelResult {
  slot: number;
  name: string;
  result: PatchLevelResult | null;
  skipped: boolean;
  skipReason: string;
}

export interface BankLevelReport {
  targetEls: number;
  targetDb: number;
  toleranceDb: number;
  slots: SlotLevelResult[];
  filled: number;
  changed: number;
  outliers: number;
}

export interface LevelBankOptions {
  toleranceDb?: number;
}

function clamp(v: number, lo: number, hi: number): number {
  return Math.min(hi, Math.max(lo, v));
}

function toDb(linear: number): number {
  return 20 * Math.log10(Math.max(linear, EPS));
}

function median(values: number[]): number {
  if (values.length === 0) return 0;
  const s = [...values].sort((a, b) => a - b);
  const mid = Math.floor(s.length / 2);
  return s.length % 2 ? s[mid]! : (s[mid - 1]! + s[mid]!) / 2;
}

function get(bytes: Uint8Array, catalog: Catalog, block: BlockName, id: string): number {
  const slot = blockLayout(catalog, 'patch').find((s) => s.block === block);
  if (!slot) throw new Error(`missing block ${block}`);
  const p = slot.info.params.find((x) => x.id === id);
  if (!p) throw new Error(`missing param ${block}.${id}`);
  return readParam(bytes, slot.start, p);
}

function toneSwitchOn(catalog: Catalog, bytes: Uint8Array, block: BlockName): boolean {
  return get(bytes, catalog, block, 'tone_switch') === 1;
}

function isEditableLoudnessParam(catalog: Catalog, bytes: Uint8Array, block: BlockName, id: string): boolean {
  if (!LOUDNESS_ALLOW.has(id)) return false;
  if (id === 'patch_level') return true;
  if (block === 'common') return false;
  return toneSwitchOn(catalog, bytes, block);
}

export function identityFingerprint(catalog: Catalog, bytes: Uint8Array): string {
  const parts: string[] = [];
  for (const slot of blockLayout(catalog, 'patch')) {
    for (const p of slot.info.params) {
      if (isEditableLoudnessParam(catalog, bytes, slot.block, p.id)) continue;
      parts.push(`${slot.block}.${p.id}=${readParam(bytes, slot.start, p)}`);
    }
  }
  return parts.join('|');
}

export function toneLevelRatios(catalog: Catalog, bytes: Uint8Array): number[] {
  const levels = TONE_BLOCKS.map((b) => (toneSwitchOn(catalog, bytes, b) ? get(bytes, catalog, b, 'tone_level') : 0));
  const sum = levels.reduce((a, b) => a + b, 0);
  if (sum <= 0) return levels.map(() => 0);
  return levels.map((l) => l / sum);
}

/** Active tones share one scale factor vs original (allows rounding, rejects mix rebuilds). */
export function proportionalToneScaleOk(
  catalog: Catalog,
  original: Uint8Array,
  candidate: Uint8Array,
  eps = 0.08,
): boolean {
  const factors: number[] = [];
  for (const block of TONE_BLOCKS) {
    if (!toneSwitchOn(catalog, original, block)) {
      if (get(original, catalog, block, 'tone_level') !== get(candidate, catalog, block, 'tone_level')) return false;
      continue;
    }
    const a = get(original, catalog, block, 'tone_level');
    const b = get(candidate, catalog, block, 'tone_level');
    if (a <= 0) {
      if (b > 0) return false;
      continue;
    }
    factors.push(b / a);
  }
  if (factors.length === 0) return true;
  const lo = Math.min(...factors);
  const hi = Math.max(...factors);
  return hi <= lo * (1 + eps) + EPS;
}

export function ratiosClose(a: number[], b: number[], eps = 0.03): boolean {
  if (a.length !== b.length) return false;
  for (let i = 0; i < a.length; i++) if (Math.abs(a[i]! - b[i]!) > eps) return false;
  return true;
}

/**
 * F_weight — TVF psychoacoustic weight.
 * Filter type: 0 OFF, 1 LPF, 2 BPF, 3 HPF, 4 PKG.
 * Dark LPF is penalised hard (Fletcher-Munson); bright/resonant paths score higher.
 */
export function tvfWeight(filterType: number, cutoff: number, resonance: number): number {
  if (filterType === 0) return 1.05; // OFF — full wave spectrum
  const c = clamp(cutoff, 0, 127) / 127;
  const r = clamp(resonance, 0, 127) / 127;
  if (filterType === 1) {
    // Steeper dark penalty below ~64 so muted brass/winds are not over-scored.
    const dark = c < 0.5 ? 0.28 + 0.5 * (c / 0.5) : 0.78 + 0.22 * ((c - 0.5) / 0.5);
    return clamp(dark + 0.18 * r, 0.28, 1.2);
  }
  if (filterType === 2) return clamp(0.9 + 0.25 * c + 0.15 * r, 0.7, 1.25);
  if (filterType === 3) return clamp(0.95 + 0.2 * c + 0.15 * r, 0.75, 1.3);
  return clamp(0.85 + 0.3 * r, 0.7, 1.35);
}

/** E_sustain — TVA integral proxy (L3-weighted). */
export function tvaSustainWeight(l1: number, l2: number, l3: number): number {
  const s = clamp(l3, 0, 127) / 127;
  const attack = (clamp(l1, 0, 127) + clamp(l2, 0, 127)) / (2 * 127);
  return clamp(0.22 + 0.58 * s + 0.2 * attack, 0.18, 1.0);
}

export function vLevel(toneLevel: number): number {
  return clamp(toneLevel, 0, 127) / 127;
}

function structureBoost(catalog: Catalog, bytes: Uint8Array, toneIndex: number): number {
  const pair = toneIndex < 2 ? 0 : 1;
  const type = get(bytes, catalog, 'common', pair === 0 ? 'structure_type_1_2' : 'structure_type_3_4') + 1;
  const boostRaw = get(bytes, catalog, 'common', pair === 0 ? 'booster_1_2' : 'booster_3_4');
  const boost = BOOSTER_LINEAR[clamp(boostRaw, 0, 3)] ?? 1;
  // Types 3–4 put the booster in the path; ring types colour energy slightly.
  if (type === 3 || type === 4) return Math.sqrt(boost);
  if (type >= 5) return 1.05;
  return 1;
}

function patchNameOf(bytes: Uint8Array): string {
  const doc = docFromBlocks('patch', bytes);
  try {
    return doc.name().trimEnd();
  } finally {
    doc.free();
  }
}

function toneEls(catalog: Catalog, bytes: Uint8Array, toneIndex: number, patchName: string): ToneEls {
  const block = TONE_BLOCKS[toneIndex]!;
  const on = toneSwitchOn(catalog, bytes, block);
  const toneLevel = get(bytes, catalog, block, 'tone_level');
  if (!on) {
    return {
      block,
      index: toneIndex,
      on: false,
      els: 0,
      intrinsic: 0,
      wBase: 0,
      fWeight: 0,
      eSustain: 0,
      vLevel: 0,
      toneLevel,
      waveCategory: 'inactive',
    };
  }
  const wave = waveformBaseEnergy(
    get(bytes, catalog, block, 'wave_group_type'),
    get(bytes, catalog, block, 'wave_group_id'),
    get(bytes, catalog, block, 'wave_number'),
    patchName,
  );
  const fWeight = tvfWeight(
    get(bytes, catalog, block, 'filter_type'),
    get(bytes, catalog, block, 'cutoff_frequency'),
    get(bytes, catalog, block, 'resonance'),
  );
  const eSustain = tvaSustainWeight(
    get(bytes, catalog, block, 'level_envelope_level_1'),
    get(bytes, catalog, block, 'level_envelope_level_2'),
    get(bytes, catalog, block, 'level_envelope_level_3'),
  );
  const boost = structureBoost(catalog, bytes, toneIndex);
  const v = vLevel(toneLevel);
  const intrinsic = wave.energy * fWeight * eSustain * boost;
  return {
    block,
    index: toneIndex,
    on: true,
    els: intrinsic * v,
    intrinsic,
    wBase: wave.energy,
    fWeight,
    eSustain,
    vLevel: v,
    toneLevel,
    waveCategory: wave.category,
  };
}

export function analyzePatchLoudness(catalog: Catalog, bytes: Uint8Array): PatchLoudnessReport {
  const name = patchNameOf(bytes);
  const tones = TONE_BLOCKS.map((_, i) => toneEls(catalog, bytes, i, name));
  const active = tones.filter((t) => t.on);
  // Power-sum so 4 layered tones do not count as 4× one tone (partially correlated).
  let toneSum = Math.sqrt(active.reduce((a, t) => a + t.els * t.els, 0));
  let intrinsicSum = Math.sqrt(active.reduce((a, t) => a + t.intrinsic * t.intrinsic, 0));
  // Patch-title presence tax: screamers (FLASH SAX, Dist TB) often hide energy in EXP waves /
  // EFX that SysEx wave names under-report. Strong tax so they score above median and get cut.
  const title = patchNameEnergyHint(name);
  if (title && title.energy > 1.0) {
    const tax = 1 + (title.energy - 0.7) * 4; // ~3× for FLASH/Dist titles
    toneSum *= tax;
    intrinsicSum *= tax;
  }
  const patchLevel = get(bytes, catalog, 'common', 'patch_level');
  const patchNorm = patchLevel / 127;
  const patchEls = toneSum * patchNorm;

  let imbalance = 0;
  for (let i = 0; i < active.length; i++) {
    for (let j = i + 1; j < active.length; j++) {
      imbalance = Math.max(imbalance, Math.abs(toDb(active[i]!.els) - toDb(active[j]!.els)));
    }
  }

  return {
    name: name || '(unnamed)',
    patchLevel,
    patchEls,
    intrinsicSum,
    patchLoudnessDb: toDb(Math.max(patchEls, EPS)),
    tones,
    activeToneCount: active.length,
    toneImbalanceDb: active.length < 2 ? 0 : imbalance,
    identity: identityFingerprint(catalog, bytes),
    toneLevelRatios: toneLevelRatios(catalog, bytes),
  };
}

function applyChanges(catalog: Catalog, bytes: Uint8Array, changes: Change[]): Uint8Array {
  const doc = docFromBlocks('patch', bytes);
  try {
    for (const c of changes) doc.set(c.block, c.id, c.value);
    return doc.toBlocks();
  } finally {
    doc.free();
  }
}

function editsFrom(before: Uint8Array, after: Uint8Array, catalog: Catalog): LevelEdit[] {
  const out: LevelEdit[] = [];
  for (const slot of blockLayout(catalog, 'patch')) {
    for (const p of slot.info.params) {
      if (!isEditableLoudnessParam(catalog, before, slot.block, p.id)) continue;
      const a = readParam(before, slot.start, p);
      const b = readParam(after, slot.start, p);
      if (a !== b) out.push({ block: slot.block, id: p.id, from: a, to: b });
    }
  }
  return out;
}

function identityOk(catalog: Catalog, before: Uint8Array, after: Uint8Array): boolean {
  if (identityFingerprint(catalog, before) !== identityFingerprint(catalog, after)) return false;
  return proportionalToneScaleOk(catalog, before, after);
}

/**
 * Closed-form gain solve toward targetEls.
 * 1) Set patch_level so Patch_ELS moves as far as possible.
 * 2) If still short/long, scale all active tone_levels by the same factor (mix lock).
 */
export function matchPatchToTarget(
  catalog: Catalog,
  bytes: Uint8Array,
  targetEls: number,
  toleranceDb = DEFAULT_TOLERANCE_DB,
): PatchLevelResult {
  const before = analyzePatchLoudness(catalog, bytes);
  const delta0 = before.patchEls > EPS ? targetEls / before.patchEls : 1;
  const tolRatio = Math.pow(10, toleranceDb / 20);

  if (before.activeToneCount === 0 || before.patchEls <= EPS || before.intrinsicSum <= EPS) {
    return {
      bytes: bytes.slice(),
      changes: [],
      edits: [],
      before,
      after: before,
      outlier: true,
      outlierReason: 'no active sounding tones to normalize',
      deltaMultiplier: 1,
    };
  }

  // Desired: powerSum(intrinsic_i * V_i) * (P/127) = target.
  // Keep tone ratios: V_i' = k * V_i → tone power-sum scales by k.
  const changes: Change[] = [];
  const curPatch = before.patchLevel;
  const toneSum = before.patchEls / Math.max(curPatch / 127, EPS); // current combined tone energy

  // Ideal patch level if tones stay put: target = toneSum * (P/127)
  let idealPatch = toneSum > EPS ? Math.round((targetEls / toneSum) * 127) : curPatch;
  idealPatch = clamp(idealPatch, 1, 127);

  let working: Uint8Array = bytes.slice();
  if (idealPatch !== curPatch) {
    changes.push({ block: 'common', id: 'patch_level', value: idealPatch });
    working = applyChanges(catalog, working, [{ block: 'common', id: 'patch_level', value: idealPatch }]);
  }

  let mid = analyzePatchLoudness(catalog, working);
  let ratio = mid.patchEls > EPS ? targetEls / mid.patchEls : 1;

  if (ratio > tolRatio || ratio < 1 / tolRatio) {
    // Proportional tone scale (exact common factor, then clamp).
    const active = TONE_BLOCKS.filter((b) => toneSwitchOn(catalog, working, b));
    const levels = active.map((b) => get(working, catalog, b, 'tone_level'));
    if (active.length && levels.every((l) => l > 0)) {
      // Choose k so the largest tone stays in 1..127 when possible.
      let k = ratio;
      const maxL = Math.max(...levels);
      const minL = Math.min(...levels);
      if (maxL * k > 127) k = 127 / maxL;
      if (minL * k < 1) k = Math.max(k, 1 / minL);
      const toneChanges: Change[] = [];
      for (let i = 0; i < active.length; i++) {
        const next = clamp(Math.round(levels[i]! * k), 1, 127);
        if (next !== levels[i]) toneChanges.push({ block: active[i]!, id: 'tone_level', value: next });
      }
      if (toneChanges.length) {
        const candidate = applyChanges(catalog, working, toneChanges);
        if (identityOk(catalog, bytes, candidate)) {
          working = candidate;
          changes.push(...toneChanges);
        }
      }
    }
  }

  // Second polish pass on patch_level after tone move.
  mid = analyzePatchLoudness(catalog, working);
  ratio = mid.patchEls > EPS ? targetEls / mid.patchEls : 1;
  if (ratio > tolRatio || ratio < 1 / tolRatio) {
    const p = get(working, catalog, 'common', 'patch_level');
    let np = clamp(Math.round(p * ratio), 1, 127);
    if (np !== p) {
      const candidate = applyChanges(catalog, working, [{ block: 'common', id: 'patch_level', value: np }]);
      if (identityOk(catalog, bytes, candidate)) {
        working = candidate;
        const existing = changes.find((c) => c.block === 'common' && c.id === 'patch_level');
        if (existing) existing.value = np;
        else changes.push({ block: 'common', id: 'patch_level', value: np });
      }
    }
  }

  if (!identityOk(catalog, bytes, working)) {
    return {
      bytes: bytes.slice(),
      changes: [],
      edits: [],
      before,
      after: before,
      outlier: true,
      outlierReason: 'normalize rejected: would break identity or tone mix ratios',
      deltaMultiplier: delta0,
    };
  }

  const after = analyzePatchLoudness(catalog, working);
  const finalRatio = after.patchEls > EPS ? targetEls / after.patchEls : 1;
  let outlier = false;
  let outlierReason = '';
  if (finalRatio > tolRatio || finalRatio < 1 / tolRatio) {
    outlier = true;
    // Max out remaining headroom one last time for under-target patches.
    if (finalRatio > 1) {
      const maxed = maxGain(catalog, working);
      if (identityOk(catalog, bytes, maxed) && analyzePatchLoudness(catalog, maxed).patchEls > after.patchEls) {
        working = maxed;
      }
    }
    const afterMax = analyzePatchLoudness(catalog, working);
    const r2 = afterMax.patchEls > EPS ? targetEls / afterMax.patchEls : 1;
    if (r2 > tolRatio || r2 < 1 / tolRatio) {
      outlierReason = `could not reach median ELS within ±${toleranceDb} dB without redesigning the patch`;
    } else {
      outlier = false;
    }
  }

  const finalAfter = analyzePatchLoudness(catalog, working);
  const edits = editsFrom(bytes, working, catalog);
  return {
    bytes: working,
    changes: edits.map((e) => ({ block: e.block, id: e.id, value: e.to })),
    edits,
    before,
    after: finalAfter,
    outlier,
    outlierReason,
    deltaMultiplier: delta0,
  };
}

/** Absolute max volume staging (patch + active tones) without touching mix ratios. */
function maxGain(catalog: Catalog, bytes: Uint8Array): Uint8Array {
  const changes: Change[] = [{ block: 'common', id: 'patch_level', value: 127 }];
  const active = TONE_BLOCKS.filter((b) => toneSwitchOn(catalog, bytes, b));
  const levels = active.map((b) => get(bytes, catalog, b, 'tone_level'));
  if (active.length && levels.some((l) => l > 0)) {
    const maxL = Math.max(...levels);
    const k = maxL > 0 ? 127 / maxL : 1;
    for (let i = 0; i < active.length; i++) {
      changes.push({ block: active[i]!, id: 'tone_level', value: clamp(Math.round(levels[i]! * k), 1, 127) });
    }
  }
  return applyChanges(catalog, bytes, changes);
}

export function proposePatchVoiceBalance(catalog: Catalog, bytes: Uint8Array): PatchLevelResult {
  const before = analyzePatchLoudness(catalog, bytes);
  return {
    bytes: bytes.slice(),
    changes: [],
    edits: [],
    before,
    after: before,
    outlier: before.activeToneCount === 0,
    outlierReason: before.activeToneCount === 0 ? 'no active tones' : '',
    deltaMultiplier: 1,
  };
}

export function levelPatch(
  catalog: Catalog,
  bytes: Uint8Array,
  opts: { targetEls?: number; targetDb?: number; toleranceDb?: number } = {},
): PatchLevelResult {
  const before = analyzePatchLoudness(catalog, bytes);
  const target =
    opts.targetEls ??
    (opts.targetDb !== undefined ? Math.pow(10, opts.targetDb / 20) : before.patchEls);
  return matchPatchToTarget(catalog, bytes, target, opts.toleranceDb ?? DEFAULT_TOLERANCE_DB);
}

export function levelBank(
  catalog: Catalog,
  slots: ReadonlyArray<{ bytes: Uint8Array | null; name?: string }>,
  opts: LevelBankOptions = {},
): BankLevelReport {
  const toleranceDb = opts.toleranceDb ?? DEFAULT_TOLERANCE_DB;
  const scored: Array<{ slot: number; name: string; bytes: Uint8Array; report: PatchLoudnessReport }> = [];
  const results: SlotLevelResult[] = [];

  for (let i = 0; i < slots.length; i++) {
    const s = slots[i]!;
    if (!s.bytes) {
      results.push({ slot: i, name: '', result: null, skipped: true, skipReason: 'empty' });
      continue;
    }
    try {
      const report = analyzePatchLoudness(catalog, s.bytes);
      scored.push({ slot: i, name: report.name, bytes: s.bytes, report });
    } catch (e) {
      results.push({
        slot: i,
        name: s.name ?? '',
        result: null,
        skipped: true,
        skipReason: e instanceof Error ? e.message : String(e),
      });
    }
  }

  // Hybrid absolute/capacity target:
  // - Above-median (Dist TB-303, stacked strings): cut to absMedian — never boost hot patches.
  // - Below-median (MariachiTp): lift toward min(absMedian, ~92% of intrinsic capacity).
  const sounding = scored.filter((s) => s.report.intrinsicSum > EPS && s.report.patchEls > EPS);
  const absMedian = median(sounding.map((s) => s.report.patchEls));
  const targetEls = absMedian;

  for (const s of scored) {
    const cap = s.report.intrinsicSum;
    const els = s.report.patchEls;
    let slotTarget = absMedian;
    if (els > absMedian) {
      slotTarget = absMedian;
    } else if (cap > EPS) {
      slotTarget = Math.min(absMedian, Math.max(els, cap * 0.92));
    }
    const matched = matchPatchToTarget(catalog, s.bytes, slotTarget, toleranceDb);
    results.push({ slot: s.slot, name: s.name, result: matched, skipped: false, skipReason: '' });
  }

  results.sort((a, b) => a.slot - b.slot);
  return {
    targetEls,
    targetDb: toDb(Math.max(targetEls, EPS)),
    toleranceDb,
    slots: results,
    filled: scored.length,
    changed: results.filter((r) => (r.result?.edits.length ?? 0) > 0).length,
    outliers: results.filter((r) => r.result?.outlier).length,
  };
}

export function applyBankLevelReport(
  report: BankLevelReport,
  getOriginal: (slot: number) => Uint8Array | null,
  setSlot: (slot: number, bytes: Uint8Array) => void,
): Array<{ slot: number; bytes: Uint8Array }> {
  const undo: Array<{ slot: number; bytes: Uint8Array }> = [];
  for (const s of report.slots) {
    if (!s.result || s.skipped || s.result.edits.length === 0) continue;
    const original = getOriginal(s.slot);
    if (!original) continue;
    undo.push({ slot: s.slot, bytes: original.slice() });
    setSlot(s.slot, s.result.bytes);
  }
  return undo;
}

/** @deprecated Prefer toneLevelRatios. */
export function tvaRatios(catalog: Catalog, bytes: Uint8Array): number[][] {
  return TONE_BLOCKS.map((block) => {
    if (!toneSwitchOn(catalog, bytes, block)) return [0, 0, 0];
    const l1 = get(bytes, catalog, block, 'level_envelope_level_1');
    const l2 = get(bytes, catalog, block, 'level_envelope_level_2');
    const l3 = get(bytes, catalog, block, 'level_envelope_level_3');
    const sum = l1 + l2 + l3;
    if (sum <= 0) return [0, 0, 0];
    return [l1 / sum, l2 / sum, l3 / sum];
  });
}

export const SALE_CONSTANTS = {
  DEFAULT_TOLERANCE_DB,
} as const;
