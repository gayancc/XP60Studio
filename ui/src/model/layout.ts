// Which section of the editor each parameter lives in. Rules, not a hand-copied list, so a parameter added to the map
// cannot silently go missing: the layout test fails if any parameter matches no rule.

import type { BlockInfo } from './catalog';

export type ToneSection =
  | 'mixer'
  | 'wave'
  | 'zone'
  | 'controllers'
  | 'lfo1'
  | 'lfo2'
  | 'pitch'
  | 'tvf'
  | 'tva'
  | 'output';

export type CommonSection =
  | 'name'
  | 'patch'
  | 'play'
  | 'portamento'
  | 'efx'
  | 'chorus'
  | 'reverb'
  | 'controllers'
  | 'structure';

const TONE_RULES: ReadonlyArray<readonly [RegExp, ToneSection]> = [
  [/^(tone_switch|tone_level|tone_pan)$/, 'mixer'],
  [/^(wave_|fxm_|tone_delay_)/, 'wave'],
  [/^(velocity_cross_fade|velocity_range_|keyboard_range_)/, 'zone'],
  [/^(redamper_|volume_control|hold_1_|bender_control|pan_control|controller_)/, 'controllers'],
  [/^lfo1_/, 'lfo1'],
  [/^lfo2_/, 'lfo2'],
  [/^(coarse_tune|fine_tune|random_pitch|pitch_)/, 'pitch'],
  [/^(filter_|cutoff_|resonance)/, 'tvf'],
  [/^(bias_|level_|pan_keyfollow|random_pan|alternate_pan|pan_lfo)/, 'tva'],
  [/^(output_assign|mix_efx|chorus_send|reverb_send)/, 'output'],
];

const COMMON_RULES: ReadonlyArray<readonly [RegExp, CommonSection]> = [
  [/^patch_name_/, 'name'],
  [/^efx_/, 'efx'],
  [/^chorus_/, 'chorus'],
  [/^(reverb_|delay_feedback)/, 'reverb'],
  [/^(patch_level|patch_pan|patch_tempo|analog_feel|octave_shift|stretch_tune|voice_priority|clock_source)/, 'patch'],
  [/^(bend_range|key_assign|solo_legato)/, 'play'],
  [/^portamento_/, 'portamento'],
  [/^(patch_control_source|control_\d_hold)/, 'controllers'],
  [/^(velocity_range_switch|structure_type|booster)/, 'structure'],
];

export function toneSectionOf(id: string): ToneSection | undefined {
  return TONE_RULES.find(([re]) => re.test(id))?.[1];
}

export function commonSectionOf(id: string): CommonSection | undefined {
  return COMMON_RULES.find(([re]) => re.test(id))?.[1];
}

/** Parameter ids of a block that fall in `section`, in map order. */
export function toneParams(block: BlockInfo, section: ToneSection): string[] {
  return block.params.map((p) => p.id).filter((id) => toneSectionOf(id) === section);
}

export function commonParams(block: BlockInfo, section: CommonSection): string[] {
  return block.params.map((p) => p.id).filter((id) => commonSectionOf(id) === section);
}

export type PerformanceCommonSection = 'name' | 'general' | 'efx' | 'chorus' | 'reverb' | 'voices';
export type SystemSection =
  | 'sound'
  | 'controllers'
  | 'receive'
  | 'keyboard'
  | 'pedals'
  | 'banks'
  | 'preview'
  | 'arpeggio';
export type PartSection = 'source' | 'mix' | 'tune' | 'range' | 'midi';
export type NoteSection = 'wave' | 'pitch' | 'tvf' | 'tva' | 'output';

const PERF_COMMON_RULES: ReadonlyArray<readonly [RegExp, PerformanceCommonSection]> = [
  [/^performance_name_/, 'name'],
  [/^efx_/, 'efx'],
  [/^chorus_/, 'chorus'],
  [/^(reverb_|delay_feedback)/, 'reverb'],
  [/^voice_reserve_/, 'voices'],
  [/^(performance_tempo|keyboard_range_switch|keyboard_mode|clock_source)/, 'general'],
];

const PART_RULES: ReadonlyArray<readonly [RegExp, PartSection]> = [
  [/^(receive_switch|midi_channel|patch_group_|patch_number)/, 'source'],
  [/^(part_level|part_pan|output_assign|mix_efx|chorus_send|reverb_send)/, 'mix'],
  [/^(part_coarse|part_fine|octave_shift)/, 'tune'],
  [/^keyboard_range_/, 'range'],
  [/^(receive_|local_switch|transmit_)/, 'midi'],
];

const NOTE_RULES: ReadonlyArray<readonly [RegExp, NoteSection]> = [
  [/^(tone_switch|wave_|bend_range|mute_group|envelope_mode|volume_control|hold_1|pan_control|source_key)/, 'wave'],
  [/^(fine_tune|random_pitch|pitch_)/, 'pitch'],
  [/^(filter_|cutoff|resonance)/, 'tvf'],
  [/^(tone_level|level_|tone_pan|random_pan|alternate_pan)/, 'tva'],
  [/^(output_assign|mix_efx|chorus_send|reverb_send)/, 'output'],
];

const SYSTEM_RULES: ReadonlyArray<readonly [RegExp, SystemSection]> = [
  [/^(sound_mode|performance_number|patch_group_|patch_number|master_tune|scale_tune_switch|efx_switch|chorus_switch|reverb_switch|patch_remain|clock_source)/, 'sound'],
  [/^(tap_control_source|hold_control_source|peak_control_source|volume_control_source|aftertouch_source|system_control_source_)/, 'controllers'],
  [/^(receive_|control_channel|patch_receive_channel|rhythm_edit_source)/, 'receive'],
  [/^(transmit_|patch_transmit_channel|transpose_|octave_shift|keyboard_|aftertouch_sens)/, 'keyboard'],
  [/^(pedal\d_|c\d_|hold_pedal_)/, 'pedals'],
  [/^bank_select_/, 'banks'],
  [/^preview_/, 'preview'],
  [/^arpeggio_/, 'arpeggio'],
];

export function systemSectionOf(id: string): SystemSection | undefined {
  return SYSTEM_RULES.find(([re]) => re.test(id))?.[1];
}

export function performanceCommonSectionOf(id: string): PerformanceCommonSection | undefined {
  return PERF_COMMON_RULES.find(([re]) => re.test(id))?.[1];
}

export function partSectionOf(id: string): PartSection | undefined {
  return PART_RULES.find(([re]) => re.test(id))?.[1];
}

export function noteSectionOf(id: string): NoteSection | undefined {
  return NOTE_RULES.find(([re]) => re.test(id))?.[1];
}

/** Ids of `block` for which `sectionOf` gives `section`, in map order. */
export function paramsInSection<S extends string>(
  block: BlockInfo,
  sectionOf: (id: string) => S | undefined,
  section: S,
): string[] {
  return block.params.map((p) => p.id).filter((id) => sectionOf(id) === section);
}

/** An envelope drawn as a graph. Its other parameters are plain knobs shown beside it. */
export interface EnvelopeSpec {
  key: 'pitch' | 'tvf' | 'tva';
  title: string;
  times: readonly string[];
  levels: readonly string[];
  /** Raw level the graph starts from (pitch rests at its centre, the others at silence). */
  startLevel: number;
  /** When set, the last stage ends at this level, which is not a parameter (the level envelope ends at 0). */
  endLevel?: number;
  extras: readonly string[];
}

const nums = (prefix: string, n: number): string[] => Array.from({ length: n }, (_, i) => `${prefix}${i + 1}`);

export const ENVELOPES: readonly EnvelopeSpec[] = [
  {
    key: 'pitch',
    title: 'Pitch envelope',
    times: nums('pitch_envelope_time_', 4),
    levels: nums('pitch_envelope_level_', 4),
    startLevel: 63,
    extras: [
      'pitch_envelope_depth',
      'pitch_envelope_velocity_sens',
      'pitch_envelope_velocity_time1',
      'pitch_envelope_velocity_time4',
      'pitch_envelope_time_keyfollow',
    ],
  },
  {
    key: 'tvf',
    title: 'Filter envelope',
    times: nums('filter_envelope_time_', 4),
    levels: nums('filter_envelope_level_', 4),
    startLevel: 0,
    extras: [
      'filter_envelope_depth',
      'filter_envelope_velocity_curve',
      'filter_envelope_velocity_sens',
      'filter_envelope_velocity_time1',
      'filter_envelope_velocity_time4',
      'filter_envelope_time_keyfollow',
    ],
  },
  {
    key: 'tva',
    title: 'Level envelope',
    times: nums('level_envelope_time_', 4),
    levels: nums('level_envelope_level_', 3),
    startLevel: 0,
    endLevel: 0,
    extras: [
      'level_envelope_velocity_curve',
      'level_envelope_velocity_sens',
      'level_envelope_velocity_time1',
      'level_envelope_velocity_time4',
      'level_envelope_time_keyfollow',
    ],
  },
];

export function envelopeParamIds(spec: EnvelopeSpec): string[] {
  return [...spec.times, ...spec.levels, ...spec.extras];
}

/** The envelopes of a Rhythm Note: the same graphs, with a single velocity-time parameter and no key follow. */
export const RHYTHM_ENVELOPES: readonly EnvelopeSpec[] = [
  {
    key: 'pitch',
    title: 'Pitch envelope',
    times: nums('pitch_envelope_time_', 4),
    levels: nums('pitch_envelope_level_', 4),
    startLevel: 63,
    extras: ['pitch_envelope_depth', 'pitch_envelope_velocity_sens', 'pitch_envelope_velocity_time'],
  },
  {
    key: 'tvf',
    title: 'Filter envelope',
    times: nums('filter_envelope_time_', 4),
    levels: nums('filter_envelope_level_', 4),
    startLevel: 0,
    extras: ['filter_envelope_depth', 'filter_envelope_velocity_sens', 'filter_envelope_velocity_time'],
  },
  {
    key: 'tva',
    title: 'Level envelope',
    times: nums('level_envelope_time_', 4),
    levels: nums('level_envelope_level_', 3),
    startLevel: 0,
    endLevel: 0,
    extras: ['level_envelope_velocity_sens', 'level_envelope_velocity_time'],
  },
];
