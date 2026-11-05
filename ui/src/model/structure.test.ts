import { describe, expect, it } from 'vitest';
import { describeStructure, STRUCTURE_TYPES, structureLegend, structureOf, structureRole } from './structure';

const kinds = (type: number, kind: string): number => structureOf(type).nodes.filter((n) => n.kind === kind).length;

describe('the ten Structure types', () => {
  it('has types 1 to 10 and refuses any other', () => {
    expect(STRUCTURE_TYPES).toEqual([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
    expect(() => structureOf(0)).toThrow(RangeError);
    expect(() => structureOf(11)).toThrow(RangeError);
  });

  it('gives every type a wave generator for each Tone and ends in an amplifier', () => {
    for (const t of STRUCTURE_TYPES) {
      const s = structureOf(t);
      const ids = new Set(s.nodes.map((n) => n.id));
      expect(ids.has('A.wg'), `type ${t}`).toBe(true);
      expect(ids.has('B.wg'), `type ${t}`).toBe(true);
      expect(ids.has('B.tva'), `type ${t}`).toBe(true);
      // every edge joins two boxes that exist
      for (const [a, b] of s.edges) {
        expect(ids.has(a), `type ${t}: ${a}`).toBe(true);
        expect(ids.has(b), `type ${t}: ${b}`).toBe(true);
      }
    }
  });

  it('puts a booster only in types 3 and 4 and a ring modulator only in types 5 to 10, as the manual draws them', () => {
    for (const t of STRUCTURE_TYPES) {
      expect(kinds(t, 'booster'), `booster in type ${t}`).toBe(t === 3 || t === 4 ? 1 : 0);
      expect(kinds(t, 'ring'), `ring modulator in type ${t}`).toBe(t >= 5 ? 1 : 0);
    }
  });

  it('makes type 1 two independent chains and every other type one joined path to Tone B’s amplifier', () => {
    const one = structureOf(1);
    expect(one.nodes.filter((n) => n.kind === 'tva').length).toBe(2);
    expect(one.edges).toHaveLength(4);
    for (const t of STRUCTURE_TYPES.filter((x) => x > 1)) {
      const s = structureOf(t);
      // Tone A's own chain feeds the junction: in types 2 to 6 only through its amplifier, in 7 to 10 through the whole chain
      expect(s.edges.some(([a, b]) => a === 'A.tva' && b === 'J'), `type ${t}`).toBe(true);
      expect(s.edges.some(([a]) => a === 'B.tva'), `type ${t}: B's amplifier is the end`).toBe(false);
    }
  });

  it('has no loops: signal only moves forward', () => {
    for (const t of STRUCTURE_TYPES) {
      const s = structureOf(t);
      const next = new Map<string, string[]>();
      for (const [a, b] of s.edges) next.set(a, [...(next.get(a) ?? []), b]);
      const visiting = new Set<string>();
      const done = new Set<string>();
      const visit = (id: string): void => {
        expect(visiting.has(id), `type ${t} loops at ${id}`).toBe(false);
        if (done.has(id)) return;
        visiting.add(id);
        for (const to of next.get(id) ?? []) visit(to);
        visiting.delete(id);
        done.add(id);
      };
      for (const nd of s.nodes) visit(nd.id);
    }
  });

  it('marks the bypass of types 6, 8 and 10 as an extra path, not a different box', () => {
    expect(structureOf(6).edges.length).toBe(structureOf(5).edges.length + 1);
    expect(structureOf(8).edges.length).toBe(structureOf(7).edges.length + 1);
    expect(structureOf(10).edges.length).toBe(structureOf(9).edges.length + 1);
  });

  it('describes every type in words and keeps boxes inside the drawing', () => {
    for (const t of STRUCTURE_TYPES) {
      expect(describeStructure(t).length).toBeGreaterThan(10);
      for (const n of structureOf(t).nodes) {
        expect(n.x).toBeGreaterThanOrEqual(0);
        expect(n.x).toBeLessThanOrEqual(200);
        expect(n.y).toBeGreaterThanOrEqual(0);
        expect(n.y).toBeLessThanOrEqual(64);
      }
    }
  });

  it('explains every type, and says what the first Tone’s amplifier does where the manual says it does something', () => {
    for (const t of STRUCTURE_TYPES) expect(structureRole(t).length, `type ${t}`).toBeGreaterThan(30);
    for (const t of [2, 4, 5, 6, 9, 10]) expect(structureRole(t), `type ${t}`).toMatch(/first Tone’s amplifier/);
    expect(structureRole(1)).not.toMatch(/amplifier sets/);
    expect(structureRole(11)).toBe('');
  });

  it('lists the parts a type is drawn with', () => {
    expect(structureLegend(1)).toBe('WG wave generator · TVF filter · TVA amplifier');
    expect(structureLegend(3)).toContain('B booster');
    expect(structureLegend(3)).not.toContain('ring');
    expect(structureLegend(7)).toContain('R ring modulator');
    expect(structureLegend(2)).not.toContain('booster');
  });
});
