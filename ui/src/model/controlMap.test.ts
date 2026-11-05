import { describe, expect, it } from 'vitest';
import { bind, describe as describeBinding, find, hears, parseBindings, scaleValue, serializeBindings, unbind, type Binding } from './controlMap';

const cutoff: Binding = { kind: 'patch', block: 'tone1', id: 'cutoff_frequency', channel: null, source: 'cc', controller: 74 };

describe('controller map', () => {
  it('scales 0..127 over a range, ends included, rounding to a whole step', () => {
    expect(scaleValue(0, 0, 127)).toBe(0);
    expect(scaleValue(127, 0, 127)).toBe(127);
    expect(scaleValue(64, 0, 127)).toBe(64);
    expect(scaleValue(0, 1, 100)).toBe(1);
    expect(scaleValue(127, 1, 100)).toBe(100);
    expect(scaleValue(127, -50, 50)).toBe(50);
    expect(scaleValue(0, -50, 50)).toBe(-50);
    expect(scaleValue(200, 0, 10)).toBe(10);
    expect(scaleValue(-5, 0, 10)).toBe(0);
  });

  it('hears a controller on its channel, or on any channel when none is set', () => {
    expect(hears(cutoff, 5, 'cc', 74)).toBe(true);
    expect(hears(cutoff, 5, 'cc', 75)).toBe(false);
    expect(hears({ ...cutoff, channel: 2 }, 2, 'cc', 74)).toBe(true);
    expect(hears({ ...cutoff, channel: 2 }, 3, 'cc', 74)).toBe(false);
  });

  it('gives a parameter one controller and a controller one parameter', () => {
    let map = bind([], cutoff);
    map = bind(map, { ...cutoff, controller: 71 }); // the same parameter moves to another controller
    expect(map).toHaveLength(1);
    expect(map[0]?.controller).toBe(71);
    map = bind(map, { ...cutoff, id: 'resonance', controller: 71 }); // a different parameter on the same controller takes it over
    expect(map).toHaveLength(1);
    expect(map[0]?.id).toBe('resonance');
    // The same controller number on a different channel is a different source.
    map = bind(map, { ...cutoff, id: 'tone_level', channel: 1, controller: 71 });
    expect(map).toHaveLength(2);
    // Another kind of document is independent.
    map = bind(map, { kind: 'performance', block: 'part1', id: 'part_level', channel: null, source: 'cc', controller: 71 });
    expect(map).toHaveLength(3);
  });

  it('never binds Bank Select', () => {
    expect(bind([], { ...cutoff, controller: 0 })).toEqual([]);
    expect(bind([], { ...cutoff, controller: 32 })).toEqual([]);
  });

  it('finds and removes by target', () => {
    const map = bind([], cutoff);
    expect(find(map, cutoff)).toEqual(cutoff);
    expect(unbind(map, cutoff)).toEqual([]);
    expect(unbind(map, { kind: 'patch', block: 'tone2', id: 'cutoff_frequency' })).toEqual(map);
  });

  it('names a binding for the menu', () => {
    expect(describeBinding(cutoff)).toBe('CC 74');
    expect(describeBinding({ ...cutoff, channel: 2 })).toBe('CC 74 ch 3');
  });

  it('round-trips through storage and drops damaged entries', () => {
    const map = bind(bind([], cutoff), { ...cutoff, id: 'resonance', controller: 71, channel: 4 });
    expect(parseBindings(serializeBindings(map))).toEqual(map);
    expect(parseBindings(null)).toEqual([]);
    expect(parseBindings('not json')).toEqual([]);
    expect(parseBindings('{"a":1}')).toEqual([]);
    const mixed = JSON.stringify([cutoff, { kind: 'nope', block: 'x', id: 'y', channel: null, source: 'cc', controller: 1 }, { ...cutoff, id: 'z', controller: 200 }, { ...cutoff, id: 'w', controller: 32 }, 7, null]);
    expect(parseBindings(mixed)).toEqual([cutoff]);
  });

  it('scales a 14-bit NRPN value over the parameter range', () => {
    expect(scaleValue(0, 0, 127, 16383)).toBe(0);
    expect(scaleValue(16383, 0, 127, 16383)).toBe(127);
    expect(scaleValue(8192, 0, 100, 16383)).toBe(50);
    expect(scaleValue(99999, 0, 10, 16383)).toBe(10);
  });

  it('keeps CC, NRPN and RPN sources apart even with the same number', () => {
    const nrpn: Binding = { ...cutoff, id: 'resonance', source: 'nrpn', controller: 74 };
    expect(hears(nrpn, 0, 'nrpn', 74)).toBe(true);
    expect(hears(nrpn, 0, 'cc', 74)).toBe(false);
    expect(hears(nrpn, 0, 'rpn', 74)).toBe(false);
    const map = bind(bind([], cutoff), nrpn);
    expect(map).toHaveLength(2);
    expect(describeBinding(nrpn)).toBe('NRPN 74');
    expect(describeBinding({ ...nrpn, source: 'rpn', channel: 1 })).toBe('RPN 74 ch 2');
  });

  it('accepts NRPN 0 and large numbers but not a CC above 127, and reads old maps as CC', () => {
    const big: Binding = { ...cutoff, source: 'nrpn', controller: 16383 };
    const zero: Binding = { ...cutoff, id: 'a', source: 'nrpn', controller: 0 };
    expect(parseBindings(JSON.stringify([big, zero]))).toEqual([big, zero]);
    expect(parseBindings(JSON.stringify([{ ...cutoff, controller: 200 }]))).toEqual([]);
    const old = { kind: 'patch', block: 'tone1', id: 'cutoff_frequency', channel: null, controller: 74 };
    expect(parseBindings(JSON.stringify([old]))).toEqual([cutoff]);
  });
});
