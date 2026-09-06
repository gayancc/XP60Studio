// GENERATED FILE — DO NOT EDIT. See Xp60SystemTables.h for provenance.
// Source digest: sha256:9bced45431a171e8
#include "xpmodel/generated/Xp60SystemTables.h"

#include "xp60/Xp60Device.h"

namespace xp60studio::xpmodel::xp60system {

namespace {

using xp60::VerificationStatus;

constexpr std::array<std::string_view, 3> kLabels1{{"PERFORMANCE", "PATCH", "GM"}};
constexpr std::array<std::string_view, 3> kLabels2{{"USER&PRESET", "<PCM>", "EXP"}};
constexpr std::array<std::string_view, 2> kLabels3{{"OFF", "ON"}};
constexpr std::array<std::string_view, 5> kLabels4{{"OFF", "HOLD-1", "SOSTENUTO", "SOFT", "HOLD-2"}};
constexpr std::array<std::string_view, 2> kLabels5{{"VOLUME", "VOL&EXP"}};
constexpr std::array<std::string_view, 3> kLabels6{{"CHANNEL", "POLY", "CH&POLY"}};
constexpr std::array<std::string_view, 17> kLabels7{{"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "OFF"}};
constexpr std::array<std::string_view, 2> kLabels8{{"SINGLE", "CHORD"}};
constexpr std::array<std::string_view, 3> kLabels9{{"LIGHT", "STANDARD", "HEAVY"}};
constexpr std::array<std::string_view, 4> kLabels10{{"OFF", "INT", "MIDI", "INT&MIDI"}};
constexpr std::array<std::string_view, 2> kLabels11{{"STANDARD", "REVERSE"}};
constexpr std::array<std::string_view, 16> kLabels12{{"PART1", "PART2", "PART3", "PART4", "PART5", "PART6", "PART7", "PART8", "PART9", "PART10", "PART11", "PART12", "PART13", "PART14", "PART15", "PART16"}};

const std::array<ParameterDescriptor, 95> kCommonRows{{
    ParameterDescriptor{"common.sound_mode", "Sound Mode", 0, ParameterEncoding::SevenBit, 1, 0, 2, 0, 1, DisplayStyle::Number, "", kLabels1, "Mode", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1 note *1"},
    ParameterDescriptor{"common.performance_number", "Performance Number", 1, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Mode", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1 note *2"},
    ParameterDescriptor{"common.patch_group_type", "Patch Group Type", 2, ParameterEncoding::SevenBit, 1, 0, 2, 0, 1, DisplayStyle::Number, "", kLabels2, "Mode", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1 note *3"},
    ParameterDescriptor{"common.patch_group_id", "Patch Group ID", 3, ParameterEncoding::SevenBit, 1, 1, 127, 0, 1, DisplayStyle::Number, "", {}, "Mode", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.patch_number", "Patch Number", 4, ParameterEncoding::Nibble, 2, 0, 254, 1, 1, DisplayStyle::Number, "", {}, "Mode", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.master_tune", "Master Tune", 6, ParameterEncoding::SevenBit, 1, 0, 126, 0, 1, DisplayStyle::Number, "", {}, "Tuning", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1 note *4"},
    ParameterDescriptor{"common.scale_tune_switch", "Scale Tune Switch", 7, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels3, "Tuning", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.efx_switch", "EFX Switch", 8, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels3, "EFX", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.chorus_switch", "Chorus Switch", 9, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels3, "Chorus", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.reverb_switch", "Reverb Switch", 10, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels3, "Reverb", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.patch_remain", "Patch Remain", 11, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels3, "Common", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.clock_source", "Clock Source", 12, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", {}, "Sync", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1 note *5"},
    ParameterDescriptor{"common.tap_control_source", "TAP Control Source", 13, ParameterEncoding::SevenBit, 1, 0, 4, 0, 1, DisplayStyle::Number, "", {}, "Controllers", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1 note *6"},
    ParameterDescriptor{"common.hold_control_source", "Hold Control Source", 14, ParameterEncoding::SevenBit, 1, 0, 4, 0, 1, DisplayStyle::Number, "", kLabels4, "Controllers", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1 note *7"},
    ParameterDescriptor{"common.peak_control_source", "Peak Control Source", 15, ParameterEncoding::SevenBit, 1, 0, 4, 0, 1, DisplayStyle::Number, "", kLabels4, "Controllers", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1 note *7"},
    ParameterDescriptor{"common.volume_control_source", "Volume Control Source", 16, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels5, "Controllers", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1 note *8"},
    ParameterDescriptor{"common.aftertouch_source", "Aftertouch Source", 17, ParameterEncoding::SevenBit, 1, 0, 2, 0, 1, DisplayStyle::Number, "", kLabels6, "Keyboard", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1 note *9"},
    ParameterDescriptor{"common.system_control_source_1", "System Control Source 1", 18, ParameterEncoding::SevenBit, 1, 1, 97, 0, 1, DisplayStyle::Number, "", {}, "Controllers", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1 note *10"},
    ParameterDescriptor{"common.system_control_source_2", "System Control Source 2", 19, ParameterEncoding::SevenBit, 1, 1, 97, 0, 1, DisplayStyle::Number, "", {}, "Controllers", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1 note *10"},
    ParameterDescriptor{"common.receive_program_change", "Receive Program Change", 20, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels3, "MIDI", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.receive_bank_select", "Receive Bank Select", 21, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels3, "Bank Select", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.receive_control_change", "Receive Control Change", 22, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels3, "MIDI", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.receive_modulation", "Receive Modulation", 23, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels3, "MIDI", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.receive_volume", "Receive Volume", 24, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels3, "MIDI", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.receive_hold_1", "Receive Hold-1", 25, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels3, "MIDI", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.receive_bender", "Receive Bender", 26, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels3, "MIDI", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.receive_aftertouch", "Receive Aftertouch", 27, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels3, "MIDI", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.control_channel", "Control Channel", 28, ParameterEncoding::SevenBit, 1, 0, 16, 0, 1, DisplayStyle::Number, "", kLabels7, "MIDI", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.patch_receive_channel", "Patch Receive Channel", 29, ParameterEncoding::SevenBit, 1, 0, 15, 1, 1, DisplayStyle::Number, "", {}, "MIDI", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.rhythm_edit_source", "Rhythm Edit Source", 30, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels3, "Common", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.preview_sound_mode", "Preview Sound Mode", 31, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels8, "Mode", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.preview_note_set_1", "Preview Note Set 1", 32, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::NoteName, "", {}, "Preview", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.preview_velocity_set_1", "Preview Velocity Set 1", 33, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Preview", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1 note *11"},
    ParameterDescriptor{"common.preview_note_set_2", "Preview Note Set 2", 34, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::NoteName, "", {}, "Preview", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.preview_velocity_set_2", "Preview Velocity Set 2", 35, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Preview", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1 note *11"},
    ParameterDescriptor{"common.preview_note_set_3", "Preview Note Set 3", 36, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::NoteName, "", {}, "Preview", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.preview_velocity_set_3", "Preview Velocity Set 3", 37, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Preview", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1 note *11"},
    ParameterDescriptor{"common.preview_note_set_4", "Preview Note Set 4", 38, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::NoteName, "", {}, "Preview", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.preview_velocity_set_4", "Preview Velocity Set 4", 39, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Preview", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1 note *11"},
    ParameterDescriptor{"common.transmit_program_change", "Transmit Program Change", 40, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels3, "MIDI", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.transmit_bank_select", "Transmit Bank Select", 41, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels3, "Bank Select", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.patch_transmit_channel", "Patch Transmit Channel", 42, ParameterEncoding::SevenBit, 1, 0, 17, 0, 1, DisplayStyle::Number, "", {}, "MIDI", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1 note *12"},
    ParameterDescriptor{"common.transpose_switch", "Transpose Switch", 43, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels3, "Tuning", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.transpose_value", "Transpose Value", 44, ParameterEncoding::SevenBit, 1, 0, 11, -5, 1, DisplayStyle::Number, "", {}, "Tuning", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.octave_shift", "Octave Shift", 45, ParameterEncoding::SevenBit, 1, 0, 6, -3, 1, DisplayStyle::Number, "", {}, "Keyboard", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.keyboard_velocity", "Keyboard Velocity", 46, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Keyboard", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1 note *13"},
    ParameterDescriptor{"common.keyboard_sens", "Keyboard Sens", 47, ParameterEncoding::SevenBit, 1, 0, 2, 0, 1, DisplayStyle::Number, "", kLabels9, "Keyboard", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1 note *14"},
    ParameterDescriptor{"common.aftertouch_sens", "Aftertouch Sens", 48, ParameterEncoding::SevenBit, 1, 0, 100, 0, 1, DisplayStyle::Number, "", {}, "Keyboard", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.pedal1_assign", "Pedal1 Assign", 49, ParameterEncoding::SevenBit, 1, 1, 104, 0, 1, DisplayStyle::Number, "", {}, "Controllers", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1 note *15"},
    ParameterDescriptor{"common.pedal1_output_mode", "Pedal1 Output Mode", 50, ParameterEncoding::SevenBit, 1, 0, 3, 0, 1, DisplayStyle::Number, "", kLabels10, "Controllers", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1 note *16"},
    ParameterDescriptor{"common.pedal1_polarity", "Pedal1 Polarity", 51, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels11, "Controllers", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1 note *17"},
    ParameterDescriptor{"common.pedal2_assign", "Pedal2 Assign", 52, ParameterEncoding::SevenBit, 1, 1, 104, 0, 1, DisplayStyle::Number, "", {}, "Controllers", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1 note *15"},
    ParameterDescriptor{"common.pedal2_output_mode", "Pedal2 Output Mode", 53, ParameterEncoding::SevenBit, 1, 0, 3, 0, 1, DisplayStyle::Number, "", kLabels10, "Controllers", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1 note *16"},
    ParameterDescriptor{"common.pedal2_polarity", "Pedal2 Polarity", 54, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels11, "Controllers", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1 note *17"},
    ParameterDescriptor{"common.c1_assign", "C1 Assign", 55, ParameterEncoding::SevenBit, 1, 1, 97, 0, 1, DisplayStyle::Number, "", {}, "Controllers", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1 note *10"},
    ParameterDescriptor{"common.c1_output_mode", "C1 Output Mode", 56, ParameterEncoding::SevenBit, 1, 0, 3, 0, 1, DisplayStyle::Number, "", kLabels10, "Controllers", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1 note *16"},
    ParameterDescriptor{"common.c2_assign", "C2 Assign", 57, ParameterEncoding::SevenBit, 1, 1, 97, 0, 1, DisplayStyle::Number, "", {}, "Controllers", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1 note *10"},
    ParameterDescriptor{"common.c2_output_mode", "C2 Output Mode", 58, ParameterEncoding::SevenBit, 1, 0, 3, 0, 1, DisplayStyle::Number, "", kLabels10, "Controllers", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1 note *16"},
    ParameterDescriptor{"common.hold_pedal_output_mode", "Hold Pedal Output Mode", 59, ParameterEncoding::SevenBit, 1, 0, 3, 0, 1, DisplayStyle::Number, "", kLabels10, "Controllers", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1 note *16"},
    ParameterDescriptor{"common.hold_pedal_polarity", "Hold Pedal Polarity", 60, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels11, "Controllers", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1 note *17"},
    ParameterDescriptor{"common.bank_select_group1_switch", "Bank Select Group1 Switch", 61, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels3, "Bank Select", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.bank_select_group1_msb", "Bank Select Group1 MSB", 62, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Bank Select", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.bank_select_group1_lsb", "Bank Select Group1 LSB", 63, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Bank Select", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.bank_select_group2_switch", "Bank Select Group2 Switch", 64, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels3, "Bank Select", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.bank_select_group2_msb", "Bank Select Group2 MSB", 65, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Bank Select", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.bank_select_group2_lsb", "Bank Select Group2 LSB", 66, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Bank Select", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.bank_select_group3_switch", "Bank Select Group3 Switch", 67, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels3, "Bank Select", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.bank_select_group3_msb", "Bank Select Group3 MSB", 68, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Bank Select", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.bank_select_group3_lsb", "Bank Select Group3 LSB", 69, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Bank Select", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.bank_select_group4_switch", "Bank Select Group4 Switch", 70, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels3, "Bank Select", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.bank_select_group4_msb", "Bank Select Group4 MSB", 71, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Bank Select", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.bank_select_group4_lsb", "Bank Select Group4 LSB", 72, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Bank Select", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.bank_select_group5_switch", "Bank Select Group5 Switch", 73, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels3, "Bank Select", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.bank_select_group5_msb", "Bank Select Group5 MSB", 74, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Bank Select", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.bank_select_group5_lsb", "Bank Select Group5 LSB", 75, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Bank Select", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.bank_select_group6_switch", "Bank Select Group6 Switch", 76, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels3, "Bank Select", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.bank_select_group6_msb", "Bank Select Group6 MSB", 77, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Bank Select", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.bank_select_group6_lsb", "Bank Select Group6 LSB", 78, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Bank Select", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.bank_select_group7_switch", "Bank Select Group7 Switch", 79, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels3, "Bank Select", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.bank_select_group7_msb", "Bank Select Group7 MSB", 80, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Bank Select", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.bank_select_group7_lsb", "Bank Select Group7 LSB", 81, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Bank Select", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.pedal3_assign", "Pedal3 Assign", 82, ParameterEncoding::SevenBit, 1, 1, 104, 0, 1, DisplayStyle::Number, "", {}, "Controllers", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1 note *15"},
    ParameterDescriptor{"common.pedal3_output_mode", "Pedal3 Output Mode", 83, ParameterEncoding::SevenBit, 1, 0, 3, 0, 1, DisplayStyle::Number, "", kLabels10, "Controllers", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1 note *16"},
    ParameterDescriptor{"common.pedal3_polarity", "Pedal3 Polarity", 84, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels11, "Controllers", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1 note *17"},
    ParameterDescriptor{"common.pedal4_assign", "Pedal4 Assign", 85, ParameterEncoding::SevenBit, 1, 1, 104, 0, 1, DisplayStyle::Number, "", {}, "Controllers", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1 note *15"},
    ParameterDescriptor{"common.pedal4_output_mode", "Pedal4 Output Mode", 86, ParameterEncoding::SevenBit, 1, 0, 3, 0, 1, DisplayStyle::Number, "", kLabels10, "Controllers", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1 note *16"},
    ParameterDescriptor{"common.pedal4_polarity", "Pedal4 Polarity", 87, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels11, "Controllers", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1 note *17"},
    ParameterDescriptor{"common.arpeggio_style", "Arpeggio Style", 88, ParameterEncoding::SevenBit, 1, 0, 32, 1, 1, DisplayStyle::Number, "", {}, "Arpeggio", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.arpeggio_motif", "Arpeggio Motif", 89, ParameterEncoding::SevenBit, 1, 0, 33, 1, 1, DisplayStyle::Number, "", {}, "Arpeggio", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.arpeggio_beat_pattern", "Arpeggio Beat Pattern", 90, ParameterEncoding::SevenBit, 1, 0, 40, 1, 1, DisplayStyle::Number, "", {}, "Arpeggio", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.arpeggio_accent_rate", "Arpeggio Accent Rate", 91, ParameterEncoding::SevenBit, 1, 0, 100, 0, 1, DisplayStyle::Number, "", {}, "Arpeggio", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.arpeggio_shuffle_rate", "Arpeggio Shuffle Rate", 92, ParameterEncoding::SevenBit, 1, 50, 90, 0, 1, DisplayStyle::Number, "", {}, "Arpeggio", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.arpeggio_keyboard_velocity", "Arpeggio Keyboard Velocity", 93, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Arpeggio", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1 note *13"},
    ParameterDescriptor{"common.arpeggio_octave_range", "Arpeggio Octave Range", 94, ParameterEncoding::SevenBit, 1, 0, 6, -3, 1, DisplayStyle::Number, "", {}, "Arpeggio", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
    ParameterDescriptor{"common.arpeggio_part_number", "Arpeggio Part Number", 95, ParameterEncoding::SevenBit, 1, 0, 15, 0, 1, DisplayStyle::Number, "", kLabels12, "Arpeggio", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-1"},
}};

const std::array<ParameterDescriptor, 12> kPartRows{{
    ParameterDescriptor{"scale.c", "Scale Tune for C", 0, ParameterEncoding::SevenBit, 1, 0, 127, -64, 1, DisplayStyle::Number, "", {}, "Scale Tune", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-2"},
    ParameterDescriptor{"scale.c_sharp", "Scale Tune for C#", 1, ParameterEncoding::SevenBit, 1, 0, 127, -64, 1, DisplayStyle::Number, "", {}, "Scale Tune", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-2"},
    ParameterDescriptor{"scale.d", "Scale Tune for D", 2, ParameterEncoding::SevenBit, 1, 0, 127, -64, 1, DisplayStyle::Number, "", {}, "Scale Tune", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-2"},
    ParameterDescriptor{"scale.d_sharp", "Scale Tune for D#", 3, ParameterEncoding::SevenBit, 1, 0, 127, -64, 1, DisplayStyle::Number, "", {}, "Scale Tune", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-2"},
    ParameterDescriptor{"scale.e", "Scale Tune for E", 4, ParameterEncoding::SevenBit, 1, 0, 127, -64, 1, DisplayStyle::Number, "", {}, "Scale Tune", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-2"},
    ParameterDescriptor{"scale.f", "Scale Tune for F", 5, ParameterEncoding::SevenBit, 1, 0, 127, -64, 1, DisplayStyle::Number, "", {}, "Scale Tune", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-2"},
    ParameterDescriptor{"scale.f_sharp", "Scale Tune for F#", 6, ParameterEncoding::SevenBit, 1, 0, 127, -64, 1, DisplayStyle::Number, "", {}, "Scale Tune", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-2"},
    ParameterDescriptor{"scale.g", "Scale Tune for G", 7, ParameterEncoding::SevenBit, 1, 0, 127, -64, 1, DisplayStyle::Number, "", {}, "Scale Tune", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-2"},
    ParameterDescriptor{"scale.g_sharp", "Scale Tune for G#", 8, ParameterEncoding::SevenBit, 1, 0, 127, -64, 1, DisplayStyle::Number, "", {}, "Scale Tune", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-2"},
    ParameterDescriptor{"scale.a", "Scale Tune for A", 9, ParameterEncoding::SevenBit, 1, 0, 127, -64, 1, DisplayStyle::Number, "", {}, "Scale Tune", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-2"},
    ParameterDescriptor{"scale.a_sharp", "Scale Tune for A#", 10, ParameterEncoding::SevenBit, 1, 0, 127, -64, 1, DisplayStyle::Number, "", {}, "Scale Tune", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-2"},
    ParameterDescriptor{"scale.b", "Scale Tune for B", 11, ParameterEncoding::SevenBit, 1, 0, 127, -64, 1, DisplayStyle::Number, "", {}, "Scale Tune", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-1-2"},
}};

} // namespace

const ParameterTable& systemCommonTable() noexcept
{
    static const ParameterTable kTable{"XP-60 System Common", kSystemCommonSize,
                                       std::span<const ParameterDescriptor>(kCommonRows.data(), kCommonRows.size()),
                                       TableCompleteness::Complete,
                                       "Roland XP-60/XP-80 MIDI Implementation, Parameter Address Map 1-1-1"};
    return kTable;
}

const ParameterTable& scaleTuneTable() noexcept
{
    static const ParameterTable kTable{"XP-60 Scale Tune", kScaleTuneSize,
                                       std::span<const ParameterDescriptor>(kPartRows.data(), kPartRows.size()),
                                       TableCompleteness::Complete,
                                       "Roland XP-60/XP-80 MIDI Implementation, Parameter Address Map 1-1-2"};
    return kTable;
}

const ParameterDescriptor& descriptor(SystemCommonParameter parameter) noexcept
{
    return kCommonRows[static_cast<std::size_t>(parameter)];
}

const ParameterDescriptor& descriptor(ScaleTuneParameter parameter) noexcept
{
    return kPartRows[static_cast<std::size_t>(parameter)];
}

} // namespace xp60studio::xpmodel::xp60system
