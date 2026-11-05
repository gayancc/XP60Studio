// Effects routing of a Patch, worked out from the stored values the way the Owner's Manual describes it (pp.60-64, "Routing effects" and "Effects routing setting
// examples"). Pure data in, a small graph out: which Tone feeds which stage, and which stages are in use. Nothing here is written to the instrument.
//
// What the manual states, and this module follows:
//  - Each Tone's Output Assign is MIX (to the MIX OUTPUT jacks), EFX (through the EFX) or DIR (to the DIRECT OUT jack only). A level of 0 sends nothing.
//  - With DIR, the Chorus and Reverb sends have no effect ("If DIR is selected for the Output parameter, this cannot be set", p.75; "have no effect", p.61).
//  - The sound that passed through the EFX goes to MIX or DIR (EFX Output Assign). Its Chorus and Reverb sends apply only when it goes to MIX; with DIR they have no
//    effect (p.61).
//  - Chorus Output Assign: MIX, REVERB or MIX+REV (p.63, p.64).
//  - Structure Type 2 to 10 on a pair of Tones combines their outputs into the second Tone of the pair; the first Tone's output settings are ignored (p.60).
// What it does not know: the [EFX], [CHORUS] and [REVERB] panel switches (System settings, which turn each effect on or off), so a stage is reported as in use
// by its settings alone.

export type ToneOutput = 'MIX' | 'EFX' | 'DIR' | 'other';
export type EfxOutput = 'MIX' | 'DIR' | 'other';
export type ChorusOutput = 'MIX' | 'REV' | 'MIX+REV';

export interface ToneRouting {
  /** Output Assign as stored: 0 MIX, 1 EFX, 2 DIR, anything else is a value the manual does not list. */
  readonly assign: number;
  readonly mixEfxSend: number;
  readonly chorusSend: number;
  readonly reverbSend: number;
}

export interface RoutingInput {
  readonly tones: readonly [ToneRouting, ToneRouting, ToneRouting, ToneRouting];
  /** Structure Type of Tones 1 and 2, and of 3 and 4, as stored (0 = Type 1). */
  readonly structure: readonly [number, number];
  readonly efx: { readonly assign: number; readonly mixSend: number; readonly chorusSend: number; readonly reverbSend: number };
  readonly chorus: { readonly level: number; readonly output: number };
  readonly reverb: { readonly level: number };
}

export type NodeId = 'tone1' | 'tone2' | 'tone3' | 'tone4' | 'efx' | 'chorus' | 'reverb' | 'mix' | 'dir';

export interface Edge {
  readonly from: NodeId;
  readonly to: NodeId;
  /** How much is sent along it, 1 to 127 (the send or level that feeds it). */
  readonly level: number;
}

export interface Routing {
  /** The connections that carry sound with the current settings. */
  readonly edges: readonly Edge[];
  /** Stages that receive sound from something. */
  readonly used: ReadonlySet<NodeId>;
  /** Tones whose output settings the instrument ignores (Structure Type 2 to 10 on the first Tone of a pair). */
  readonly ignoredTones: ReadonlySet<NodeId>;
  /** Stages that receive sound but pass none on, with the reason (for example an output level of 0). */
  readonly deadEnds: ReadonlyArray<{ readonly node: NodeId; readonly why: string }>;
  /** Parameters that currently have no effect, by Tone index (0 to 3) and parameter id, each with the manual's reason. */
  readonly inert: ReadonlyArray<{ readonly tone: number; readonly id: string; readonly why: string }>;
}

const TONES: readonly NodeId[] = ['tone1', 'tone2', 'tone3', 'tone4'];

export function toneOutputOf(assign: number): ToneOutput {
  return assign === 0 ? 'MIX' : assign === 1 ? 'EFX' : assign === 2 ? 'DIR' : 'other';
}

export function efxOutputOf(assign: number): EfxOutput {
  return assign === 0 ? 'MIX' : assign === 1 ? 'DIR' : 'other';
}

export function chorusOutputOf(output: number): ChorusOutput {
  return output === 1 ? 'REV' : output === 2 ? 'MIX+REV' : 'MIX';
}

/** Is the first Tone of pair 0 (Tones 1 and 2) or pair 1 (Tones 3 and 4) ignored because the pair's Structure Type is 2 to 10? */
export const firstToneIgnored = (structureType: number): boolean => structureType > 0;

export function routingOf(input: RoutingInput): Routing {
  const edges: Edge[] = [];
  const inert: { tone: number; id: string; why: string }[] = [];
  const ignored = new Set<NodeId>();
  if (firstToneIgnored(input.structure[0])) ignored.add('tone1');
  if (firstToneIgnored(input.structure[1])) ignored.add('tone3');

  input.tones.forEach((t, i) => {
    const node = TONES[i] as NodeId;
    if (ignored.has(node)) return;
    const out = toneOutputOf(t.assign);
    if (t.mixEfxSend > 0) {
      const to: NodeId | undefined = out === 'MIX' ? 'mix' : out === 'EFX' ? 'efx' : out === 'DIR' ? 'dir' : undefined;
      if (to) edges.push({ from: node, to, level: t.mixEfxSend });
    }
    if (out === 'DIR') {
      if (t.chorusSend > 0) inert.push({ tone: i, id: 'chorus_send_level', why: 'Output Assign is DIR: only the direct sound is sent to the DIRECT OUT jack, so chorus has no effect.' });
      if (t.reverbSend > 0) inert.push({ tone: i, id: 'reverb_send_level', why: 'Output Assign is DIR: only the direct sound is sent to the DIRECT OUT jack, so reverb has no effect.' });
    } else if (out !== 'other') {
      if (t.chorusSend > 0) edges.push({ from: node, to: 'chorus', level: t.chorusSend });
      if (t.reverbSend > 0) edges.push({ from: node, to: 'reverb', level: t.reverbSend });
    }
  });

  const efxFed = edges.some((e) => e.to === 'efx');
  if (efxFed && input.efx.mixSend > 0) {
    const out = efxOutputOf(input.efx.assign);
    if (out === 'MIX') edges.push({ from: 'efx', to: 'mix', level: input.efx.mixSend });
    else if (out === 'DIR') edges.push({ from: 'efx', to: 'dir', level: input.efx.mixSend });
    if (out === 'MIX') {
      if (input.efx.chorusSend > 0) edges.push({ from: 'efx', to: 'chorus', level: input.efx.chorusSend });
      if (input.efx.reverbSend > 0) edges.push({ from: 'efx', to: 'reverb', level: input.efx.reverbSend });
    }
  }

  const chorusFed = edges.some((e) => e.to === 'chorus');
  if (chorusFed && input.chorus.level > 0) {
    const out = chorusOutputOf(input.chorus.output);
    if (out === 'MIX' || out === 'MIX+REV') edges.push({ from: 'chorus', to: 'mix', level: input.chorus.level });
    if (out === 'REV' || out === 'MIX+REV') edges.push({ from: 'chorus', to: 'reverb', level: input.chorus.level });
  }

  const reverbFed = edges.some((e) => e.to === 'reverb');
  if (reverbFed && input.reverb.level > 0) edges.push({ from: 'reverb', to: 'mix', level: input.reverb.level });

  const dead: { node: NodeId; why: string }[] = [];
  const leaves = (n: NodeId): boolean => edges.some((e) => e.from === n);
  if (efxFed && !leaves('efx')) {
    dead.push({
      node: 'efx',
      why:
        input.efx.mixSend === 0
          ? 'EFX Output Level (Mix Out Send Level) is 0, so nothing leaves the EFX: at level 0 nothing is sent (manual p.60)'
          : 'EFX Output Assign is a value the manual does not list, so the sound goes nowhere',
    });
  }
  if (chorusFed && !leaves('chorus')) dead.push({ node: 'chorus', why: 'Chorus Level is 0, so no chorus sound leaves the chorus' });
  if (reverbFed && !leaves('reverb')) dead.push({ node: 'reverb', why: 'Reverb Level is 0, so no reverb sound leaves the reverb' });

  return { edges, used: new Set(edges.map((e) => e.to)), ignoredTones: ignored, inert, deadEnds: dead };
}
