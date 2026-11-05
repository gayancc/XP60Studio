import { afterEach, beforeAll, describe, expect, it } from 'vitest';
import { cleanup, fireEvent, render } from '@testing-library/svelte';
import { tick } from 'svelte';
import { readFileSync } from 'node:fs';
import { catalogJson, initSync } from '../wasm/pkg/xppatch.js';
import { parseCatalog, type Catalog } from '../model/catalog';
import { PatchStore } from '../model/store.svelte';
import { MemoryTransport } from '../model/transport';
import EnvelopeHarness from './EnvelopeHarness.svelte';

let catalog: Catalog;
beforeAll(() => {
  initSync({ module: readFileSync('src/wasm/pkg/xppatch_bg.wasm') });
  catalog = parseCatalog(catalogJson());
});
afterEach(cleanup);

const make = (): PatchStore => {
  const s = new PatchStore(catalog, new MemoryTransport());
  s.newPatch();
  return s;
};
const points = (c: HTMLElement): Array<{ x: number; y: number }> =>
  [...c.querySelectorAll('circle.pt')].map((p) => ({ x: Number(p.getAttribute('cx')), y: Number(p.getAttribute('cy')) }));

describe('the envelope graph and the parameters behind it', () => {
  it('follows the stored times and levels: changing a level or a time moves its point', async () => {
    const store = make();
    const { container } = render(EnvelopeHarness, { props: { store } });
    const before = points(container);
    expect(before).toHaveLength(4);
    store.set('tone1', 'filter_envelope_level_2', 20);
    await tick();
    const afterLevel = points(container);
    expect(afterLevel[1]?.y, 'a lower level draws the point lower').toBeGreaterThan(before[1]?.y ?? 0);
    expect(afterLevel[0]?.y).toBe(before[0]?.y);
    store.set('tone1', 'filter_envelope_time_3', 100);
    await tick();
    const afterTime = points(container);
    expect(afterTime[2]?.x, 'a longer time draws the point further right').toBeGreaterThan(afterLevel[2]?.x ?? 0);
  });

  it('writes back what is dragged or nudged: the point and the stored values stay one thing', async () => {
    const store = make();
    const { container } = render(EnvelopeHarness, { props: { store } });
    const level = store.get('tone1', 'filter_envelope_level_1');
    const point = container.querySelectorAll('circle.pt')[0] as SVGElement;
    await fireEvent.keyDown(point, { key: 'ArrowDown', shiftKey: true });
    expect(store.get('tone1', 'filter_envelope_level_1')).toBe(Math.max(0, level - 10));
    const time = store.get('tone1', 'filter_envelope_time_1');
    await fireEvent.keyDown(point, { key: 'ArrowRight' });
    expect(store.get('tone1', 'filter_envelope_time_1')).toBe(Math.min(127, time + 1));
    // and the drawing already shows it
    expect(point.getAttribute('aria-valuenow')).toBe(String(store.get('tone1', 'filter_envelope_level_1')));
    // undo puts the values and the drawing back
    const moved = points(container)[0];
    store.undo();
    await tick();
    store.undo();
    await tick();
    expect(store.get('tone1', 'filter_envelope_level_1')).toBe(level);
    expect(points(container)[0]).not.toEqual(moved);
  });

  it('does not redraw for the controls beside the graph: depth, velocity and key follow set how the envelope is used, not its shape', async () => {
    const store = make();
    const { container } = render(EnvelopeHarness, { props: { store } });
    const before = points(container);
    for (const id of ['filter_envelope_depth', 'filter_envelope_velocity_sens', 'filter_envelope_velocity_time1', 'filter_envelope_time_keyfollow']) {
      store.set('tone1', id, id === 'filter_envelope_depth' ? 100 : 7);
    }
    await tick();
    expect(points(container)).toEqual(before);
  });
});
