// How two Tones are wired together: the ten Structure types of the Owner's Manual (chapter 3: "The Structure parameter determines how Tones 1 and 2 (and 3 and 4)
// are connected"). Each Tone is a chain: wave generator (WG), filter (TVF), amplifier (TVA); a structure joins the two chains. A "B" box is the booster and an "R" box is
// the ring modulator. The wiring below is read from the figures in the manual (the XP-30 manual prints them as vectors on p.70; the XP-60/80 manual has the same ten
// figures). It is the app's own data model, drawn by the app's own diagram.

export type StructureNodeKind = 'wg' | 'tvf' | 'tva' | 'booster' | 'ring' | 'sum';

export interface StructureNode {
  readonly id: string;
  readonly kind: StructureNodeKind;
  /** Tone A (1 or 3), Tone B (2 or 4), or null for a box that belongs to neither. */
  readonly tone: 'A' | 'B' | null;
  /** Position in a 200 by 64 drawing. */
  readonly x: number;
  readonly y: number;
}

export interface Structure {
  readonly type: number;
  readonly nodes: readonly StructureNode[];
  /** Signal flows from the first node to the second. */
  readonly edges: readonly (readonly [string, string])[];
}

type Wiring = Pick<Structure, 'nodes' | 'edges'>;

const ROW_A = 14;
const ROW_B = 50;

const node = (id: string, kind: StructureNodeKind, tone: 'A' | 'B' | null, x: number, y: number): StructureNode => ({ id, kind, tone, x, y });

/** Type 1: two independent chains, one on each row. */
const independent = (): Wiring => ({
  nodes: [
    node('A.wg', 'wg', 'A', 14, ROW_A),
    node('A.tvf', 'tvf', 'A', 100, ROW_A),
    node('A.tva', 'tva', 'A', 176, ROW_A),
    node('B.wg', 'wg', 'B', 14, ROW_B),
    node('B.tvf', 'tvf', 'B', 100, ROW_B),
    node('B.tva', 'tva', 'B', 176, ROW_B),
  ],
  edges: [
    ['A.wg', 'A.tvf'],
    ['A.tvf', 'A.tva'],
    ['B.wg', 'B.tvf'],
    ['B.tvf', 'B.tva'],
  ],
});

interface Join {
  /** What meets at the junction: a plain sum, or the ring modulator. */
  readonly kind: 'sum' | 'ring';
  /** Optional booster and where it sits in the line from the junction to Tone B's filter. */
  readonly booster?: 'before-filter-a' | 'between-filters';
  /** Tone B's wave also goes straight on, past the ring modulator (type 6). */
  readonly dry?: boolean;
}

/**
 * Types 2 to 6. Tone A is wave generator then amplifier. Tone B's wave generator meets it at a junction (a sum, or the ring modulator). What leaves the junction runs
 * through Tone A's filter, then Tone B's filter and amplifier; types 3 and 4 put the booster in that line.
 */
const joinedAfterAmp = (join: Join): Wiring => {
  const nodes: StructureNode[] = [
    node('A.wg', 'wg', 'A', 14, ROW_A),
    node('A.tva', 'tva', 'A', 52, ROW_A),
    node('A.tvf', 'tvf', 'A', 118, ROW_A),
    node('B.wg', 'wg', 'B', 14, ROW_B),
    node('B.tvf', 'tvf', 'B', 150, ROW_B),
    node('B.tva', 'tva', 'B', 184, ROW_B),
    node('J', join.kind, null, 88, 32),
  ];
  const edges: [string, string][] = [
    ['A.wg', 'A.tva'],
    ['A.tva', 'J'],
    ['B.wg', 'J'],
    ['B.tvf', 'B.tva'],
  ];
  if (join.booster === 'before-filter-a') {
    nodes.push(node('K', 'booster', null, 104, 32));
    edges.push(['J', 'K'], ['K', 'A.tvf'], ['A.tvf', 'B.tvf']);
  } else if (join.booster === 'between-filters') {
    nodes.push(node('K', 'booster', null, 150, 32));
    edges.push(['J', 'A.tvf'], ['A.tvf', 'K'], ['K', 'B.tvf']);
  } else {
    edges.push(['J', 'A.tvf'], ['A.tvf', 'B.tvf']);
  }
  if (join.dry) edges.push(['B.wg', 'A.tvf']);
  return { nodes, edges };
};

/** Types 7 and 8: Tone A is a full chain; it and Tone B's wave generator meet at the ring modulator, then run through B's filter and amplifier. */
const ringIntoFilterB = (dry: boolean): Wiring => ({
  nodes: [
    node('A.wg', 'wg', 'A', 14, ROW_A),
    node('A.tvf', 'tvf', 'A', 56, ROW_A),
    node('A.tva', 'tva', 'A', 98, ROW_A),
    node('B.wg', 'wg', 'B', 14, ROW_B),
    node('B.tvf', 'tvf', 'B', 150, ROW_B),
    node('B.tva', 'tva', 'B', 184, ROW_B),
    node('J', 'ring', null, 128, 32),
  ],
  edges: [
    ['A.wg', 'A.tvf'],
    ['A.tvf', 'A.tva'],
    ['A.tva', 'J'],
    ['B.wg', 'J'],
    ['J', 'B.tvf'],
    ['B.tvf', 'B.tva'],
    ...(dry ? ([['B.wg', 'B.tvf']] as [string, string][]) : []),
  ],
});

/** Types 9 and 10: both Tones have wave generator and filter; the ring modulator joins A's full chain with B's filtered wave and feeds B's amplifier. */
const ringIntoAmpB = (dry: boolean): Wiring => ({
  nodes: [
    node('A.wg', 'wg', 'A', 14, ROW_A),
    node('A.tvf', 'tvf', 'A', 56, ROW_A),
    node('A.tva', 'tva', 'A', 98, ROW_A),
    node('B.wg', 'wg', 'B', 14, ROW_B),
    node('B.tvf', 'tvf', 'B', 56, ROW_B),
    node('B.tva', 'tva', 'B', 184, ROW_B),
    node('J', 'ring', null, 138, 32),
  ],
  edges: [
    ['A.wg', 'A.tvf'],
    ['A.tvf', 'A.tva'],
    ['A.tva', 'J'],
    ['B.wg', 'B.tvf'],
    ['B.tvf', 'J'],
    ['J', 'B.tva'],
    ...(dry ? ([['B.tvf', 'B.tva']] as [string, string][]) : []),
  ],
});

function build(type: number): Wiring {
  switch (type) {
    case 1:
      return independent();
    case 2:
      return joinedAfterAmp({ kind: 'sum' });
    case 3:
      return joinedAfterAmp({ kind: 'sum', booster: 'between-filters' });
    case 4:
      return joinedAfterAmp({ kind: 'sum', booster: 'before-filter-a' });
    case 5:
      return joinedAfterAmp({ kind: 'ring' });
    case 6:
      return joinedAfterAmp({ kind: 'ring', dry: true });
    case 7:
      return ringIntoFilterB(false);
    case 8:
      return ringIntoFilterB(true);
    case 9:
      return ringIntoAmpB(false);
    case 10:
      return ringIntoAmpB(true);
    default:
      throw new RangeError(`Structure type ${type} does not exist (1 to 10)`);
  }
}

/** The wiring of one Structure type, 1 to 10. */
export function structureOf(type: number): Structure {
  return { type, ...build(type) };
}

export const STRUCTURE_TYPES: readonly number[] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

/** One line in words, for the hint bar and for assistive technology. */
export function describeStructure(type: number): string {
  switch (type) {
    case 1:
      return 'Independent: each Tone has its own wave generator, filter and amplifier.';
    case 2:
      return 'Tone A through its amplifier is mixed with Tone B’s wave; both filters in series, then Tone B’s amplifier.';
    case 3:
      return 'Like type 2, with a booster between the two filters.';
    case 4:
      return 'Like type 2, with a booster before the first filter.';
    case 5:
      return 'Tone A through its amplifier ring-modulates Tone B’s wave; both filters in series, then Tone B’s amplifier.';
    case 6:
      return 'Like type 5, with Tone B’s wave also passed on unmodulated.';
    case 7:
      return 'Tone A (full chain) ring-modulates Tone B’s wave; then Tone B’s filter and amplifier.';
    case 8:
      return 'Like type 7, with Tone B’s wave also passed on unmodulated.';
    case 9:
      return 'Tone A (full chain) ring-modulates Tone B’s filtered wave; then Tone B’s amplifier.';
    case 10:
      return 'Like type 9, with Tone B’s filtered wave also passed on unmodulated.';
    default:
      return '';
  }
}
