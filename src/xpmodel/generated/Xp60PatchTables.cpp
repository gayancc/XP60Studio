// GENERATED FILE — DO NOT EDIT. See Xp60PatchTables.h for provenance.
// Source digest: sha256:a30c743a97ba9d89
#include "xpmodel/generated/Xp60PatchTables.h"

#include "xp60/Xp60Device.h"

namespace xp60studio::xpmodel::xp60tables {

namespace {

using xp60::VerificationStatus;

constexpr std::array<std::string_view, 3> kLabels1{{"MIX", "DIR", "<OUTPUT-2>"}};
constexpr std::array<std::string_view, 11> kLabels2{{"OFF", "SYS-CTRL1", "SYS-CTRL2", "MODULATION", "BREATH", "FOOT", "VOLUME", "PAN", "EXPRESSION", "PITCH BEND", "AFTERTOUCH"}};
constexpr std::array<std::string_view, 3> kLabels3{{"MIX", "REVERB", "MIX+REV"}};
constexpr std::array<std::string_view, 8> kLabels4{{"ROOM1", "ROOM2", "STAGE1", "STAGE2", "HALL1", "HALL2", "DELAY", "PAN-DLY"}};
constexpr std::array<std::string_view, 18> kLabels5{{"200", "250", "315", "400", "500", "630", "800", "1000", "1250", "1600", "2000", "2500", "3150", "4000", "5000", "6300", "8000", "BYPASS"}};
constexpr std::array<std::string_view, 2> kLabels6{{"POLY", "SOLO"}};
constexpr std::array<std::string_view, 2> kLabels7{{"OFF", "ON"}};
constexpr std::array<std::string_view, 2> kLabels8{{"NORMAL", "LEGATO"}};
constexpr std::array<std::string_view, 2> kLabels9{{"RATE", "TIME"}};
constexpr std::array<std::string_view, 2> kLabels10{{"PITCH", "NOTE"}};
constexpr std::array<std::string_view, 16> kLabels11{{"OFF", "SYS-CTRL1", "SYS-CTRL2", "MODULATION", "BREATH", "FOOT", "VOLUME", "PAN", "EXPRESSION", "PITCH BEND", "AFTERTOUCH", "LFO1", "LFO2", "VELOCITY", "KEYFOLLOW", "PLAYMATE"}};
constexpr std::array<std::string_view, 3> kLabels12{{"OFF", "HOLD", "PEAK"}};
constexpr std::array<std::string_view, 4> kLabels13{{"OFF", "1", "2", "3"}};
constexpr std::array<std::string_view, 2> kLabels14{{"LAST", "LOUDEST"}};
constexpr std::array<std::string_view, 4> kLabels15{{"0", "+6", "+12", "+18"}};
constexpr std::array<std::string_view, 2> kLabels16{{"PATCH", "SEQUENCER"}};
constexpr std::array<std::string_view, 3> kLabels17{{"INT", "<PCM>", "EXP"}};
constexpr std::array<std::string_view, 4> kLabels18{{"-6", "0", "+6", "+12"}};
constexpr std::array<std::string_view, 8> kLabels19{{"NORMAL", "HOLD", "PLAYMATE", "CLOCK-SYNC", "<TAP-SYNC>", "KEY-OFF-N", "KEY-OFF-D", "TEMPO-SYNC"}};
constexpr std::array<std::string_view, 3> kLabels20{{"OFF", "CONTINUOUS", "KEY-ON"}};
constexpr std::array<std::string_view, 19> kLabels21{{"OFF", "PCH", "CUT", "RES", "LEV", "PAN", "MIX", "CHO", "REV", "PL1", "PL2", "FL1", "FL2", "AL1", "AL2", "pL1", "pL2", "L1R", "L2R"}};
constexpr std::array<std::string_view, 8> kLabels22{{"TRI", "SIN", "SAW", "SQR", "TRP", "S&H", "RND", "CHS"}};
constexpr std::array<std::string_view, 5> kLabels23{{"-100", "-50", "0", "+50", "+100"}};
constexpr std::array<std::string_view, 4> kLabels24{{"ON-IN", "ON-OUT", "OFF-IN", "OFF-OUT"}};
constexpr std::array<std::string_view, 3> kLabels25{{"OFF", "CLOCK", "<TAP>"}};
constexpr std::array<std::string_view, 31> kLabels26{{"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "20", "30", "40", "50", "60", "70", "80", "90", "100", "200", "300", "400", "500", "600", "700", "800", "900", "1000", "1100", "1200"}};
constexpr std::array<std::string_view, 16> kLabels27{{"-100", "-70", "-50", "-30", "-10", "0", "+10", "+20", "+30", "+40", "+50", "+70", "+100", "+120", "+150", "+200"}};
constexpr std::array<std::string_view, 15> kLabels28{{"-100", "-70", "-50", "-40", "-30", "-20", "-10", "0", "+10", "+20", "+30", "+40", "+50", "+70", "+100"}};
constexpr std::array<std::string_view, 5> kLabels29{{"OFF", "LPF", "BPF", "HPF", "PKG"}};
constexpr std::array<std::string_view, 4> kLabels30{{"LOWER", "UPPER", "LOW&UP", "ALL"}};
constexpr std::array<std::string_view, 4> kLabels31{{"MIX", "EFX", "DIR", "<OUTPUT-2>"}};

const std::array<ParameterDescriptor, 72> kCommonRows{{
    ParameterDescriptor{"common.name.1", "Patch Name 1", 0, ParameterEncoding::Ascii, 1, 32, 127, 0, 1, DisplayStyle::Number, "", {}, "Name", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.name.2", "Patch Name 2", 1, ParameterEncoding::Ascii, 1, 32, 127, 0, 1, DisplayStyle::Number, "", {}, "Name", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.name.3", "Patch Name 3", 2, ParameterEncoding::Ascii, 1, 32, 127, 0, 1, DisplayStyle::Number, "", {}, "Name", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.name.4", "Patch Name 4", 3, ParameterEncoding::Ascii, 1, 32, 127, 0, 1, DisplayStyle::Number, "", {}, "Name", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.name.5", "Patch Name 5", 4, ParameterEncoding::Ascii, 1, 32, 127, 0, 1, DisplayStyle::Number, "", {}, "Name", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.name.6", "Patch Name 6", 5, ParameterEncoding::Ascii, 1, 32, 127, 0, 1, DisplayStyle::Number, "", {}, "Name", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.name.7", "Patch Name 7", 6, ParameterEncoding::Ascii, 1, 32, 127, 0, 1, DisplayStyle::Number, "", {}, "Name", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.name.8", "Patch Name 8", 7, ParameterEncoding::Ascii, 1, 32, 127, 0, 1, DisplayStyle::Number, "", {}, "Name", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.name.9", "Patch Name 9", 8, ParameterEncoding::Ascii, 1, 32, 127, 0, 1, DisplayStyle::Number, "", {}, "Name", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.name.10", "Patch Name 10", 9, ParameterEncoding::Ascii, 1, 32, 127, 0, 1, DisplayStyle::Number, "", {}, "Name", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.name.11", "Patch Name 11", 10, ParameterEncoding::Ascii, 1, 32, 127, 0, 1, DisplayStyle::Number, "", {}, "Name", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.name.12", "Patch Name 12", 11, ParameterEncoding::Ascii, 1, 32, 127, 0, 1, DisplayStyle::Number, "", {}, "Name", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.efx_type", "EFX Type", 12, ParameterEncoding::SevenBit, 1, 0, 39, 1, 1, DisplayStyle::Number, "", {}, "EFX", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.efx_parameter_1", "EFX Parameter 1", 13, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "EFX", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.efx_parameter_2", "EFX Parameter 2", 14, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "EFX", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.efx_parameter_3", "EFX Parameter 3", 15, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "EFX", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.efx_parameter_4", "EFX Parameter 4", 16, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "EFX", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.efx_parameter_5", "EFX Parameter 5", 17, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "EFX", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.efx_parameter_6", "EFX Parameter 6", 18, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "EFX", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.efx_parameter_7", "EFX Parameter 7", 19, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "EFX", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.efx_parameter_8", "EFX Parameter 8", 20, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "EFX", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.efx_parameter_9", "EFX Parameter 9", 21, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "EFX", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.efx_parameter_10", "EFX Parameter 10", 22, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "EFX", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.efx_parameter_11", "EFX Parameter 11", 23, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "EFX", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.efx_parameter_12", "EFX Parameter 12", 24, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "EFX", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.efx_output_assign", "EFX Output Assign", 25, ParameterEncoding::SevenBit, 1, 0, 2, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels1), "EFX", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.efx_mix_out_send_level", "EFX Mix Out Send Level", 26, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "EFX", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.efx_chorus_send_level", "EFX Chorus Send Level", 27, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "EFX", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.efx_reverb_send_level", "EFX Reverb Send Level", 28, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "EFX", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.efx_control_source_1", "EFX Control Source 1", 29, ParameterEncoding::SevenBit, 1, 0, 10, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels2), "EFX", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.efx_control_depth_1", "EFX Control Depth 1", 30, ParameterEncoding::SevenBit, 1, 0, 126, -63, 1, DisplayStyle::Number, "", {}, "EFX", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.efx_control_source_2", "EFX Control Source 2", 31, ParameterEncoding::SevenBit, 1, 0, 10, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels2), "EFX", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.efx_control_depth_2", "EFX Control Depth 2", 32, ParameterEncoding::SevenBit, 1, 0, 126, -63, 1, DisplayStyle::Number, "", {}, "EFX", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.chorus_level", "Chorus Level", 33, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Chorus", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.chorus_rate", "Chorus Rate", 34, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Chorus", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.chorus_depth", "Chorus Depth", 35, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Chorus", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.chorus_pre_delay", "Chorus Pre-Delay", 36, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Chorus", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.chorus_feedback", "Chorus Feedback", 37, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Chorus", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.chorus_output", "Chorus Output", 38, ParameterEncoding::SevenBit, 1, 0, 2, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels3), "Chorus", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.reverb_type", "Reverb Type", 39, ParameterEncoding::SevenBit, 1, 0, 7, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels4), "Reverb", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.reverb_level", "Reverb Level", 40, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Reverb", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.reverb_time", "Reverb Time", 41, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Reverb", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.reverb_hf_damp", "Reverb HF Damp", 42, ParameterEncoding::SevenBit, 1, 0, 17, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels5), "Reverb", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.delay_feedback", "Delay Feedback", 43, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Reverb", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.patch_tempo", "Patch Tempo", 44, ParameterEncoding::Nibble, 2, 20, 250, 0, 1, DisplayStyle::Number, "", {}, "Common", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.patch_level", "Patch Level", 46, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Common", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.patch_pan", "Patch Pan", 47, ParameterEncoding::SevenBit, 1, 0, 127, -64, 1, DisplayStyle::Pan, "", {}, "Pan", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.analog_feel", "Analog Feel", 48, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Common", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.bend_range_up", "Bend Range Up", 49, ParameterEncoding::SevenBit, 1, 0, 12, 0, 1, DisplayStyle::Number, "", {}, "Common", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.bend_range_down", "Bend Range Down", 50, ParameterEncoding::SevenBit, 1, 0, 48, 0, -1, DisplayStyle::Number, "", {}, "Common", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.key_assign_mode", "Key Assign Mode", 51, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels6), "Common", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.solo_legato", "Solo Legato", 52, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels7), "Common", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.portamento_switch", "Portamento Switch", 53, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels7), "Portamento", VerificationStatus::DocumentationDerived, "MIDI Implementation p.223"},
    ParameterDescriptor{"common.portamento_mode", "Portamento Mode", 54, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels8), "Portamento", VerificationStatus::DocumentationDerived, "MIDI Implementation pp.223-224"},
    ParameterDescriptor{"common.portamento_type", "Portamento Type", 55, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels9), "Portamento", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"common.portamento_start", "Portamento Start", 56, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels10), "Portamento", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"common.portamento_time", "Portamento Time", 57, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Portamento", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"common.patch_control_source_2", "Patch Control Source 2", 58, ParameterEncoding::SevenBit, 1, 0, 15, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels11), "Common", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"common.patch_control_source_3", "Patch Control Source 3", 59, ParameterEncoding::SevenBit, 1, 0, 15, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels11), "Common", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"common.efx_control_hold_peak", "EFX Control Hold/Peak", 60, ParameterEncoding::SevenBit, 1, 0, 2, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels12), "EFX", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"common.control_1_hold_peak", "Control 1 Hold/Peak", 61, ParameterEncoding::SevenBit, 1, 0, 2, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels12), "Common", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"common.control_2_hold_peak", "Control 2 Hold/Peak", 62, ParameterEncoding::SevenBit, 1, 0, 2, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels12), "Common", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"common.control_3_hold_peak", "Control 3 Hold/Peak", 63, ParameterEncoding::SevenBit, 1, 0, 2, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels12), "Common", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"common.velocity_range_switch", "Velocity Range Switch", 64, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels7), "Range", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"common.octave_shift", "Octave Shift", 65, ParameterEncoding::SevenBit, 1, 0, 6, -3, 1, DisplayStyle::Number, "", {}, "Common", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"common.stretch_tune_depth", "Stretch Tune Depth", 66, ParameterEncoding::SevenBit, 1, 0, 3, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels13), "Common", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"common.voice_priority", "Voice Priority", 67, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels14), "Common", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"common.structure_type_1_2", "Structure Type 1&2", 68, ParameterEncoding::SevenBit, 1, 0, 9, 1, 1, DisplayStyle::Number, "", {}, "Structure", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"common.booster_1_2", "Booster 1&2", 69, ParameterEncoding::SevenBit, 1, 0, 3, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels15), "Structure", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"common.structure_type_3_4", "Structure Type 3&4", 70, ParameterEncoding::SevenBit, 1, 0, 9, 1, 1, DisplayStyle::Number, "", {}, "Structure", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"common.booster_3_4", "Booster 3&4", 71, ParameterEncoding::SevenBit, 1, 0, 3, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels15), "Structure", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"common.clock_source", "Clock Source", 72, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels16), "Common", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
}};

const std::array<ParameterDescriptor, 128> kToneRows{{
    ParameterDescriptor{"tone.tone_switch", "Tone Switch", 0, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels7), "Tone", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.wave_group_type", "Wave Group Type", 1, ParameterEncoding::SevenBit, 1, 0, 2, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels17), "Wave", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.wave_group_id", "Wave Group ID", 2, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Wave", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.wave_number", "Wave Number", 3, ParameterEncoding::Nibble, 2, 0, 254, 1, 1, DisplayStyle::Number, "", {}, "Wave", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.wave_gain", "Wave Gain", 5, ParameterEncoding::SevenBit, 1, 0, 3, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels18), "Wave", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.fxm_switch", "FXM Switch", 6, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels7), "Wave", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.fxm_color", "FXM Color", 7, ParameterEncoding::SevenBit, 1, 0, 3, 1, 1, DisplayStyle::Number, "", {}, "Wave", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.fxm_depth", "FXM Depth", 8, ParameterEncoding::SevenBit, 1, 0, 15, 1, 1, DisplayStyle::Number, "", {}, "Wave", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.tone_delay_mode", "Tone Delay Mode", 9, ParameterEncoding::SevenBit, 1, 0, 7, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels19), "Tone Delay", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.tone_delay_time", "Tone Delay Time", 10, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Tone Delay", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.velocity_cross_fade", "Velocity Cross Fade", 11, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Range", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.velocity_range_lower", "Velocity Range Lower", 12, ParameterEncoding::SevenBit, 1, 1, 127, 0, 1, DisplayStyle::Number, "", {}, "Range", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.velocity_range_upper", "Velocity Range Upper", 13, ParameterEncoding::SevenBit, 1, 1, 127, 0, 1, DisplayStyle::Number, "", {}, "Range", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.keyboard_range_lower", "Keyboard Range Lower", 14, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::NoteName, "", {}, "Range", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.keyboard_range_upper", "Keyboard Range Upper", 15, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::NoteName, "", {}, "Range", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.redamper_control_switch", "Redamper Control Switch", 16, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels7), "Control Switches", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.volume_control_switch", "Volume Control Switch", 17, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels7), "Control Switches", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.hold_1_control_switch", "Hold-1 Control Switch", 18, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels7), "Control Switches", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.bender_control_switch", "Bender Control Switch", 19, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels7), "Control Switches", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.pan_control_switch", "Pan Control Switch", 20, ParameterEncoding::SevenBit, 1, 0, 2, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels20), "Control Switches", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.controller_1_destination_1", "Controller 1 Destination 1", 21, ParameterEncoding::SevenBit, 1, 0, 18, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels21), "Controllers", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.controller_1_depth_1", "Controller 1 Depth 1", 22, ParameterEncoding::SevenBit, 1, 0, 126, -63, 1, DisplayStyle::Number, "", {}, "Controllers", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.controller_1_destination_2", "Controller 1 Destination 2", 23, ParameterEncoding::SevenBit, 1, 0, 18, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels21), "Controllers", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.controller_1_depth_2", "Controller 1 Depth 2", 24, ParameterEncoding::SevenBit, 1, 0, 126, -63, 1, DisplayStyle::Number, "", {}, "Controllers", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.controller_1_destination_3", "Controller 1 Destination 3", 25, ParameterEncoding::SevenBit, 1, 0, 18, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels21), "Controllers", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.controller_1_depth_3", "Controller 1 Depth 3", 26, ParameterEncoding::SevenBit, 1, 0, 126, -63, 1, DisplayStyle::Number, "", {}, "Controllers", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.controller_1_destination_4", "Controller 1 Destination 4", 27, ParameterEncoding::SevenBit, 1, 0, 18, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels21), "Controllers", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.controller_1_depth_4", "Controller 1 Depth 4", 28, ParameterEncoding::SevenBit, 1, 0, 126, -63, 1, DisplayStyle::Number, "", {}, "Controllers", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.controller_2_destination_1", "Controller 2 Destination 1", 29, ParameterEncoding::SevenBit, 1, 0, 18, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels21), "Controllers", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.controller_2_depth_1", "Controller 2 Depth 1", 30, ParameterEncoding::SevenBit, 1, 0, 126, -63, 1, DisplayStyle::Number, "", {}, "Controllers", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.controller_2_destination_2", "Controller 2 Destination 2", 31, ParameterEncoding::SevenBit, 1, 0, 18, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels21), "Controllers", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.controller_2_depth_2", "Controller 2 Depth 2", 32, ParameterEncoding::SevenBit, 1, 0, 126, -63, 1, DisplayStyle::Number, "", {}, "Controllers", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.controller_2_destination_3", "Controller 2 Destination 3", 33, ParameterEncoding::SevenBit, 1, 0, 18, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels21), "Controllers", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.controller_2_depth_3", "Controller 2 Depth 3", 34, ParameterEncoding::SevenBit, 1, 0, 126, -63, 1, DisplayStyle::Number, "", {}, "Controllers", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.controller_2_destination_4", "Controller 2 Destination 4", 35, ParameterEncoding::SevenBit, 1, 0, 18, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels21), "Controllers", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.controller_2_depth_4", "Controller 2 Depth 4", 36, ParameterEncoding::SevenBit, 1, 0, 126, -63, 1, DisplayStyle::Number, "", {}, "Controllers", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.controller_3_destination_1", "Controller 3 Destination 1", 37, ParameterEncoding::SevenBit, 1, 0, 18, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels21), "Controllers", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.controller_3_depth_1", "Controller 3 Depth 1", 38, ParameterEncoding::SevenBit, 1, 0, 126, -63, 1, DisplayStyle::Number, "", {}, "Controllers", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.controller_3_destination_2", "Controller 3 Destination 2", 39, ParameterEncoding::SevenBit, 1, 0, 18, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels21), "Controllers", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.controller_3_depth_2", "Controller 3 Depth 2", 40, ParameterEncoding::SevenBit, 1, 0, 126, -63, 1, DisplayStyle::Number, "", {}, "Controllers", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.controller_3_destination_3", "Controller 3 Destination 3", 41, ParameterEncoding::SevenBit, 1, 0, 18, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels21), "Controllers", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.controller_3_depth_3", "Controller 3 Depth 3", 42, ParameterEncoding::SevenBit, 1, 0, 126, -63, 1, DisplayStyle::Number, "", {}, "Controllers", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.controller_3_destination_4", "Controller 3 Destination 4", 43, ParameterEncoding::SevenBit, 1, 0, 18, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels21), "Controllers", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.controller_3_depth_4", "Controller 3 Depth 4", 44, ParameterEncoding::SevenBit, 1, 0, 126, -63, 1, DisplayStyle::Number, "", {}, "Controllers", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.lfo1_waveform", "LFO1 Waveform", 45, ParameterEncoding::SevenBit, 1, 0, 7, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels22), "Wave", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.lfo1_key_trigger", "LFO1 Key Trigger", 46, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels7), "LFO1", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.lfo1_rate", "LFO1 Rate", 47, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "LFO1", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.lfo1_offset", "LFO1 Offset", 48, ParameterEncoding::SevenBit, 1, 0, 4, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels23), "LFO1", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.lfo1_delay_time", "LFO1 Delay Time", 49, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "LFO1", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.lfo1_fade_mode", "LFO1 Fade Mode", 50, ParameterEncoding::SevenBit, 1, 0, 3, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels24), "LFO1", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.lfo1_fade_time", "LFO1 Fade Time", 51, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "LFO1", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.lfo1_external_sync", "LFO1 External Sync", 52, ParameterEncoding::SevenBit, 1, 0, 2, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels25), "LFO1", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.lfo2_waveform", "LFO2 Waveform", 53, ParameterEncoding::SevenBit, 1, 0, 7, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels22), "Wave", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.lfo2_key_trigger", "LFO2 Key Trigger", 54, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels7), "LFO2", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.lfo2_rate", "LFO2 Rate", 55, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "LFO2", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.lfo2_offset", "LFO2 Offset", 56, ParameterEncoding::SevenBit, 1, 0, 4, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels23), "LFO2", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.lfo2_delay_time", "LFO2 Delay Time", 57, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "LFO2", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.lfo2_fade_mode", "LFO2 Fade Mode", 58, ParameterEncoding::SevenBit, 1, 0, 3, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels24), "LFO2", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.lfo2_fade_time", "LFO2 Fade Time", 59, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "LFO2", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.lfo2_external_sync", "LFO2 External Sync", 60, ParameterEncoding::SevenBit, 1, 0, 2, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels25), "LFO2", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.coarse_tune", "Coarse Tune", 61, ParameterEncoding::SevenBit, 1, 0, 96, -48, 1, DisplayStyle::Number, "", {}, "Pitch", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.fine_tune", "Fine Tune", 62, ParameterEncoding::SevenBit, 1, 0, 100, -50, 1, DisplayStyle::Number, "", {}, "Pitch", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.random_pitch_depth", "Random Pitch Depth", 63, ParameterEncoding::SevenBit, 1, 0, 30, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels26), "Pitch", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.pitch_keyfollow", "Pitch Keyfollow", 64, ParameterEncoding::SevenBit, 1, 0, 15, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels27), "Pitch", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.pitch_envelope_depth", "Pitch Envelope Depth", 65, ParameterEncoding::SevenBit, 1, 0, 24, -12, 1, DisplayStyle::Number, "", {}, "Pitch Envelope", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.pitch_envelope_velocity_sens", "Pitch Envelope Velocity Sens", 66, ParameterEncoding::SevenBit, 1, 0, 125, -100, 2, DisplayStyle::Number, "", {}, "Pitch Envelope", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.pitch_envelope_velocity_time1", "Pitch Envelope Velocity Time1", 67, ParameterEncoding::SevenBit, 1, 0, 14, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels28), "Pitch Envelope", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.pitch_envelope_velocity_time4", "Pitch Envelope Velocity Time4", 68, ParameterEncoding::SevenBit, 1, 0, 14, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels28), "Pitch Envelope", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.pitch_envelope_time_keyfollow", "Pitch Envelope Time Keyfollow", 69, ParameterEncoding::SevenBit, 1, 0, 14, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels28), "Pitch Envelope", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.pitch_envelope_time_1", "Pitch Envelope Time 1", 70, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Pitch Envelope", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.pitch_envelope_time_2", "Pitch Envelope Time 2", 71, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Pitch Envelope", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.pitch_envelope_time_3", "Pitch Envelope Time 3", 72, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Pitch Envelope", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.pitch_envelope_time_4", "Pitch Envelope Time 4", 73, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Pitch Envelope", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.pitch_envelope_level_1", "Pitch Envelope Level 1", 74, ParameterEncoding::SevenBit, 1, 0, 126, -63, 1, DisplayStyle::Number, "", {}, "Pitch Envelope", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.pitch_envelope_level_2", "Pitch Envelope Level 2", 75, ParameterEncoding::SevenBit, 1, 0, 126, -63, 1, DisplayStyle::Number, "", {}, "Pitch Envelope", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.pitch_envelope_level_3", "Pitch Envelope Level 3", 76, ParameterEncoding::SevenBit, 1, 0, 126, -63, 1, DisplayStyle::Number, "", {}, "Pitch Envelope", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.pitch_envelope_level_4", "Pitch Envelope Level 4", 77, ParameterEncoding::SevenBit, 1, 0, 126, -63, 1, DisplayStyle::Number, "", {}, "Pitch Envelope", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.pitch_lfo1_depth", "Pitch LFO1 Depth", 78, ParameterEncoding::SevenBit, 1, 0, 126, -63, 1, DisplayStyle::Number, "", {}, "LFO1", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.pitch_lfo2_depth", "Pitch LFO2 Depth", 79, ParameterEncoding::SevenBit, 1, 0, 126, -63, 1, DisplayStyle::Number, "", {}, "LFO2", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.filter_type", "Filter Type", 80, ParameterEncoding::SevenBit, 1, 0, 4, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels29), "TVF", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.cutoff_frequency", "Cutoff Frequency", 81, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "TVF", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.cutoff_keyfollow", "Cutoff Keyfollow", 82, ParameterEncoding::SevenBit, 1, 0, 15, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels27), "TVF", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.resonance", "Resonance", 83, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "TVF", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.resonance_velocity_sens", "Resonance Velocity Sens", 84, ParameterEncoding::SevenBit, 1, 0, 125, -100, 2, DisplayStyle::Number, "", {}, "TVF", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.filter_envelope_depth", "Filter Envelope Depth", 85, ParameterEncoding::SevenBit, 1, 0, 126, -63, 1, DisplayStyle::Number, "", {}, "TVF Envelope", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.filter_envelope_velocity_curve", "Filter Envelope Velocity Curve", 86, ParameterEncoding::SevenBit, 1, 0, 6, 1, 1, DisplayStyle::Number, "", {}, "TVF Envelope", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.filter_envelope_velocity_sens", "Filter Envelope Velocity Sens", 87, ParameterEncoding::SevenBit, 1, 0, 125, -100, 2, DisplayStyle::Number, "", {}, "TVF Envelope", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.filter_envelope_velocity_time1", "Filter Envelope Velocity Time1", 88, ParameterEncoding::SevenBit, 1, 0, 14, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels28), "TVF Envelope", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.filter_envelope_velocity_time4", "Filter Envelope Velocity Time4", 89, ParameterEncoding::SevenBit, 1, 0, 14, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels28), "TVF Envelope", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.filter_envelope_time_keyfollow", "Filter Envelope Time Keyfollow", 90, ParameterEncoding::SevenBit, 1, 0, 14, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels28), "TVF Envelope", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.filter_envelope_time_1", "Filter Envelope Time 1", 91, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "TVF Envelope", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.filter_envelope_time_2", "Filter Envelope Time 2", 92, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "TVF Envelope", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.filter_envelope_time_3", "Filter Envelope Time 3", 93, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "TVF Envelope", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.filter_envelope_time_4", "Filter Envelope Time 4", 94, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "TVF Envelope", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.filter_envelope_level_1", "Filter Envelope Level 1", 95, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "TVF Envelope", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.filter_envelope_level_2", "Filter Envelope Level 2", 96, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "TVF Envelope", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.filter_envelope_level_3", "Filter Envelope Level 3", 97, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "TVF Envelope", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.filter_envelope_level_4", "Filter Envelope Level 4", 98, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "TVF Envelope", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.filter_lfo1_depth", "Filter LFO1 Depth", 99, ParameterEncoding::SevenBit, 1, 0, 126, -63, 1, DisplayStyle::Number, "", {}, "LFO1", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.filter_lfo2_depth", "Filter LFO2 Depth", 100, ParameterEncoding::SevenBit, 1, 0, 126, -63, 1, DisplayStyle::Number, "", {}, "LFO2", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.tone_level", "Tone Level", 101, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "TVA", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.bias_direction", "Bias Direction", 102, ParameterEncoding::SevenBit, 1, 0, 3, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels30), "TVA", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.bias_position", "Bias Position", 103, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::NoteName, "", {}, "TVA", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.bias_level", "Bias Level", 104, ParameterEncoding::SevenBit, 1, 0, 14, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels28), "TVA", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.level_envelope_velocity_curve", "Level Envelope Velocity Curve", 105, ParameterEncoding::SevenBit, 1, 0, 6, 1, 1, DisplayStyle::Number, "", {}, "TVA Envelope", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.level_envelope_velocity_sens", "Level Envelope Velocity Sens", 106, ParameterEncoding::SevenBit, 1, 0, 125, -100, 2, DisplayStyle::Number, "", {}, "TVA Envelope", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.level_envelope_velocity_time1", "Level Envelope Velocity Time1", 107, ParameterEncoding::SevenBit, 1, 0, 14, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels28), "TVA Envelope", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.level_envelope_velocity_time4", "Level Envelope Velocity Time4", 108, ParameterEncoding::SevenBit, 1, 0, 14, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels28), "TVA Envelope", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.level_envelope_time_keyfollow", "Level Envelope Time Keyfollow", 109, ParameterEncoding::SevenBit, 1, 0, 14, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels28), "TVA Envelope", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.level_envelope_time_1", "Level Envelope Time 1", 110, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "TVA Envelope", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.level_envelope_time_2", "Level Envelope Time 2", 111, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "TVA Envelope", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.level_envelope_time_3", "Level Envelope Time 3", 112, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "TVA Envelope", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.level_envelope_time_4", "Level Envelope Time 4", 113, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "TVA Envelope", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.level_envelope_level_1", "Level Envelope Level 1", 114, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "TVA Envelope", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.level_envelope_level_2", "Level Envelope Level 2", 115, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "TVA Envelope", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.level_envelope_level_3", "Level Envelope Level 3", 116, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "TVA Envelope", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.level_lfo1_depth", "Level LFO1 Depth", 117, ParameterEncoding::SevenBit, 1, 0, 126, -63, 1, DisplayStyle::Number, "", {}, "LFO1", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.level_lfo2_depth", "Level LFO2 Depth", 118, ParameterEncoding::SevenBit, 1, 0, 126, -63, 1, DisplayStyle::Number, "", {}, "LFO2", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.tone_pan", "Tone Pan", 119, ParameterEncoding::SevenBit, 1, 0, 127, -64, 1, DisplayStyle::Pan, "", {}, "Pan", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.pan_keyfollow", "Pan Keyfollow", 120, ParameterEncoding::SevenBit, 1, 0, 14, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels28), "Pan", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.random_pan_depth", "Random Pan Depth", 121, ParameterEncoding::SevenBit, 1, 0, 63, 0, 1, DisplayStyle::Number, "", {}, "Pan", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.alternate_pan_depth", "Alternate Pan Depth", 122, ParameterEncoding::SevenBit, 1, 1, 127, -64, 1, DisplayStyle::Pan, "", {}, "Pan", VerificationStatus::DocumentationDerived, "MIDI Implementation p.224"},
    ParameterDescriptor{"tone.pan_lfo1_depth", "Pan LFO1 Depth", 123, ParameterEncoding::SevenBit, 1, 0, 126, -63, 1, DisplayStyle::Number, "", {}, "LFO1", VerificationStatus::DocumentationDerived, "MIDI Implementation p.225"},
    ParameterDescriptor{"tone.pan_lfo2_depth", "Pan LFO2 Depth", 124, ParameterEncoding::SevenBit, 1, 0, 126, -63, 1, DisplayStyle::Number, "", {}, "LFO2", VerificationStatus::DocumentationDerived, "MIDI Implementation p.225"},
    ParameterDescriptor{"tone.output_assign", "Output Assign", 125, ParameterEncoding::SevenBit, 1, 0, 3, 0, 1, DisplayStyle::Number, "", std::span<const std::string_view>(kLabels31), "Output", VerificationStatus::DocumentationDerived, "MIDI Implementation p.225"},
    ParameterDescriptor{"tone.mix_efx_send_level", "Mix/EFX Send Level", 126, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "EFX", VerificationStatus::DocumentationDerived, "MIDI Implementation p.225"},
    ParameterDescriptor{"tone.chorus_send_level", "Chorus Send Level", 127, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Chorus", VerificationStatus::DocumentationDerived, "MIDI Implementation p.225"},
    ParameterDescriptor{"tone.reverb_send_level", "Reverb Send Level", 128, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Reverb", VerificationStatus::DocumentationDerived, "MIDI Implementation p.225"},
}};

} // namespace

const ParameterTable& patchCommonTable() noexcept
{
    static const ParameterTable kTable{"XP-60 Patch Common", kPatchCommonSize,
                                       std::span<const ParameterDescriptor>(kCommonRows.data(), kCommonRows.size()),
                                       TableCompleteness::Complete,
                                       "Roland XP-60/XP-80 MIDI Implementation, Parameter Address Map 1-3-1 (pp.223-224)"};
    return kTable;
}

const ParameterTable& patchToneTable() noexcept
{
    static const ParameterTable kTable{"XP-60 Patch Tone", kPatchToneSize,
                                       std::span<const ParameterDescriptor>(kToneRows.data(), kToneRows.size()),
                                       TableCompleteness::Complete,
                                       "Roland XP-60/XP-80 MIDI Implementation, Parameter Address Map 1-3-2 (pp.224-225)"};
    return kTable;
}

const ParameterDescriptor& descriptor(CommonParameter parameter) noexcept
{
    return kCommonRows[static_cast<std::size_t>(parameter)];
}

const ParameterDescriptor& descriptor(ToneParameter parameter) noexcept
{
    return kToneRows[static_cast<std::size_t>(parameter)];
}

} // namespace xp60studio::xpmodel::xp60tables
