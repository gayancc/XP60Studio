import { describe, expect, it } from 'vitest';
import { routingOf, type RoutingInput, type ToneRouting } from './routing';

const tone = (assign: number, mix = 127, cho = 0, rev = 0): ToneRouting => ({ assign, mixEfxSend: mix, chorusSend: cho, reverbSend: rev });
const base = (over: Partial<RoutingInput> = {}): RoutingInput => ({
  tones: [tone(0), tone(0), tone(0), tone(0)],
  structure: [0, 0],
  efx: { assign: 0, mixSend: 127, chorusSend: 0, reverbSend: 0 },
  chorus: { level: 127, output: 0 },
  reverb: { level: 127 },
  ...over,
});
const has = (r: ReturnType<typeof routingOf>, from: string, to: string): boolean => r.edges.some((e) => e.from === from && e.to === to);

describe('effects routing of a patch', () => {
  it('sends a MIX tone straight to the MIX output', () => {
    const r = routingOf(base());
    expect(has(r, 'tone1', 'mix')).toBe(true);
    expect(r.used.has('efx')).toBe(false);
  });

  it('sends an EFX tone through the EFX, then to the output the EFX Output Assign names', () => {
    const r = routingOf(base({ tones: [tone(1), tone(0), tone(0), tone(0)] }));
    expect(has(r, 'tone1', 'efx')).toBe(true);
    expect(has(r, 'efx', 'mix')).toBe(true);
    const dir = routingOf(base({ tones: [tone(1), tone(0), tone(0), tone(0)], efx: { assign: 1, mixSend: 127, chorusSend: 0, reverbSend: 0 } }));
    expect(has(dir, 'efx', 'dir')).toBe(true);
    expect(has(dir, 'efx', 'mix')).toBe(false);
  });

  it('sends a DIR tone only to the DIRECT OUT, and reports its chorus and reverb sends as having no effect', () => {
    const r = routingOf(base({ tones: [tone(2, 127, 100, 90), tone(0), tone(0), tone(0)] }));
    expect(has(r, 'tone1', 'dir')).toBe(true);
    expect(has(r, 'tone1', 'chorus')).toBe(false);
    expect(has(r, 'tone1', 'reverb')).toBe(false);
    expect(r.inert.map((i) => i.id)).toEqual(['chorus_send_level', 'reverb_send_level']);
    expect(r.inert[0]?.why).toMatch(/DIR/);
  });

  it('a level of 0 sends nothing', () => {
    const r = routingOf(base({ tones: [tone(0, 0), tone(0), tone(0), tone(0)] }));
    expect(has(r, 'tone1', 'mix')).toBe(false);
  });

  it('routes chorus to reverb in series when the chorus output is REVERB', () => {
    const r = routingOf(base({ tones: [tone(0, 127, 100), tone(0), tone(0), tone(0)], chorus: { level: 100, output: 1 } }));
    expect(has(r, 'tone1', 'chorus')).toBe(true);
    expect(has(r, 'chorus', 'reverb')).toBe(true);
    expect(has(r, 'chorus', 'mix')).toBe(false);
    expect(has(r, 'reverb', 'mix')).toBe(true);
  });

  it('feeds chorus and reverb from the EFX only while the EFX goes to MIX', () => {
    const efx = { assign: 0, mixSend: 127, chorusSend: 80, reverbSend: 60 };
    const r = routingOf(base({ tones: [tone(1), tone(0), tone(0), tone(0)], efx }));
    expect(has(r, 'efx', 'chorus')).toBe(true);
    expect(has(r, 'efx', 'reverb')).toBe(true);
    const dir = routingOf(base({ tones: [tone(1), tone(0), tone(0), tone(0)], efx: { ...efx, assign: 1 } }));
    expect(has(dir, 'efx', 'chorus')).toBe(false);
  });

  it('ignores the first Tone of a pair whose Structure Type is 2 to 10', () => {
    const r = routingOf(base({ structure: [3, 0] }));
    expect([...r.ignoredTones]).toEqual(['tone1']);
    expect(has(r, 'tone1', 'mix')).toBe(false);
    expect(has(r, 'tone2', 'mix')).toBe(true);
    expect([...routingOf(base({ structure: [0, 9] })).ignoredTones]).toEqual(['tone3']);
  });

  it('does not route a value the manual does not list', () => {
    const r = routingOf(base({ tones: [tone(3, 127, 100), tone(0), tone(0), tone(0)] }));
    expect(r.edges.some((e) => e.from === 'tone1')).toBe(false);
  });

  it('reports an EFX that is fed but whose output level is 0 as a dead end, with the reason', () => {
    const r = routingOf(base({ tones: [tone(1), tone(1), tone(1), tone(1)], efx: { assign: 0, mixSend: 0, chorusSend: 0, reverbSend: 0 } }));
    expect(has(r, 'efx', 'mix')).toBe(false);
    expect(r.deadEnds.map((d) => d.node)).toEqual(['efx']);
    expect(r.deadEnds[0]?.why).toMatch(/is 0/);
  });

  it('reports no dead end when the EFX passes sound on', () => {
    expect(routingOf(base({ tones: [tone(1), tone(0), tone(0), tone(0)] })).deadEnds).toEqual([]);
  });
});
