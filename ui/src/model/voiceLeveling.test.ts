import { beforeAll, describe, expect, it, vi } from 'vitest';
import { readFileSync } from 'node:fs';
import { catalogJson, initSync, PatchDoc } from '../wasm/pkg/xppatch.js';
import { parseCatalog, TONE_BLOCKS, type Catalog } from './catalog';
import { BankService } from './bank.svelte';
import { blockLayout } from './kinds';
import { readParam } from './tools';
import {
  analyzePatchLoudness,
  applyBankLevelReport,
  identityFingerprint,
  levelBank,
  levelPatch,
  LOUDNESS_ALLOW,
  matchPatchToTarget,
  toneLevelRatios,
  tvfWeight,
  tvaSustainWeight,
} from './voiceLeveling';
import { patchNameEnergyHint, waveEnergyFromName, waveformBaseEnergy } from './waveEnergy';
import type { Backend, ArmedBank, BankSaveReport, Progress } from './backend';
import { fakeBulks } from '../test/bulkBackend';

let catalog: Catalog;

beforeAll(() => {
  initSync({ module: readFileSync('src/wasm/pkg/xppatch_bg.wasm') });
  catalog = parseCatalog(catalogJson());
});

function patch(opts: {
  name?: string;
  patchLevel?: number;
  tones?: Array<{
    on?: boolean;
    level?: number;
    cutoff?: number;
    filterType?: number;
    resonance?: number;
    l1?: number;
    l2?: number;
    l3?: number;
    wave?: number;
  }>;
}): Uint8Array {
  const d = new PatchDoc();
  d.setName(opts.name ?? 'Test');
  d.set('common', 'patch_level', opts.patchLevel ?? 100);
  const tones = opts.tones ?? [{ on: true, level: 100 }];
  for (let i = 0; i < 4; i++) {
    const t = tones[i] ?? { on: false, level: 100 };
    const block = TONE_BLOCKS[i]!;
    d.set(block, 'tone_switch', t.on === false ? 0 : 1);
    d.set(block, 'tone_level', t.level ?? 100);
    d.set(block, 'wave_group_type', 0);
    d.set(block, 'wave_group_id', 1);
    d.set(block, 'wave_number', t.wave ?? 0);
    d.set(block, 'filter_type', t.filterType ?? 1);
    d.set(block, 'cutoff_frequency', t.cutoff ?? 100);
    d.set(block, 'resonance', t.resonance ?? 0);
    d.set(block, 'level_envelope_level_1', t.l1 ?? 100);
    d.set(block, 'level_envelope_level_2', t.l2 ?? 100);
    d.set(block, 'level_envelope_level_3', t.l3 ?? 100);
    d.set(block, 'keyboard_range_lower', 0);
    d.set(block, 'keyboard_range_upper', 127);
    d.set(block, 'velocity_range_lower', 1);
    d.set(block, 'velocity_range_upper', 127);
  }
  const bytes = d.toBlocks();
  d.free();
  return bytes;
}

function onlyVolumeDiffer(a: Uint8Array, b: Uint8Array): boolean {
  for (const slot of blockLayout(catalog, 'patch')) {
    for (const p of slot.info.params) {
      const va = readParam(a, slot.start, p);
      const vb = readParam(b, slot.start, p);
      if (va === vb) continue;
      if (!LOUDNESS_ALLOW.has(p.id)) return false;
      if (p.id === 'tone_level') {
        const sw = slot.info.params.find((x) => x.id === 'tone_switch')!;
        if (readParam(a, slot.start, sw) !== 1) return false;
      }
    }
  }
  return true;
}

function withinDb(a: number, b: number, db: number): boolean {
  const ratio = a > 0 && b > 0 ? a / b : 0;
  return ratio >= 1 / Math.pow(10, db / 20) && ratio <= Math.pow(10, db / 20);
}

describe('wave energy LUT', () => {
  it('scores acid/dist higher than mariachi/acoustic brass', () => {
    expect(waveEnergyFromName('Dist TB-303').energy).toBeGreaterThan(waveEnergyFromName('Mariachi Tp').energy);
    expect(patchNameEnergyHint('FLASH SAX xp')!.energy).toBeGreaterThan(patchNameEnergyHint('MariachiTp 2')!.energy);
  });

  it('uses patch title when the wave name is missing (EXP)', () => {
    const w = waveformBaseEnergy(2, 1, 0, 'FLASH SAX xp');
    expect(w.category).toMatch(/flash|sax/i);
    expect(w.energy).toBeGreaterThan(0.9);
  });
});

describe('ELS components', () => {
  it('penalises dark LPF more than open LPF', () => {
    expect(tvfWeight(1, 20, 0)).toBeLessThan(tvfWeight(1, 120, 0));
  });

  it('gives higher sustain weight to high L3', () => {
    expect(tvaSustainWeight(127, 127, 127)).toBeGreaterThan(tvaSustainWeight(127, 64, 10));
  });
});

describe('ELS analyze', () => {
  it('scores only active tones into Patch_ELS', () => {
    const bytes = patch({
      patchLevel: 100,
      tones: [
        { on: true, level: 100, cutoff: 120, l3: 120 },
        { on: false, level: 127, cutoff: 127, l3: 127 },
        { on: false },
        { on: false },
      ],
    });
    const r = analyzePatchLoudness(catalog, bytes);
    expect(r.activeToneCount).toBe(1);
    expect(r.tones[1]!.els).toBe(0);
    expect(r.patchEls).toBeCloseTo(r.tones[0]!.els * (100 / 127), 5);
  });

  it('rates Dist TB-303 style louder than MariachiTp style at equal levels', () => {
    const mariachi = patch({
      name: 'MariachiTp 2',
      patchLevel: 100,
      tones: [{ on: true, level: 100, filterType: 1, cutoff: 70, l3: 80 }],
    });
    const dist = patch({
      name: 'Dist TB-303',
      patchLevel: 100,
      tones: [{ on: true, level: 100, filterType: 1, cutoff: 120, resonance: 60, l3: 100 }],
    });
    expect(analyzePatchLoudness(catalog, dist).patchEls).toBeGreaterThan(
      analyzePatchLoudness(catalog, mariachi).patchEls * 1.15,
    );
  });
});

describe('safe scaling', () => {
  it('matches quiet toward loud with closed-form patch+tone moves and locked ratios', () => {
    const quiet = patch({
      name: 'MariachiTp 2',
      patchLevel: 40,
      tones: [
        { on: true, level: 60, cutoff: 55, l3: 50 },
        { on: true, level: 30, cutoff: 55, l3: 50 },
        { on: false, level: 33 },
        { on: false },
      ],
    });
    const loudEls = analyzePatchLoudness(
      catalog,
      patch({
        name: 'Pad',
        patchLevel: 110,
        tones: [
          { on: true, level: 100, cutoff: 100, l3: 100 },
          { on: true, level: 50, cutoff: 100, l3: 100 },
          { on: false, level: 33 },
          { on: false },
        ],
      }),
    ).patchEls;

    const result = matchPatchToTarget(catalog, quiet, loudEls, 1.0);
    expect(identityFingerprint(catalog, quiet)).toBe(identityFingerprint(catalog, result.bytes));
    expect(onlyVolumeDiffer(quiet, result.bytes)).toBe(true);
    expect(result.after.tones[2]!.toneLevel).toBe(33);
    // Mix 2:1 preserved among active tones
    expect(result.after.tones[0]!.toneLevel / result.after.tones[1]!.toneLevel).toBeCloseTo(2, 0);
    expect(withinDb(result.after.patchEls, loudEls, 1.05) || result.outlier).toBe(true);
  });

  it('cuts Dist TB-303 style down toward a lower target', () => {
    const hot = patch({
      name: 'Dist TB-303',
      patchLevel: 127,
      tones: [{ on: true, level: 127, cutoff: 127, resonance: 80, l3: 120 }],
    });
    const before = analyzePatchLoudness(catalog, hot);
    const target = before.patchEls * 0.45;
    const result = matchPatchToTarget(catalog, hot, target, 1.0);
    expect(result.after.patchEls).toBeLessThan(before.patchEls * 0.7);
    expect(withinDb(result.after.patchEls, target, 1.05)).toBe(true);
    expect(onlyVolumeDiffer(hot, result.bytes)).toBe(true);
  });
});

describe('Normalize bank pipeline', () => {
  it('cuts Dist/Flash absolute ELS and lifts Mariachi toward the median', () => {
    const slots = [
      {
        bytes: patch({
          name: 'MariachiTp 2',
          patchLevel: 50,
          tones: [{ on: true, level: 55, cutoff: 50, l3: 40 }],
        }),
      },
      {
        bytes: patch({
          name: 'Dist TB-303',
          patchLevel: 120,
          tones: [{ on: true, level: 120, cutoff: 127, resonance: 70, l3: 110 }],
        }),
      },
      {
        bytes: patch({
          name: 'FLASH SAX xp',
          patchLevel: 115,
          tones: [{ on: true, level: 110, cutoff: 120, resonance: 40, l3: 100 }],
        }),
      },
      {
        bytes: patch({
          name: 'Ref Pad',
          patchLevel: 90,
          tones: [{ on: true, level: 90, cutoff: 100, l3: 100 }],
        }),
      },
      { bytes: null },
    ];
    const els0 = (bytes: Uint8Array) => analyzePatchLoudness(catalog, bytes).patchEls;
    const m0 = els0(slots[0]!.bytes!);
    const d0 = els0(slots[1]!.bytes!);
    const f0 = els0(slots[2]!.bytes!);
    expect(d0).toBeGreaterThan(m0);

    const report = levelBank(catalog, slots, { toleranceDb: 1.0 });
    const byName = Object.fromEntries(report.slots.filter((s) => s.result).map((s) => [s.name, s.result!]));
    expect(byName['Dist TB-303']!.after.patchEls).toBeLessThan(d0 * 0.95);
    expect(byName['FLASH SAX xp']!.after.patchEls).toBeLessThanOrEqual(f0 * 1.01);
    expect(byName['MariachiTp 2']!.after.patchEls).toBeGreaterThan(m0 * 0.98);
    // Dist must not stay far above the bank median after cut.
    expect(byName['Dist TB-303']!.after.patchEls).toBeLessThanOrEqual(report.targetEls * 1.15);
  });

  it('applyBankLevelReport returns originals for undo', () => {
    const original = patch({ name: 'X', patchLevel: 20, tones: [{ on: true, level: 50 }] });
    const report = levelBank(catalog, [
      { bytes: original },
      { bytes: patch({ name: 'Y', patchLevel: 120, tones: [{ on: true, level: 110, cutoff: 127, l3: 127 }] }) },
    ]);
    const store = new Map<number, Uint8Array>([[0, original.slice()]]);
    applyBankLevelReport(
      report,
      (slot) => store.get(slot) ?? null,
      (slot, bytes) => store.set(slot, bytes.slice()),
    );
    if (report.slots[0]?.result?.edits.length) {
      expect(store.get(0)).toEqual(report.slots[0]!.result!.bytes);
    }
  });
});

describe('BankService Normalize bank', () => {
  function fakeBackend(): Backend {
    const never = (): never => {
      throw new Error('not expected');
    };
    return {
      ...fakeBulks().api,
      libraryImport: vi.fn(never),
      libraryList: vi.fn(never),
      libraryCount: vi.fn(never),
      libraryTags: vi.fn(never),
      libraryGet: vi.fn(never),
      libraryAdd: vi.fn(never),
      libraryFavourite: vi.fn(never),
      libraryTagAdd: vi.fn(never),
      libraryTagRemove: vi.fn(never),
      libraryRename: vi.fn(never),
      libraryDelete: vi.fn(never),
      libraryExport: vi.fn(never),
      backupsList: vi.fn(never),
      backupRead: vi.fn(never),
      backupAll: vi.fn(never),
      storageInfo: vi.fn(never),
      storageSet: vi.fn(never),
      armSave: vi.fn(never),
      disarmSave: vi.fn(never),
      commitSave: vi.fn(never),
      restoreSave: vi.fn(never),
      armSystemSave: vi.fn(never),
      disarmSystemSave: vi.fn(never),
      commitSystemSave: vi.fn(never),
      restoreSystemSave: vi.fn(never),
      saveFile: vi.fn(never),
      readBank: vi.fn(async () => []),
      armBank: vi.fn(async (): Promise<ArmedBank> => ({ backup_path: '', names: [] })),
      disarmBank: vi.fn(async () => undefined),
      commitBank: vi.fn(async (): Promise<BankSaveReport> => ({ written: [], stopped_at: null, cancelled: false })),
      bankToSyx: vi.fn(async () => new Uint8Array([0xf0, 0xf7])),
      cancelOperation: vi.fn(async () => undefined),
      onProgress: vi.fn(async (_listener: (p: Progress) => void) => () => undefined),
    };
  }

  it('previews, applies, and undoes in memory', () => {
    const bank = new BankService(fakeBackend());
    bank.assign(0, patch({ name: 'MariachiTp 2', patchLevel: 35, tones: [{ on: true, level: 50, cutoff: 45, l3: 35 }] }), 'file');
    bank.assign(1, patch({ name: 'Dist TB-303', patchLevel: 120, tones: [{ on: true, level: 120, cutoff: 127, resonance: 60, l3: 110 }] }), 'file');
    const preview = bank.previewLevelBank(catalog);
    expect(preview!.filled).toBe(2);
    const before0 = bank.slots[0]!.bytes!.slice();
    expect(bank.applyLevelBank()).toBe(true);
    expect(identityFingerprint(catalog, before0)).toBe(identityFingerprint(catalog, bank.slots[0]!.bytes!));
    expect(bank.undoLevelBank()).toBe(true);
    expect(bank.slots[0]!.bytes).toEqual(before0);
  });
});

describe('levelPatch API', () => {
  it('normalizes toward an explicit target ELS', () => {
    const bytes = patch({ patchLevel: 50, tones: [{ on: true, level: 80 }] });
    const target = analyzePatchLoudness(catalog, bytes).patchEls * 1.4;
    const result = levelPatch(catalog, bytes, { targetEls: target });
    expect(onlyVolumeDiffer(bytes, result.bytes)).toBe(true);
  });
});
