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

  // The manual's own worked examples (p.62-64), each as its Output Assign, send levels and effect settings, with the connections its figure draws.
  describe("the manual's routing examples", () => {
    const edges = (r: ReturnType<typeof routingOf>): string[] => r.edges.map((e) => `${e.from}>${e.to}`).sort();
    const T1 = (assign: number, mix: number, cho: number, rev: number): RoutingInput['tones'] => [tone(assign, mix, cho, rev), tone(0, 0), tone(0, 0), tone(0, 0)];

    it('sending unprocessed direct sound to the MIX OUTPUT jack', () => {
      expect(edges(routingOf(base({ tones: T1(0, 127, 0, 0) })))).toEqual(['tone1>mix']);
    });
    it('connecting reverb and chorus in series (chorus output REV)', () => {
      const r = routingOf(base({ tones: T1(0, 127, 127, 0), chorus: { level: 127, output: 1 }, reverb: { level: 127 } }));
      expect(edges(r)).toEqual(['chorus>reverb', 'reverb>mix', 'tone1>chorus', 'tone1>mix']);
    });
    it('using the EFX only', () => {
      const r = routingOf(base({ tones: T1(1, 127, 0, 0), efx: { assign: 0, mixSend: 127, chorusSend: 0, reverbSend: 0 } }));
      expect(edges(r)).toEqual(['efx>mix', 'tone1>efx']);
    });
    it('applying reverb to only the chorus sound, in parallel with the EFX', () => {
      const r = routingOf(base({ tones: T1(1, 127, 127, 0), efx: { assign: 0, mixSend: 127, chorusSend: 0, reverbSend: 0 }, chorus: { level: 127, output: 1 } }));
      expect(edges(r)).toEqual(['chorus>reverb', 'efx>mix', 'reverb>mix', 'tone1>chorus', 'tone1>efx']);
    });
    it('connecting the EFX, chorus and reverb in series', () => {
      const r = routingOf(base({ tones: T1(1, 127, 0, 0), efx: { assign: 0, mixSend: 127, chorusSend: 127, reverbSend: 127 }, chorus: { level: 127, output: 1 } }));
      expect(edges(r)).toEqual(['chorus>reverb', 'efx>chorus', 'efx>mix', 'efx>reverb', 'reverb>mix', 'tone1>efx']);
    });
    it('connecting chorus and reverb in parallel after the EFX (chorus output MIX)', () => {
      const r = routingOf(base({ tones: T1(1, 127, 0, 0), efx: { assign: 0, mixSend: 127, chorusSend: 127, reverbSend: 127 }, chorus: { level: 127, output: 0 } }));
      expect(edges(r)).toEqual(['chorus>mix', 'efx>chorus', 'efx>mix', 'efx>reverb', 'reverb>mix', 'tone1>efx']);
    });
    it('a Tone on DIR draws a single line to the DIRECT OUT jack (p.61 figure)', () => {
      const r = routingOf(base({ tones: T1(2, 127, 127, 127) }));
      expect(edges(r)).toEqual(['tone1>dir']);
      expect(r.tones[0]?.chorus.state).toBe('inert');
      expect(r.tones[0]?.reverb.state).toBe('inert');
    });
  });

  describe('the per-Tone route indicator', () => {
    it('reports each of the three send lines of a Tone: on, off, inert or ignored', () => {
      const r = routingOf(base({ tones: [tone(1, 127, 51, 0), tone(2, 100, 80, 0), tone(0, 0, 0, 0), tone(0)], structure: [0, 0] }));
      expect(r.tones[0]?.main).toMatchObject({ to: 'efx', state: 'on', level: 127 });
      expect(r.tones[0]?.chorus.state).toBe('on');
      expect(r.tones[0]?.reverb.state).toBe('off');
      expect(r.tones[1]?.main).toMatchObject({ to: 'dir', state: 'on' });
      expect(r.tones[1]?.chorus.state).toBe('inert');
      expect(r.tones[2]?.main.state).toBe('off');
    });
    it('marks the first Tone of a combined pair as ignored on every line', () => {
      const r = routingOf(base({ structure: [2, 0] }));
      expect([r.tones[0]?.main.state, r.tones[0]?.chorus.state, r.tones[0]?.reverb.state]).toEqual(['ignored', 'ignored', 'ignored']);
    });
  });
});
