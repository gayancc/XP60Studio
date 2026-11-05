// How each tab of the System workspace is laid out. Parameters that come in families (seven Bank Select groups, the pedals,
// four preview notes) are rows of a table so that every switch, MSB and LSB lines up in a column; the rest are titled groups of
// related controls. A test makes sure every System parameter appears exactly once and in the tab its section says.

import type { SystemSection } from './layout';

export interface SystemGroup {
  kind: 'group';
  title: string;
  ids: readonly string[];
  /** All the controls are on/off switches: lay them out in aligned columns instead of letting them wrap. */
  switches?: boolean;
}

export interface SystemTable {
  kind: 'table';
  title: string;
  /** Column headings, after the row label. */
  columns: readonly string[];
  /** A null cell is a parameter that this row does not have. */
  rows: ReadonlyArray<{ label: string; cells: ReadonlyArray<string | null> }>;
}

export type SystemBlock = SystemGroup | SystemTable;

const group = (title: string, ...ids: string[]): SystemGroup => ({ kind: 'group', title, ids });
const switches = (title: string, ...ids: string[]): SystemGroup => ({ kind: 'group', title, ids, switches: true });

const range = (n: number): number[] => Array.from({ length: n }, (_, i) => i + 1);

export const SYSTEM_PAGES: Readonly<Record<SystemSection, readonly SystemBlock[]>> = {
  sound: [
    group('Sound mode', 'sound_mode', 'performance_number', 'patch_group_type', 'patch_group_id', 'patch_number'),
    group('Tuning and effects', 'master_tune', 'scale_tune_switch', 'efx_switch', 'chorus_switch', 'reverb_switch'),
    group('Patch and clock', 'patch_remain', 'clock_source'),
  ],
  controllers: [
    group('Control sources', 'tap_control_source', 'hold_control_source', 'peak_control_source', 'volume_control_source', 'aftertouch_source'),
    group('System control sources', 'system_control_source_1', 'system_control_source_2'),
  ],
  receive: [
    switches(
      'Messages received',
      'receive_program_change',
      'receive_bank_select',
      'receive_control_change',
      'receive_modulation',
      'receive_volume',
      'receive_hold_1',
      'receive_bender',
      'receive_aftertouch',
    ),
    group('Channels', 'control_channel', 'patch_receive_channel', 'rhythm_edit_source'),
  ],
  keyboard: [
    group('Transmit', 'transmit_program_change', 'transmit_bank_select', 'patch_transmit_channel'),
    group('Transpose', 'transpose_switch', 'transpose_value', 'octave_shift'),
    group('Keyboard feel', 'keyboard_velocity', 'keyboard_sens', 'aftertouch_sens'),
  ],
  pedals: [
    {
      kind: 'table',
      title: 'Pedals and control inputs',
      columns: ['Assign', 'Output mode', 'Polarity'],
      rows: [
        ...range(4).map((n) => ({ label: `Pedal ${n}`, cells: [`pedal${n}_assign`, `pedal${n}_output_mode`, `pedal${n}_polarity`] })),
        { label: 'C1', cells: ['c1_assign', 'c1_output_mode', null] },
        { label: 'C2', cells: ['c2_assign', 'c2_output_mode', null] },
        { label: 'Hold pedal', cells: [null, 'hold_pedal_output_mode', 'hold_pedal_polarity'] },
      ],
    },
  ],
  banks: [
    {
      kind: 'table',
      title: 'Bank Select for each group',
      columns: ['On', 'MSB', 'LSB'],
      rows: range(7).map((n) => ({ label: `Group ${n}`, cells: [`bank_select_group${n}_switch`, `bank_select_group${n}_msb`, `bank_select_group${n}_lsb`] })),
    },
  ],
  preview: [
    group('Preview', 'preview_sound_mode'),
    {
      kind: 'table',
      title: 'Preview notes',
      columns: ['Note', 'Velocity'],
      rows: range(4).map((n) => ({ label: `Set ${n}`, cells: [`preview_note_set_${n}`, `preview_velocity_set_${n}`] })),
    },
  ],
  arpeggio: [
    group('Pattern', 'arpeggio_style', 'arpeggio_motif', 'arpeggio_beat_pattern'),
    group('Feel', 'arpeggio_accent_rate', 'arpeggio_shuffle_rate'),
    group('Playing', 'arpeggio_keyboard_velocity', 'arpeggio_octave_range', 'arpeggio_part_number'),
  ],
};

/** Every parameter id a block lists, in order. */
export function idsOf(block: SystemBlock): string[] {
  return block.kind === 'group' ? [...block.ids] : block.rows.flatMap((r) => r.cells.flatMap((c) => (c === null ? [] : [c])));
}

/**
 * Short labels for the System controls. The automatic label drops words it thinks are redundant, which leaves three switches
 * called just "Switch" and breaks long words across lines; here every control says what it is, and not what its group title
 * already says. Controls without an entry use the automatic label.
 */
export const SYSTEM_LABELS: Readonly<Record<string, string>> = {
  // Sound and mode
  sound_mode: 'Mode',
  performance_number: 'Performance',
  patch_group_type: 'Group type',
  patch_group_id: 'Group ID',
  patch_number: 'Patch',
  master_tune: 'Master tune',
  scale_tune_switch: 'Scale tune',
  efx_switch: 'EFX',
  chorus_switch: 'Chorus',
  reverb_switch: 'Reverb',
  patch_remain: 'Patch remain',
  clock_source: 'Clock source',
  // Control sources
  tap_control_source: 'Tap',
  hold_control_source: 'Hold',
  peak_control_source: 'Peak',
  volume_control_source: 'Volume',
  aftertouch_source: 'Aftertouch',
  system_control_source_1: 'Source 1',
  system_control_source_2: 'Source 2',
  // Messages received
  receive_program_change: 'Program change',
  receive_bank_select: 'Bank select',
  receive_control_change: 'Control change',
  receive_modulation: 'Modulation',
  receive_volume: 'Volume',
  receive_hold_1: 'Hold-1',
  receive_bender: 'Pitch bend',
  receive_aftertouch: 'Aftertouch',
  control_channel: 'Control channel',
  patch_receive_channel: 'Patch channel',
  rhythm_edit_source: 'Rhythm edit',
  // Keyboard and transmit
  transmit_program_change: 'Program change',
  transmit_bank_select: 'Bank select',
  patch_transmit_channel: 'Channel',
  transpose_switch: 'On',
  transpose_value: 'Semitones',
  octave_shift: 'Octave',
  keyboard_velocity: 'Velocity',
  keyboard_sens: 'Sensitivity',
  aftertouch_sens: 'Aftertouch',
  // Arpeggio
  arpeggio_style: 'Style',
  arpeggio_motif: 'Motif',
  arpeggio_beat_pattern: 'Beat',
  arpeggio_accent_rate: 'Accent',
  arpeggio_shuffle_rate: 'Shuffle',
  arpeggio_keyboard_velocity: 'Velocity',
  arpeggio_octave_range: 'Octaves',
  arpeggio_part_number: 'Part',
  // Preview
  preview_sound_mode: 'Sound mode',
};
