import { tick } from 'svelte';
import { messages } from '../model/messages.svelte';
import { afterEach, beforeAll, beforeEach, describe, expect, it } from 'vitest';
import { cleanup, fireEvent, render } from '@testing-library/svelte';
import { readFileSync } from 'node:fs';
import { catalogJson, initSync } from '../wasm/pkg/xppatch.js';
import { parseCatalog, type Catalog } from '../model/catalog';
import { SCALE_PRESETS, rawOf } from '../model/scaleTune';
import { PatchStore } from '../model/store.svelte';
import { MemoryTransport } from '../model/transport';
import { ui } from '../model/ui.svelte';
import ScaleHarness from './ScaleHarness.svelte';

let catalog: Catalog;
beforeAll(() => {
  initSync({ module: readFileSync('src/wasm/pkg/xppatch_bg.wasm') });
  catalog = parseCatalog(catalogJson());
});
beforeEach(() => {
  ui.tune = 0;
});
afterEach(cleanup);

const make = (): PatchStore => {
  const s = new PatchStore(catalog, new MemoryTransport(), 0x10, undefined, 'system');
  s.newPatch();
  return s;
};
const ids = (s: PatchStore): string[] => s.catalog.scaleTune.params.map((p) => p.id);
const raws = (s: PatchStore, block: string): number[] => ids(s).map((id) => s.get(block, id));
const button = (c: HTMLElement, text: string | RegExp): HTMLButtonElement =>
  [...c.querySelectorAll('button')].find((b) => (typeof text === 'string' ? b.textContent?.trim() === text : text.test(b.textContent ?? ''))) as HTMLButtonElement;

describe('the Scale tune page', () => {
  it('shows twelve notes with their cents readable, zero for equal temperament', () => {
    const store = make();
    store.set('tune1', ids(store)[1] ?? '', 56); // C# -8
    const { container } = render(ScaleHarness, { props: { store } });
    expect(container.querySelectorAll('.note')).toHaveLength(12);
    const readout = (note: string): string => container.querySelector(`.note[data-note="${note}"] .val`)?.textContent ?? '';
    expect(readout('C#')).toBe('−8');
    expect(readout('C')).toBe('0');
    expect(container.querySelectorAll('[role=slider]').length).toBe(12);
  });

  it('applies a manual preset to the chosen Part as one undo step, and marks it as current', async () => {
    const store = make();
    const { container } = render(ScaleHarness, { props: { store } });
    const before = store.canUndo;
    await fireEvent.click(button(container, 'Pure, tonic C'));
    expect(raws(store, 'tune1')).toEqual(SCALE_PRESETS[1]?.cents.map(rawOf));
    expect(raws(store, 'tune2').every((r) => r === 64), 'other Parts untouched').toBe(true);
    expect(button(container, 'Pure, tonic C').getAttribute('aria-pressed')).toBe('true');
    store.undo();
    expect(raws(store, 'tune1').every((r) => r === 64)).toBe(true);
    expect(before).toBe(false);
  });

  it('resets to equal temperament with the Equal chip', async () => {
    const store = make();
    const { container } = render(ScaleHarness, { props: { store } });
    await fireEvent.click(button(container, 'Arabian'));
    await fireEvent.click(button(container, 'Equal'));
    expect(raws(store, 'tune1').every((r) => r === 64)).toBe(true);
  });

  it('chooses a Part, and Patch mode edits the last scale block', async () => {
    const store = make();
    const { container } = render(ScaleHarness, { props: { store } });
    await fireEvent.click(button(container, '3'));
    await fireEvent.click(button(container, 'Arabian'));
    expect(raws(store, 'tune3')[1]).toBe(rawOf(45));
    expect(raws(store, 'tune1')[1]).toBe(64);
    // the Part button carries a dot now
    expect(container.querySelector('.grid button[aria-label^="Part 3"]')?.getAttribute('aria-label')).toMatch(/not equal temperament/);
    await fireEvent.click(button(container, 'Patch mode'));
    await fireEvent.click(button(container, 'Pure, tonic C'));
    expect(raws(store, 'tune17')[1]).toBe(rawOf(-8));
  });

  it('asks before copying a scale to the other Parts, and copies on confirmation only', async () => {
    const store = make();
    const { container } = render(ScaleHarness, { props: { store } });
    await fireEvent.click(button(container, 'Arabian'));
    await fireEvent.click(button(container, /Copy to the other Parts/));
    expect(raws(store, 'tune2')[1]).toBe(64); // nothing yet
    await fireEvent.click(button(container, 'Cancel'));
    expect(raws(store, 'tune2')[1]).toBe(64);
    await fireEvent.click(button(container, /Copy to the other Parts/));
    await fireEvent.click(button(container, 'Copy'));
    for (const b of ['tune2', 'tune9', 'tune16']) expect(raws(store, b)).toEqual(SCALE_PRESETS[2]?.cents.map(rawOf));
    expect(raws(store, 'tune17')[1], 'Patch mode is not a Part').toBe(64);
  });

  it('copies the Patch-mode scale into a Part after a confirmation', async () => {
    const store = make();
    store.begin('seed');
    ids(store).forEach((id, i) => store.set('tune17', id, rawOf(SCALE_PRESETS[1]?.cents[i] ?? 0)));
    store.end();
    const { container } = render(ScaleHarness, { props: { store } });
    await fireEvent.click(button(container, /Copy from Patch mode/));
    await fireEvent.click(button(container, 'Replace'));
    expect(raws(store, 'tune1')).toEqual(SCALE_PRESETS[1]?.cents.map(rawOf));
  });

  it('says the scale is not used while the switch is off, and still lets values be edited', async () => {
    const store = make();
    const { container, getByRole } = render(ScaleHarness, { props: { store } });
    expect(container.querySelector('.scale')?.classList.contains('off')).toBe(true);
    await fireEvent.click(getByRole('switch', { name: /Scale tune/i }));
    expect(store.get('common', 'scale_tune_switch')).toBe(1);
    expect(container.querySelector('.scale')?.classList.contains('off')).toBe(false);
    messages.clear();
    await fireEvent.click(getByRole('switch', { name: /Scale tune/i }));
    await tick();
    expect(container.querySelector('.scale')?.classList.contains('off')).toBe(true);
    expect(messages.list.map((m) => m.text).join(' | ')).toContain('plays equal temperament');
    expect(container.querySelector('[role=status]')).toBeNull(); // the message is in the corner, not on the page
  });
});

describe('a System page that has not been read from the instrument', () => {
  it('starts the keyboard scale at equal temperament, not -64 cents, and counts as unedited', () => {
    const s = new PatchStore(catalog, new MemoryTransport(), 0x10, undefined, 'system');
    expect(raws(s, 'tune1').every((r) => r === 0), 'a plain new document is all minimum').toBe(true);
    s.newSystemSettings();
    for (const b of ['tune1', 'tune9', 'tune17']) expect(raws(s, b).every((r) => r === 64), b).toBe(true);
    expect(s.canUndo).toBe(false);
    expect(s.origin).toBe('New System settings');
    // everything that is not the scale stays at its minimum
    expect(s.get('common', 'master_tune')).toBe(0);
    const { container } = render(ScaleHarness, { props: { store: s } });
    expect([...container.querySelectorAll('.val')].every((v) => v.textContent === '0')).toBe(true);
  });
});

describe('the keyboard drawing', () => {
  it('has seven white keys and five black keys, each note’s knob placed over or under its own key', () => {
    const { container } = render(ScaleHarness, { props: { store: make() } });
    expect(container.querySelectorAll('.piano .white')).toHaveLength(7);
    expect(container.querySelectorAll('.piano .black')).toHaveLength(5);
    const left = (note: string): number => parseFloat((container.querySelector(`.note[data-note="${note}"]`) as HTMLElement).style.left);
    // naturals are evenly spaced 112 px apart, centred on their white keys
    const naturals = ['C', 'D', 'E', 'F', 'G', 'A', 'B'].map(left);
    naturals.slice(1).forEach((x, i) => expect(x - (naturals[i] ?? 0)).toBe(112));
    expect(naturals[0]).toBe(56);
    // each sharp sits on the line between its two white keys
    expect(left('C#')).toBe(112);
    expect(left('D#')).toBe(224);
    expect(left('F#')).toBe(448);
    expect(left('G#')).toBe(560);
    expect(left('A#')).toBe(672);
  });

  it('tints the key of the note being edited, not outlining it', async () => {
    const { container } = render(ScaleHarness, { props: { store: make() } });
    const note = container.querySelector('.note[data-note="E"]') as HTMLElement;
    await fireEvent.pointerEnter(note);
    expect(container.querySelectorAll('.piano .white.on')).toHaveLength(1);
    await fireEvent.pointerLeave(note);
    expect(container.querySelectorAll('.piano .on')).toHaveLength(0);
  });
});
