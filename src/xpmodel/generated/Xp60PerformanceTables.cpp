// GENERATED FILE — DO NOT EDIT. See Xp60PerformanceTables.h for provenance.
// Source digest: sha256:1b4012b07f799209
#include "xpmodel/generated/Xp60PerformanceTables.h"

#include "xp60/Xp60Device.h"

namespace xp60studio::xpmodel::xp60performance {

namespace {

using xp60::VerificationStatus;

constexpr std::array<std::string_view, 16> kLabels1{{"PERFORM", "1", "2", "3", "4", "5", "6", "7", "8", "9", "11", "12", "13", "14", "15", "16"}};
constexpr std::array<std::string_view, 3> kLabels2{{"MIX", "DIR", "<OUTPUT-2>"}};
constexpr std::array<std::string_view, 11> kLabels3{{"OFF", "SYS-CTRL1", "SYS-CTRL2", "MODULATION", "BREATH", "FOOT", "VOLUME", "PAN", "EXPRESSION", "PITCH BEND", "AFTERTOUCH"}};
constexpr std::array<std::string_view, 3> kLabels4{{"MIX", "REVERB", "MIX+REV"}};
constexpr std::array<std::string_view, 8> kLabels5{{"ROOM1", "ROOM2", "STAGE1", "STAGE2", "HALL1", "HALL2", "DELAY", "PAN-DLY"}};
constexpr std::array<std::string_view, 18> kLabels6{{"200", "250", "315", "400", "500", "630", "800", "1000", "1250", "1600", "2000", "2500", "3150", "4000", "5000", "6300", "8000", "BYPASS"}};
constexpr std::array<std::string_view, 2> kLabels7{{"OFF", "ON"}};
constexpr std::array<std::string_view, 2> kLabels8{{"LAYER", "SINGLE"}};
constexpr std::array<std::string_view, 2> kLabels9{{"PERFORMANCE", "SEQUENCER"}};
constexpr std::array<std::string_view, 3> kLabels10{{"USER&PRESET", "<PCM>", "EXP"}};
constexpr std::array<std::string_view, 5> kLabels11{{"MIX", "EFX", "DIR", "<OUTPUT-2>", "PAT"}};
constexpr std::array<std::string_view, 8> kLabels12{{"PATCH", "GROUP1", "GROUP2", "GROUP3", "GROUP4", "GROUP5", "GROUP6", "GROUP7"}};
constexpr std::array<std::string_view, 129> kLabels13{{"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20", "21", "22", "23", "24", "25", "26", "27", "28", "29", "30", "31", "32", "33", "34", "35", "36", "37", "38", "39", "40", "41", "42", "43", "44", "45", "46", "47", "48", "49", "50", "51", "52", "53", "54", "55", "56", "57", "58", "59", "60", "61", "62", "63", "64", "65", "66", "67", "68", "69", "70", "71", "72", "73", "74", "75", "76", "77", "78", "79", "80", "81", "82", "83", "84", "85", "86", "87", "88", "89", "90", "91", "92", "93", "94", "95", "96", "97", "98", "99", "100", "101", "102", "103", "104", "105", "106", "107", "108", "109", "110", "111", "112", "113", "114", "115", "116", "117", "118", "119", "120", "121", "122", "123", "124", "125", "126", "127", "OFF"}};

const std::array<ParameterDescriptor, 65> kCommonRows{{
    ParameterDescriptor{"common.name.1", "Performance Name 1", 0, ParameterEncoding::Ascii, 1, 32, 127, 0, 1, DisplayStyle::Number, "", {}, "Name", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.name.2", "Performance Name 2", 1, ParameterEncoding::Ascii, 1, 32, 127, 0, 1, DisplayStyle::Number, "", {}, "Name", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.name.3", "Performance Name 3", 2, ParameterEncoding::Ascii, 1, 32, 127, 0, 1, DisplayStyle::Number, "", {}, "Name", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.name.4", "Performance Name 4", 3, ParameterEncoding::Ascii, 1, 32, 127, 0, 1, DisplayStyle::Number, "", {}, "Name", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.name.5", "Performance Name 5", 4, ParameterEncoding::Ascii, 1, 32, 127, 0, 1, DisplayStyle::Number, "", {}, "Name", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.name.6", "Performance Name 6", 5, ParameterEncoding::Ascii, 1, 32, 127, 0, 1, DisplayStyle::Number, "", {}, "Name", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.name.7", "Performance Name 7", 6, ParameterEncoding::Ascii, 1, 32, 127, 0, 1, DisplayStyle::Number, "", {}, "Name", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.name.8", "Performance Name 8", 7, ParameterEncoding::Ascii, 1, 32, 127, 0, 1, DisplayStyle::Number, "", {}, "Name", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.name.9", "Performance Name 9", 8, ParameterEncoding::Ascii, 1, 32, 127, 0, 1, DisplayStyle::Number, "", {}, "Name", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.name.10", "Performance Name 10", 9, ParameterEncoding::Ascii, 1, 32, 127, 0, 1, DisplayStyle::Number, "", {}, "Name", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.name.11", "Performance Name 11", 10, ParameterEncoding::Ascii, 1, 32, 127, 0, 1, DisplayStyle::Number, "", {}, "Name", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.name.12", "Performance Name 12", 11, ParameterEncoding::Ascii, 1, 32, 127, 0, 1, DisplayStyle::Number, "", {}, "Name", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.efx_source", "EFX Source", 12, ParameterEncoding::SevenBit, 1, 0, 15, 0, 1, DisplayStyle::Number, "", kLabels1, "EFX", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1 note *1"},
    ParameterDescriptor{"common.efx_type", "EFX Type", 13, ParameterEncoding::SevenBit, 1, 0, 39, 1, 1, DisplayStyle::Number, "", {}, "EFX", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.efx_parameter_1", "EFX Parameter 1", 14, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "EFX", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.efx_parameter_2", "EFX Parameter 2", 15, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "EFX", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.efx_parameter_3", "EFX Parameter 3", 16, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "EFX", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.efx_parameter_4", "EFX Parameter 4", 17, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "EFX", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.efx_parameter_5", "EFX Parameter 5", 18, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "EFX", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.efx_parameter_6", "EFX Parameter 6", 19, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "EFX", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.efx_parameter_7", "EFX Parameter 7", 20, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "EFX", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.efx_parameter_8", "EFX Parameter 8", 21, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "EFX", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.efx_parameter_9", "EFX Parameter 9", 22, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "EFX", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.efx_parameter_10", "EFX Parameter 10", 23, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "EFX", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.efx_parameter_11", "EFX Parameter 11", 24, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "EFX", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.efx_parameter_12", "EFX Parameter 12", 25, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "EFX", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.efx_output_assign", "EFX Output Assign", 26, ParameterEncoding::SevenBit, 1, 0, 2, 0, 1, DisplayStyle::Number, "", kLabels2, "EFX", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1 note *2"},
    ParameterDescriptor{"common.efx_mix_out_send_level", "EFX Mix Out Send Level", 27, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "EFX", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.efx_chorus_send_level", "EFX Chorus Send Level", 28, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "EFX", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.efx_reverb_send_level", "EFX Reverb Send Level", 29, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "EFX", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.efx_control_source_1", "EFX Control Source 1", 30, ParameterEncoding::SevenBit, 1, 0, 10, 0, 1, DisplayStyle::Number, "", kLabels3, "EFX", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1 note *3"},
    ParameterDescriptor{"common.efx_control_depth_1", "EFX Control Depth 1", 31, ParameterEncoding::SevenBit, 1, 0, 126, -63, 1, DisplayStyle::Number, "", {}, "EFX", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.efx_control_source_2", "EFX Control Source 2", 32, ParameterEncoding::SevenBit, 1, 0, 10, 0, 1, DisplayStyle::Number, "", kLabels3, "EFX", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1 note *3"},
    ParameterDescriptor{"common.efx_control_depth_2", "EFX Control Depth 2", 33, ParameterEncoding::SevenBit, 1, 0, 126, -63, 1, DisplayStyle::Number, "", {}, "EFX", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.chorus_level", "Chorus Level", 34, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Chorus", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.chorus_rate", "Chorus Rate", 35, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Chorus", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.chorus_depth", "Chorus Depth", 36, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Chorus", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.chorus_pre_delay", "Chorus Pre-Delay", 37, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Chorus", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.chorus_feedback", "Chorus Feedback", 38, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Chorus", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.chorus_output", "Chorus Output", 39, ParameterEncoding::SevenBit, 1, 0, 2, 0, 1, DisplayStyle::Number, "", kLabels4, "Chorus", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1 note *4"},
    ParameterDescriptor{"common.reverb_type", "Reverb Type", 40, ParameterEncoding::SevenBit, 1, 0, 7, 0, 1, DisplayStyle::Number, "", kLabels5, "Reverb", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1 note *5"},
    ParameterDescriptor{"common.reverb_level", "Reverb Level", 41, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Reverb", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.reverb_time", "Reverb Time", 42, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Reverb", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.reverb_hf_damp", "Reverb HF Damp", 43, ParameterEncoding::SevenBit, 1, 0, 17, 0, 1, DisplayStyle::Number, "", kLabels6, "Reverb", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1 note *6"},
    ParameterDescriptor{"common.delay_feedback", "Delay Feedback", 44, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Reverb", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.performance_tempo", "Performance Tempo", 45, ParameterEncoding::Nibble, 2, 20, 250, 0, 1, DisplayStyle::Number, "", {}, "Tempo", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.keyboard_range_switch", "Keyboard Range Switch", 47, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels7, "Range", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.voice_reserve_1", "Voice Reserve 1", 48, ParameterEncoding::SevenBit, 1, 0, 64, 0, 1, DisplayStyle::Number, "", {}, "Voice Reserve", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.voice_reserve_2", "Voice Reserve 2", 49, ParameterEncoding::SevenBit, 1, 0, 64, 0, 1, DisplayStyle::Number, "", {}, "Voice Reserve", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.voice_reserve_3", "Voice Reserve 3", 50, ParameterEncoding::SevenBit, 1, 0, 64, 0, 1, DisplayStyle::Number, "", {}, "Voice Reserve", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.voice_reserve_4", "Voice Reserve 4", 51, ParameterEncoding::SevenBit, 1, 0, 64, 0, 1, DisplayStyle::Number, "", {}, "Voice Reserve", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.voice_reserve_5", "Voice Reserve 5", 52, ParameterEncoding::SevenBit, 1, 0, 64, 0, 1, DisplayStyle::Number, "", {}, "Voice Reserve", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.voice_reserve_6", "Voice Reserve 6", 53, ParameterEncoding::SevenBit, 1, 0, 64, 0, 1, DisplayStyle::Number, "", {}, "Voice Reserve", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.voice_reserve_7", "Voice Reserve 7", 54, ParameterEncoding::SevenBit, 1, 0, 64, 0, 1, DisplayStyle::Number, "", {}, "Voice Reserve", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.voice_reserve_8", "Voice Reserve 8", 55, ParameterEncoding::SevenBit, 1, 0, 64, 0, 1, DisplayStyle::Number, "", {}, "Voice Reserve", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.voice_reserve_9", "Voice Reserve 9", 56, ParameterEncoding::SevenBit, 1, 0, 64, 0, 1, DisplayStyle::Number, "", {}, "Voice Reserve", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.voice_reserve_10", "Voice Reserve 10", 57, ParameterEncoding::SevenBit, 1, 0, 64, 0, 1, DisplayStyle::Number, "", {}, "Voice Reserve", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.voice_reserve_11", "Voice Reserve 11", 58, ParameterEncoding::SevenBit, 1, 0, 64, 0, 1, DisplayStyle::Number, "", {}, "Voice Reserve", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.voice_reserve_12", "Voice Reserve 12", 59, ParameterEncoding::SevenBit, 1, 0, 64, 0, 1, DisplayStyle::Number, "", {}, "Voice Reserve", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.voice_reserve_13", "Voice Reserve 13", 60, ParameterEncoding::SevenBit, 1, 0, 64, 0, 1, DisplayStyle::Number, "", {}, "Voice Reserve", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.voice_reserve_14", "Voice Reserve 14", 61, ParameterEncoding::SevenBit, 1, 0, 64, 0, 1, DisplayStyle::Number, "", {}, "Voice Reserve", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.voice_reserve_15", "Voice Reserve 15", 62, ParameterEncoding::SevenBit, 1, 0, 64, 0, 1, DisplayStyle::Number, "", {}, "Voice Reserve", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.voice_reserve_16", "Voice Reserve 16", 63, ParameterEncoding::SevenBit, 1, 0, 64, 0, 1, DisplayStyle::Number, "", {}, "Voice Reserve", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.keyboard_mode", "Keyboard Mode", 64, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels8, "Keyboard", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1"},
    ParameterDescriptor{"common.clock_source", "Clock Source", 65, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels9, "Tempo", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-1 note *7"},
}};

const std::array<ParameterDescriptor, 23> kPartRows{{
    ParameterDescriptor{"part.receive_switch", "Receive Switch", 0, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels7, "MIDI", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-2"},
    ParameterDescriptor{"part.midi_channel", "MIDI Channel", 1, ParameterEncoding::SevenBit, 1, 0, 15, 1, 1, DisplayStyle::Number, "", {}, "MIDI", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-2"},
    ParameterDescriptor{"part.patch_group_type", "Patch Group Type", 2, ParameterEncoding::SevenBit, 1, 0, 2, 0, 1, DisplayStyle::Number, "", kLabels10, "Patch", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-2 note *1"},
    ParameterDescriptor{"part.patch_group_id", "Patch Group ID", 3, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Patch", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-2"},
    ParameterDescriptor{"part.patch_number", "Patch Number", 4, ParameterEncoding::Nibble, 2, 0, 254, 1, 1, DisplayStyle::Number, "", {}, "Patch", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-2"},
    ParameterDescriptor{"part.part_level", "Part Level", 6, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Mix", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-2"},
    ParameterDescriptor{"part.part_pan", "Part Pan", 7, ParameterEncoding::SevenBit, 1, 0, 127, -64, 1, DisplayStyle::Pan, "", {}, "Mix", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-2"},
    ParameterDescriptor{"part.part_coarse_tune", "Part Coarse Tune", 8, ParameterEncoding::SevenBit, 1, 0, 96, -48, 1, DisplayStyle::Number, "", {}, "Pitch", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-2"},
    ParameterDescriptor{"part.part_fine_tune", "Part Fine Tune", 9, ParameterEncoding::SevenBit, 1, 0, 100, -50, 1, DisplayStyle::Number, "", {}, "Pitch", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-2"},
    ParameterDescriptor{"part.output_assign", "Output Assign", 10, ParameterEncoding::SevenBit, 1, 0, 4, 0, 1, DisplayStyle::Number, "", kLabels11, "Mix", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-2 note *2"},
    ParameterDescriptor{"part.mix_efx_send_level", "Mix/EFX Send Level", 11, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "EFX", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-2"},
    ParameterDescriptor{"part.chorus_send_level", "Chorus Send Level", 12, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Chorus", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-2"},
    ParameterDescriptor{"part.reverb_send_level", "Reverb Send Level", 13, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Reverb", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-2"},
    ParameterDescriptor{"part.receive_program_change_switch", "Receive Program Change Switch", 14, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels7, "MIDI", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-2"},
    ParameterDescriptor{"part.receive_volume_switch", "Receive Volume Switch", 15, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels7, "MIDI", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-2"},
    ParameterDescriptor{"part.receive_hold_1_switch", "Receive Hold-1 Switch", 16, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels7, "MIDI", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-2"},
    ParameterDescriptor{"part.keyboard_range_lower", "Keyboard Range Lower", 17, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::NoteName, "", {}, "Range", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-2"},
    ParameterDescriptor{"part.keyboard_range_upper", "Keyboard Range Upper", 18, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::NoteName, "", {}, "Range", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-2"},
    ParameterDescriptor{"part.octave_shift", "Octave Shift", 19, ParameterEncoding::SevenBit, 1, 0, 6, -3, 1, DisplayStyle::Number, "", {}, "Pitch", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-2"},
    ParameterDescriptor{"part.local_switch", "Local Switch", 20, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels7, "MIDI", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-2"},
    ParameterDescriptor{"part.transmit_switch", "Transmit Switch", 21, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels7, "MIDI", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-2"},
    ParameterDescriptor{"part.transmit_bank_select_group", "Transmit Bank Select Group", 22, ParameterEncoding::SevenBit, 1, 0, 7, 0, 1, DisplayStyle::Number, "", kLabels12, "MIDI", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-2 note *3"},
    ParameterDescriptor{"part.transmit_volume", "Transmit Volume", 23, ParameterEncoding::Nibble, 2, 0, 128, 0, 1, DisplayStyle::Number, "", kLabels13, "MIDI", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-2-2"},
}};

} // namespace

const ParameterTable& performanceCommonTable() noexcept
{
    static const ParameterTable kTable{"XP-60 Performance Common", kPerformanceCommonSize,
                                       std::span<const ParameterDescriptor>(kCommonRows.data(), kCommonRows.size()),
                                       TableCompleteness::Complete,
                                       "Roland XP-60/XP-80 MIDI Implementation, Parameter Address Map 1-2-1"};
    return kTable;
}

const ParameterTable& performancePartTable() noexcept
{
    static const ParameterTable kTable{"XP-60 Performance Part", kPerformancePartSize,
                                       std::span<const ParameterDescriptor>(kPartRows.data(), kPartRows.size()),
                                       TableCompleteness::Complete,
                                       "Roland XP-60/XP-80 MIDI Implementation, Parameter Address Map 1-2-2"};
    return kTable;
}

const ParameterDescriptor& descriptor(PerformanceCommonParameter parameter) noexcept
{
    return kCommonRows[static_cast<std::size_t>(parameter)];
}

const ParameterDescriptor& descriptor(PerformancePartParameter parameter) noexcept
{
    return kPartRows[static_cast<std::size_t>(parameter)];
}

} // namespace xp60studio::xpmodel::xp60performance
