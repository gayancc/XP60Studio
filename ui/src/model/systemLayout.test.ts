import { beforeAll, describe, expect, it } from 'vitest';
import { readFileSync } from 'node:fs';
import { catalogJson, initSync } from '../wasm/pkg/xppatch.js';
import { parseCatalog, type Catalog } from './catalog';
import { systemSectionOf, type SystemSection } from './layout';
import { idsOf, SYSTEM_LABELS, SYSTEM_PAGES } from './systemLayout';

let catalog: Catalog;

beforeAll(() => {
  initSync({ module: readFileSync('src/wasm/pkg/xppatch_bg.wasm') });
  catalog = parseCatalog(catalogJson());
});

const sections = Object.keys(SYSTEM_PAGES) as SystemSection[];

describe('the layout of the System tabs', () => {
  it('shows every System parameter exactly once', () => {
    const shown = sections.flatMap((s) => SYSTEM_PAGES[s].flatMap(idsOf));
    expect(new Set(shown).size).toBe(shown.length); // none twice
    const all = catalog.systemCommon.params.map((p) => p.id);
    expect([...shown].sort()).toEqual([...all].sort()); // none missing, none invented
  });

  it('puts each parameter in the tab its section says', () => {
    for (const s of sections) {
      for (const id of SYSTEM_PAGES[s].flatMap(idsOf)) expect(systemSectionOf(id), id).toBe(s);
    }
  });

  it('gives every table rows of equal width, matching its column headings', () => {
    for (const s of sections) {
      for (const block of SYSTEM_PAGES[s]) {
        if (block.kind !== 'table') continue;
        expect(block.rows.length).toBeGreaterThan(0);
        for (const row of block.rows) expect(row.cells, `${s} ${row.label}`).toHaveLength(block.columns.length);
      }
    }
  });

  it('keeps every table row to parameters of its own family, in column order', () => {
    const banks = SYSTEM_PAGES.banks[0];
    expect(banks?.kind).toBe('table');
    if (banks?.kind !== 'table') return;
    expect(banks.rows).toHaveLength(7);
    banks.rows.forEach((row, i) => {
      const n = i + 1;
      expect(row.cells).toEqual([`bank_select_group${n}_switch`, `bank_select_group${n}_msb`, `bank_select_group${n}_lsb`]);
    });
    const pedals = SYSTEM_PAGES.pedals[0];
    if (pedals?.kind !== 'table') throw new Error('pedals is a table');
    // The C inputs have no polarity and the hold pedal has no assignment: those cells are empty, never shifted.
    expect(pedals.rows.find((r) => r.label === 'C1')?.cells[2]).toBeNull();
    expect(pedals.rows.find((r) => r.label === 'Hold pedal')?.cells[0]).toBeNull();
    expect(pedals.rows.find((r) => r.label === 'Hold pedal')?.cells[1]).toBe('hold_pedal_output_mode');
  });

  it('titles every group and table', () => {
    for (const s of sections) for (const b of SYSTEM_PAGES[s]) expect(b.title.trim()).not.toBe('');
  });

  it('has a label for no parameter that does not exist, and none that is empty or too long to fit', () => {
    const all = new Set(catalog.systemCommon.params.map((p) => p.id));
    for (const [id, label] of Object.entries(SYSTEM_LABELS)) {
      expect(all.has(id), id).toBe(true);
      expect(label.trim(), id).not.toBe('');
      expect(label.length, id).toBeLessThanOrEqual(16);
    }
  });

  it('never gives two controls of one group the same label (three switches called Switch)', () => {
    for (const s of sections) {
      for (const b of SYSTEM_PAGES[s]) {
        if (b.kind !== 'group') continue;
        const names = b.ids.map((id) => SYSTEM_LABELS[id] ?? id);
        expect(new Set(names).size, `${s} ${b.title}`).toBe(names.length);
      }
    }
  });

  it('labels every control that sits in a group, so none depends on the automatic label', () => {
    for (const s of sections) {
      for (const b of SYSTEM_PAGES[s]) {
        if (b.kind !== 'group') continue;
        for (const id of b.ids) expect(SYSTEM_LABELS[id], id).toBeDefined();
      }
    }
  });
});
