import { afterEach, beforeAll, beforeEach, describe, expect, it, vi } from 'vitest';
import { readFileSync } from 'node:fs';
import { catalogJson, initSync } from '../wasm/pkg/xppatch.js';
import { parseCatalog, type Catalog } from './catalog';
import { changedParams, sameBytes, splitFrames } from './frames';
import { PatchStore } from './store.svelte';
import { MemoryTransport } from './transport';

let catalog: Catalog;
const BANK = new Uint8Array(readFileSync('../tests/golden/xp60-user-bank.syx'));

beforeAll(() => {
  initSync({ module: readFileSync('src/wasm/pkg/xppatch_bg.wasm') });
  catalog = parseCatalog(catalogJson());
});

beforeEach(() => vi.useFakeTimers());
afterEach(() => vi.useRealTimers());

function make(): { store: PatchStore; transport: MemoryTransport } {
  const transport = new MemoryTransport();
  const store = new PatchStore(catalog, transport, 0x10, { now: () => Date.now() });
  return { store, transport };
}


/** Put the whole patch on the (fake) instrument first, so later edits are single-parameter frames. */
function prime(store: PatchStore, transport: MemoryTransport): void {
  store.sendPatch();
  vi.advanceTimersByTime(1000);
  transport.frames.length = 0;
}

describe('frames helpers', () => {
  it('splits concatenated frames on F7 and drops a dangling tail', () => {
    const bytes = new Uint8Array([0xf0, 1, 0xf7, 0xf0, 2, 3, 0xf7, 0xf0, 9]);
    expect(splitFrames(bytes).map((f) => [...f])).toEqual([
      [0xf0, 1, 0xf7],
      [0xf0, 2, 3, 0xf7],
    ]);
  });
  it('finds exactly the parameters whose bytes differ', () => {
    const { store } = make();
    const before = store.snapshot();
    store.set('tone3', 'resonance', 40);
    store.set('common', 'patch_level', 7);
    const changed = changedParams(catalog, 'patch', before, store.snapshot());
    expect(changed).toEqual([
      { block: 'common', id: 'patch_level' },
      { block: 'tone3', id: 'resonance' },
    ]);
    expect(sameBytes(before, before)).toBe(true);
  });
});

describe('editing', () => {
  it('reads, writes and formats through the Rust codec', () => {
    const { store } = make();
    store.set('tone2', 'lfo1_waveform', 2);
    expect(store.get('tone2', 'lfo1_waveform')).toBe(2);
    expect(store.format('tone2', 'lfo1_waveform')).toBe('SAW');
    expect(store.get('tone1', 'lfo1_waveform')).toBe(0);
    expect(store.sync).toBe('edited');
  });

  it('refuses an out-of-range value, keeps the old one, and reports why', () => {
    const { store } = make();
    store.set('tone1', 'tone_level', 500);
    expect(store.get('tone1', 'tone_level')).toBe(0);
    expect(store.error).not.toBe('');
    expect(store.items).toHaveLength(0);
  });

  it('a group is one undo step however many values change', () => {
    const { store } = make();
    store.begin('Tone 1 · Cutoff Frequency');
    for (let v = 1; v <= 30; v++) store.set('tone1', 'cutoff_frequency', v);
    store.end();
    expect(store.items.map((i) => i.label)).toEqual(['Tone 1 · Cutoff Frequency']);
    store.undo();
    expect(store.get('tone1', 'cutoff_frequency')).toBe(0);
    store.redo();
    expect(store.get('tone1', 'cutoff_frequency')).toBe(30);
  });

  it('a group that changes nothing records nothing', () => {
    const { store } = make();
    store.begin('x');
    store.end();
    store.begin('x');
    store.set('tone1', 'cutoff_frequency', 0);
    store.end();
    expect(store.items).toHaveLength(0);
  });

  it('undo, redo and jump restore exact states and update the position', () => {
    const { store } = make();
    store.set('common', 'patch_level', 10);
    store.set('common', 'patch_level', 20);
    store.set('common', 'patch_level', 30);
    expect(store.position).toBe(3);
    store.jumpTo(1);
    expect(store.get('common', 'patch_level')).toBe(10);
    store.jumpTo(0);
    expect(store.get('common', 'patch_level')).toBe(0);
    store.jumpTo(3);
    expect(store.get('common', 'patch_level')).toBe(30);
    expect(store.canUndo).toBe(true);
    expect(store.canRedo).toBe(false);
  });

  it('a new edit after undo drops the redo tail', () => {
    const { store } = make();
    store.set('common', 'patch_level', 10);
    store.set('common', 'patch_level', 20);
    store.undo();
    store.set('common', 'patch_level', 55);
    expect(store.items).toHaveLength(2);
    expect(store.canRedo).toBe(false);
  });

  it('renames the patch, and rejects a name that is too long', () => {
    const { store } = make();
    store.setName('Warm Pad');
    expect(store.name).toBe('Warm Pad');
    store.setName('this name is much too long');
    expect(store.name).toBe('Warm Pad');
    expect(store.error).not.toBe('');
    store.undo();
    expect(store.name.trim()).toBe('');
  });
});

describe('live edit pipeline', () => {
  it('a drag sends only the newest value, as one DT1 to the temporary patch', () => {
    const { store, transport } = make();
    prime(store, transport);
    store.begin('drag');
    for (let v = 1; v <= 40; v++) store.set('tone4', 'tone_level', v);
    store.end();
    vi.advanceTimersByTime(500);
    expect(transport.frames).toHaveLength(1);
    const f = transport.frames[0] as Uint8Array;
    expect([...f.slice(0, 5)]).toEqual([0xf0, 0x41, 0x10, 0x6a, 0x12]);
    expect([...f.slice(5, 9)]).toEqual([0x03, 0x00, 0x16, 0x65]);
    expect(f[9]).toBe(40);
    expect(store.sync).toBe('sent');
  });

  it('different parameters are paced at least 20 ms apart', () => {
    const { store, transport } = make();
    prime(store, transport);
    const times: number[] = [];
    const orig = transport.send.bind(transport);
    transport.send = (f) => {
      times.push(Date.now());
      orig(f);
    };
    store.set('tone1', 'tone_level', 1);
    store.set('tone1', 'tone_pan', 2);
    store.set('tone1', 'resonance', 3);
    vi.advanceTimersByTime(500);
    expect(transport.frames).toHaveLength(3);
    expect(times[1]! - times[0]!).toBeGreaterThanOrEqual(20);
    expect(times[2]! - times[1]!).toBeGreaterThanOrEqual(20);
  });

  it('with live edit off nothing is sent, and turning it on sends the whole patch', () => {
    const { store, transport } = make();
    store.setLive(false);
    store.set('tone1', 'tone_level', 9);
    vi.advanceTimersByTime(500);
    expect(transport.frames).toHaveLength(0);
    expect(store.sync).toBe('edited');
    store.setLive(true);
    vi.advanceTimersByTime(1000);
    expect(transport.frames).toHaveLength(9); // Common + four Tones, each tone split 128 + 1
    expect(store.sync).toBe('sent');
  });

  it('undo sends exactly the parameters it changed back', () => {
    const { store, transport } = make();
    prime(store, transport);
    store.set('tone1', 'tone_level', 50);
    vi.advanceTimersByTime(100);
    const before = transport.frames.length;
    store.undo();
    vi.advanceTimersByTime(100);
    expect(transport.frames.length - before).toBe(1);
    expect(transport.frames.at(-1)?.[9]).toBe(0);
  });
});

describe('opening patches', () => {
  it('opens a slot of a real bank file and starts a fresh history', () => {
    const { store } = make();
    store.set('common', 'patch_level', 5);
    const names = PatchStore.listSyx(BANK);
    expect(names).toHaveLength(128);
    store.loadSyx(BANK, 9, 'bank.syx');
    expect(store.name).toBe(names[9]?.[1]);
    expect(store.origin).toBe('bank.syx, slot 10');
    expect(store.items).toHaveLength(0);
    expect(store.sync).toBe('local');
  });

  it('a bad file or slot reports an error and keeps the open patch', () => {
    const { store } = make();
    store.setName('Keep Me');
    store.loadSyx(BANK.slice(0, 50), 0, 'broken.syx');
    expect(store.error).not.toBe('');
    expect(store.name).toBe('Keep Me');
    store.loadSyx(BANK, 200, 'bank.syx');
    expect(store.name).toBe('Keep Me');
  });

  it('new patch clears the history and replaces any pending live frames with the new sound', () => {
    const { store, transport } = make();
    store.set('tone1', 'tone_level', 1);
    store.set('tone1', 'tone_pan', 2);
    transport.frames.length = 0;
    store.newPatch();
    vi.advanceTimersByTime(1000);
    expect(store.items).toHaveLength(0);
    // Live edit is on in tests: New is sent whole so the instrument matches the neutral sound.
    expect(transport.frames.length).toBeGreaterThanOrEqual(9);
  });

  it('new patch is the neutral sound: it plays (full ranges, first tone on) yet counts as unedited', () => {
    const { store } = make();
    store.newPatch();
    expect(store.get('tone1', 'tone_switch')).toBe(1);
    expect(store.get('tone1', 'keyboard_range_upper')).toBe(127);
    expect(store.get('tone4', 'velocity_range_upper')).toBe(127);
    expect(store.items).toHaveLength(0);
    expect(store.canUndo).toBe(false);
  });

  it('XP-60 Init loads the panel-captured defaults exactly and one Undo restores the prior sound', () => {
    const { store } = make();
    store.setName('Before Init');
    const before = store.snapshot();
    store.initXp60Patch();
    const panel = readFileSync('../crates/xppatch-core/data/xp60-init-patch.blocks');
    expect(Buffer.from(store.snapshot())).toEqual(panel);
    expect(store.name.trimEnd()).toBe('INIT PATCH');
    expect(store.items.at(-1)?.label).toBe('XP-60 Init Patch');
    store.undo();
    expect(Buffer.from(store.snapshot())).toEqual(Buffer.from(before));
  });
});

describe('the instrument', () => {
  it('a patch read from the instrument opens as verified with a fresh history', () => {
    const { store } = make();
    store.set('tone1', 'tone_level', 9);
    const other = make().store;
    other.set('tone2', 'resonance', 70);
    store.openFromInstrument(other.snapshot());
    expect(store.sync).toBe('tempVerified');
    expect(store.get('tone2', 'resonance')).toBe(70);
    expect(store.items).toHaveLength(0);
    expect(store.origin).toContain('Instrument');
  });

  it('a wrong-sized read keeps the open patch and reports an error', () => {
    const { store } = make();
    store.setName('Keep');
    store.openFromInstrument(new Uint8Array(10));
    expect(store.error).not.toBe('');
    expect(store.name.trimEnd()).toBe('Keep');
  });

  it('read-back comparison decides between verified and mismatch', () => {
    const { store } = make();
    store.set('tone1', 'tone_level', 9);
    const same = store.snapshot();
    expect(store.verifyAgainst(same)).toBe(true);
    expect(store.sync).toBe('tempVerified');
    const different = same.slice();
    different[73 + 101] = 10;
    expect(store.verifyAgainst(different)).toBe(false);
    expect(store.sync).toBe('mismatch');
  });

  it('a panel change marks only a sent or verified sound as stale', () => {
    const { store } = make();
    store.markStale();
    expect(store.sync).toBe('local');
    store.set('tone1', 'tone_level', 3);
    vi.advanceTimersByTime(200);
    expect(store.sync).toBe('sent');
    store.markStale();
    expect(store.sync).toBe('stale');
  });

  it('auditioning a bank slot does not claim the open patch was fully sent', () => {
    const { store, transport } = make();
    store.set('tone1', 'tone_level', 9);
    vi.advanceTimersByTime(2000);
    expect(store.sync).toBe('sent');
    transport.frames.length = 0;
    const other = make().store;
    other.setName('Bank Pad');
    other.set('tone2', 'resonance', 77);
    store.auditionBlocks(other.snapshot());
    vi.advanceTimersByTime(5000);
    expect(store.sync).toBe('stale');
    expect(transport.frames.length).toBeGreaterThan(1);
    transport.frames.length = 0;
    store.set('tone1', 'tone_level', 11);
    vi.advanceTimersByTime(200);
    // Must be a whole send of the open patch, not one parameter on top of the auditioned slot.
    expect(transport.frames.length).toBeGreaterThan(1);
  });
});

describe('honest sync state', () => {
  it('the first live edit sends the whole patch, then only single parameters', () => {
    const { store, transport } = make();
    store.set('tone1', 'tone_level', 5);
    vi.advanceTimersByTime(1000);
    expect(transport.frames).toHaveLength(9);
    expect(store.sync).toBe('sent');
    transport.frames.length = 0;
    store.set('tone1', 'tone_level', 6);
    vi.advanceTimersByTime(100);
    expect(transport.frames).toHaveLength(1);
  });

  it('an EFX type change is sent as a whole patch with its twelve defaults', () => {
    const { store, transport } = make();
    store.set('tone1', 'tone_level', 5);
    vi.advanceTimersByTime(1000);
    transport.frames.length = 0;
    const before = store.get('common', 'efx_type');
    const next = before === 0 ? 1 : 0;
    store.applyMany(
      [
        { block: 'common', id: 'efx_type', value: next },
        ...Array.from({ length: 12 }, (_, i) => ({ block: 'common' as const, id: `efx_parameter_${i + 1}`, value: 10 + i })),
      ],
      'Effects · Type',
    );
    vi.advanceTimersByTime(1000);
    expect(transport.frames.length).toBeGreaterThanOrEqual(9);
    expect(store.get('common', 'efx_type')).toBe(next);
  });

  it('a wave group change is sent as a whole patch so type/id never land alone', () => {
    const { store, transport } = make();
    store.set('tone1', 'tone_level', 5);
    vi.advanceTimersByTime(1000);
    expect(store.sync).toBe('sent');
    transport.frames.length = 0;
    store.applyMany(
      [
        { block: 'tone1', id: 'wave_group_type', value: 2 },
        { block: 'tone1', id: 'wave_group_id', value: 9 },
        { block: 'tone1', id: 'wave_number', value: 0 },
      ],
      'Tone 1 · Wave',
    );
    vi.advanceTimersByTime(1000);
    expect(transport.frames.length).toBeGreaterThanOrEqual(9);
    expect(store.get('tone1', 'wave_group_type')).toBe(2);
    expect(store.get('tone1', 'wave_group_id')).toBe(9);
    expect(store.get('tone1', 'wave_number')).toBe(0);
  });

  it('wave type/id/number set inside one undo group only go out once, after end()', () => {
    const { store, transport } = make();
    prime(store, transport);
    store.begin('Tone 1 · Wave');
    store.set('tone1', 'wave_group_type', 2);
    store.set('tone1', 'wave_group_id', 9);
    store.set('tone1', 'wave_number', 0);
    expect(transport.frames).toHaveLength(0);
    store.end();
    vi.advanceTimersByTime(1000);
    expect(transport.frames.length).toBeGreaterThanOrEqual(9);
  });

  it('opening a file with Live edit on sends the sound immediately, before any Tone click', () => {
    const { store, transport } = make();
    store.loadSyx(BANK, 0, 'bank.syx');
    vi.advanceTimersByTime(1000);
    expect(transport.frames.length).toBeGreaterThanOrEqual(9);
    expect(store.sync).toBe('sent');
    transport.frames.length = 0;
    const was = store.get('tone2', 'tone_switch');
    store.set('tone2', 'tone_switch', was === 1 ? 0 : 1);
    vi.advanceTimersByTime(100);
    // Base is already on the instrument: a Tone switch is a single live frame, not another whole send.
    expect(transport.frames).toHaveLength(1);
    expect(store.get('tone2', 'tone_switch')).toBe(was === 1 ? 0 : 1);
  });

  it('fetching from the instrument does not send the same sound straight back', () => {
    const { store, transport } = make();
    store.loadSyx(BANK, 0, 'bank.syx');
    vi.advanceTimersByTime(1000);
    const bytes = store.snapshot();
    transport.frames.length = 0;
    store.openFromInstrument(bytes);
    vi.advanceTimersByTime(500);
    expect(transport.frames).toHaveLength(0);
    expect(store.sync).toBe('tempVerified');
  });

  it('a sound changed on the panel is stale, and the next edit resends the whole patch', () => {
    const { store, transport } = make();
    prime(store, transport);
    store.set('tone1', 'tone_level', 5);
    vi.advanceTimersByTime(100);
    store.markStale();
    expect(store.sync).toBe('stale');
    transport.frames.length = 0;
    store.set('tone1', 'tone_level', 6);
    vi.advanceTimersByTime(1000);
    expect(transport.frames).toHaveLength(9);
    expect(store.sync).toBe('sent');
  });

  it('a send that keeps failing is never reported as sent', () => {
    const transport = new MemoryTransport();
    transport.send = () => {
      throw new Error('port gone');
    };
    const store = new PatchStore(catalog, transport, 0x10, { now: () => Date.now() });
    store.set('tone1', 'tone_level', 5);
    vi.advanceTimersByTime(5000);
    expect(store.queue.state).toBe('failed');
    expect(store.queue.lost).toBeGreaterThan(0);
    expect(store.sync).toBe('edited');
    expect(store.error).not.toBe('');
  });

  it('turning live edit off drops what was waiting and stops claiming SENT', () => {
    const { store, transport } = make();
    prime(store, transport);
    store.set('tone1', 'tone_level', 5);
    vi.advanceTimersByTime(100);
    expect(store.sync).toBe('sent');
    store.set('tone1', 'tone_pan', 9);
    store.set('tone1', 'resonance', 9);
    store.setLive(false);
    transport.frames.length = 0;
    vi.advanceTimersByTime(1000);
    expect(transport.frames).toHaveLength(0);
    expect(store.sync).toBe('edited');
  });

  it('disconnecting clears waiting frames and the sent claim', () => {
    const { store, transport } = make();
    prime(store, transport);
    store.set('tone1', 'tone_level', 5);
    vi.advanceTimersByTime(100);
    store.connectionChanged(false);
    expect(store.sync).toBe('edited');
    expect(store.queue.pending).toBe(0);
  });

  it('redo while a group is open closes the group first', () => {
    const { store } = make();
    store.set('common', 'patch_level', 10);
    store.undo();
    store.begin('open group');
    store.redo();
    expect(store.get('common', 'patch_level')).toBe(10);
    expect(store.items).toHaveLength(1);
  });

  it('retyping the same name changes nothing and sends nothing', () => {
    const { store, transport } = make();
    store.setName('Warm Pad');
    prime(store, transport);
    store.setName('Warm Pad');
    store.setName('Warm Pad  ');
    vi.advanceTimersByTime(500);
    expect(transport.frames).toHaveLength(0);
  });

  it('only the changed characters of a rename are sent', () => {
    const { store, transport } = make();
    store.setName('Warm Pad');
    prime(store, transport);
    store.setName('Warm Pam');
    vi.advanceTimersByTime(500);
    expect(transport.frames).toHaveLength(1);
  });

  it('whenQueueIdle resolves once frames have drained', async () => {
    const { store } = make();
    store.set('tone1', 'tone_level', 5);
    let done = false;
    const idle = store.whenQueueIdle().then(() => (done = true));
    expect(done).toBe(false);
    vi.advanceTimersByTime(1000);
    await idle;
    expect(done).toBe(true);
  });
});

describe('review fixes for several document kinds', () => {
  it('fetching system settings is not an XP TEMP state and cannot go stale', () => {
    const transport = new MemoryTransport();
    const store = new PatchStore(catalog, transport, 0x10, { now: () => Date.now() }, 'system');
    const bytes = new PatchStore(catalog, transport, 0x10, { now: () => Date.now() }, 'system').snapshot();
    store.openFromInstrument(bytes);
    expect(store.sync).toBe('local');
    store.markStale();
    expect(store.sync).toBe('local');
  });

  it('the rhythm set name is never sent live (its temporary address is unconfirmed)', () => {
    const transport = new MemoryTransport();
    const store = new PatchStore(catalog, transport, 0x10, { now: () => Date.now() }, 'rhythm');
    store.sendPatch();
    vi.advanceTimersByTime(5000);
    transport.frames.length = 0;
    store.setName('Kit One');
    vi.advanceTimersByTime(1000);
    expect(transport.frames).toHaveLength(0);
    expect(store.sync).toBe('edited');
    expect(store.items[0]?.label).toBe('Rhythm set · Name');
  });
});

describe('sound-design tools in the store', () => {
  it('applyMany is one undo step, and a large batch is sent as a whole patch', () => {
    const { store, transport } = make();
    store.sendPatch();
    vi.advanceTimersByTime(2000);
    transport.frames.length = 0;
    const changes = Array.from({ length: 40 }, (_, i) => ({ block: 'tone1', id: `controller_1_depth_${(i % 4) + 1}`, value: 10 + i }));
    // Only four distinct parameters: the last value for each wins.
    store.applyMany(changes, 'Many');
    expect(store.items.map((i) => i.label)).toEqual(['Many']);
    store.undo();
    expect(store.get('tone1', 'controller_1_depth_1')).toBe(0);
    store.redo();
    expect(store.get('tone1', 'controller_1_depth_4')).toBe(10 + 39);
    const wide = ['cutoff_frequency', 'resonance', 'tone_level', 'tone_pan', 'coarse_tune', 'fine_tune']
      .flatMap((id, i) => [{ block: 'tone2', id, value: 20 + i }, { block: 'tone3', id, value: 30 + i }, { block: 'tone4', id, value: 40 + i }, { block: 'tone1', id, value: 50 + i }]);
    const many = [
      ...wide,
      ...Array.from({ length: 20 }, (_, i) => ({ block: 'tone1', id: `lfo1_rate`, value: i })),
    ];
    transport.frames.length = 0;
    store.applyMany(many, 'Wide');
    vi.advanceTimersByTime(3000);
    expect(transport.frames.length).toBeGreaterThanOrEqual(9); // whole-patch frames, not 44 single ones
  });

  it('an empty batch does nothing; an invalid value is reported and skipped', () => {
    const { store } = make();
    store.applyMany([], 'Nothing');
    expect(store.items).toHaveLength(0);
    store.applyMany([{ block: 'tone1', id: 'tone_level', value: 500 }, { block: 'tone1', id: 'tone_pan', value: 9 }], 'Mixed');
    expect(store.get('tone1', 'tone_pan')).toBe(9);
    expect(store.get('tone1', 'tone_level')).toBe(0);
    expect(store.error).not.toBe('');
  });

  it('A/B switches between two versions, undoably', () => {
    const { store } = make();
    store.set('tone1', 'tone_level', 50);
    store.abSwitch(); // B starts as a copy of A, then we edit B
    expect(store.ab.active).toBe('b');
    expect(store.get('tone1', 'tone_level')).toBe(50);
    store.set('tone1', 'tone_level', 100);
    store.abSwitch(); // back to A
    expect(store.ab.active).toBe('a');
    expect(store.get('tone1', 'tone_level')).toBe(50);
    store.abSwitch();
    expect(store.get('tone1', 'tone_level')).toBe(100);
    store.undo();
    expect(store.get('tone1', 'tone_level')).toBe(50);
  });

  it('morph blends A and B and returns exactly to either end', () => {
    const { store } = make();
    expect(store.canMorph).toBe(false);
    store.set('tone1', 'tone_level', 20);
    store.abSwitch();
    store.set('tone1', 'tone_level', 120);
    expect(store.canMorph).toBe(true);
    store.begin('Morph drag');
    store.morphTo(0.5);
    expect(store.get('tone1', 'tone_level')).toBe(70);
    store.morphTo(0.25);
    expect(store.get('tone1', 'tone_level')).toBe(45);
    store.morphTo(1);
    expect(store.get('tone1', 'tone_level')).toBe(120);
    store.morphTo(0);
    expect(store.get('tone1', 'tone_level')).toBe(20);
    store.endMorph();
    expect(store.items.filter((i) => i.label === 'Morph drag')).toHaveLength(1);
  });

  it('Compare lists what changed since the sound was opened, and clears on open', () => {
    const { store } = make();
    expect(store.changesSinceOpened).toEqual([]);
    store.set('tone2', 'resonance', 33);
    const d = store.changesSinceOpened;
    expect(d.map((e) => [e.block, e.id, e.a, e.b])).toEqual([['tone2', 'resonance', 0, 33]]);
    store.newPatch();
    expect(store.changesSinceOpened).toEqual([]);
    expect(store.ab.a).toBeNull();
  });
});
