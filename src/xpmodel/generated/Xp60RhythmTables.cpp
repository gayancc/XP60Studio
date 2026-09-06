// GENERATED FILE — DO NOT EDIT. See Xp60RhythmTables.h for provenance.
// Source digest: sha256:4938876e8039d2b4
#include "xpmodel/generated/Xp60RhythmTables.h"

#include "xp60/Xp60Device.h"

namespace xp60studio::xpmodel::xp60rhythm {

namespace {

using xp60::VerificationStatus;

constexpr std::array<std::string_view, 2> kLabels1{{"OFF", "ON"}};
constexpr std::array<std::string_view, 3> kLabels2{{"INT", "<PCM>", "EXP"}};
constexpr std::array<std::string_view, 4> kLabels3{{"-6", "0", "+6", "+12"}};
constexpr std::array<std::string_view, 33> kLabels4{{"OFF", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20", "21", "22", "23", "24", "25", "26", "27", "28", "29", "30", "31", "32"}};
constexpr std::array<std::string_view, 2> kLabels5{{"NO-SUS", "SUSTAIN"}};
constexpr std::array<std::string_view, 3> kLabels6{{"OFF", "CONTINUOUS", "KEY-ON"}};
constexpr std::array<std::string_view, 31> kLabels7{{"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "20", "30", "40", "50", "60", "70", "80", "90", "100", "200", "300", "400", "500", "600", "700", "800", "900", "1000", "1100", "1200"}};
constexpr std::array<std::string_view, 15> kLabels8{{"-100", "-70", "-50", "-40", "-30", "-20", "-10", "0", "+10", "+20", "+30", "+40", "+50", "+70", "+100"}};
constexpr std::array<std::string_view, 5> kLabels9{{"OFF", "LPF", "BPF", "HPF", "PKG"}};
constexpr std::array<std::string_view, 4> kLabels10{{"MIX", "EFX", "DIR", "<OUTPUT-2>"}};

const std::array<ParameterDescriptor, 12> kCommonRows{{
    ParameterDescriptor{"common.name.1", "Rhythm Name 1", 0, ParameterEncoding::Ascii, 1, 32, 127, 0, 1, DisplayStyle::Number, "", {}, "Name", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-1"},
    ParameterDescriptor{"common.name.2", "Rhythm Name 2", 1, ParameterEncoding::Ascii, 1, 32, 127, 0, 1, DisplayStyle::Number, "", {}, "Name", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-1"},
    ParameterDescriptor{"common.name.3", "Rhythm Name 3", 2, ParameterEncoding::Ascii, 1, 32, 127, 0, 1, DisplayStyle::Number, "", {}, "Name", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-1"},
    ParameterDescriptor{"common.name.4", "Rhythm Name 4", 3, ParameterEncoding::Ascii, 1, 32, 127, 0, 1, DisplayStyle::Number, "", {}, "Name", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-1"},
    ParameterDescriptor{"common.name.5", "Rhythm Name 5", 4, ParameterEncoding::Ascii, 1, 32, 127, 0, 1, DisplayStyle::Number, "", {}, "Name", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-1"},
    ParameterDescriptor{"common.name.6", "Rhythm Name 6", 5, ParameterEncoding::Ascii, 1, 32, 127, 0, 1, DisplayStyle::Number, "", {}, "Name", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-1"},
    ParameterDescriptor{"common.name.7", "Rhythm Name 7", 6, ParameterEncoding::Ascii, 1, 32, 127, 0, 1, DisplayStyle::Number, "", {}, "Name", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-1"},
    ParameterDescriptor{"common.name.8", "Rhythm Name 8", 7, ParameterEncoding::Ascii, 1, 32, 127, 0, 1, DisplayStyle::Number, "", {}, "Name", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-1"},
    ParameterDescriptor{"common.name.9", "Rhythm Name 9", 8, ParameterEncoding::Ascii, 1, 32, 127, 0, 1, DisplayStyle::Number, "", {}, "Name", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-1"},
    ParameterDescriptor{"common.name.10", "Rhythm Name 10", 9, ParameterEncoding::Ascii, 1, 32, 127, 0, 1, DisplayStyle::Number, "", {}, "Name", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-1"},
    ParameterDescriptor{"common.name.11", "Rhythm Name 11", 10, ParameterEncoding::Ascii, 1, 32, 127, 0, 1, DisplayStyle::Number, "", {}, "Name", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-1"},
    ParameterDescriptor{"common.name.12", "Rhythm Name 12", 11, ParameterEncoding::Ascii, 1, 32, 127, 0, 1, DisplayStyle::Number, "", {}, "Name", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-1"},
}};

const std::array<ParameterDescriptor, 57> kPartRows{{
    ParameterDescriptor{"note.tone_switch", "Tone Switch", 0, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels1, "Wave", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.wave_group_type", "Wave Group Type", 1, ParameterEncoding::SevenBit, 1, 0, 2, 0, 1, DisplayStyle::Number, "", kLabels2, "Wave", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.wave_group_id", "Wave Group ID", 2, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Wave", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.wave_number", "Wave Number", 3, ParameterEncoding::Nibble, 2, 0, 254, 1, 1, DisplayStyle::Number, "", {}, "Wave", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.wave_gain", "Wave Gain", 5, ParameterEncoding::SevenBit, 1, 0, 3, 0, 1, DisplayStyle::Number, "", kLabels3, "Wave", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.bend_range", "Bend Range", 6, ParameterEncoding::SevenBit, 1, 0, 12, 0, 1, DisplayStyle::Number, "", {}, "Pitch", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.mute_group", "Mute Group", 7, ParameterEncoding::SevenBit, 1, 0, 32, 0, 1, DisplayStyle::Number, "", kLabels4, "Wave", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.envelope_mode", "Envelope Mode", 8, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels5, "Wave", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2 note *1"},
    ParameterDescriptor{"note.volume_control_switch", "Volume Control Switch", 9, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels1, "Controllers", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.hold_1_control_switch", "Hold-1 Control Switch", 10, ParameterEncoding::SevenBit, 1, 0, 1, 0, 1, DisplayStyle::Number, "", kLabels1, "Controllers", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.pan_control_switch", "Pan Control Switch", 11, ParameterEncoding::SevenBit, 1, 0, 2, 0, 1, DisplayStyle::Number, "", kLabels6, "Controllers", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2 note *2"},
    ParameterDescriptor{"note.source_key", "Source Key", 12, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::NoteName, "", {}, "Wave", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.fine_tune", "Fine Tune", 13, ParameterEncoding::SevenBit, 1, 0, 100, -50, 1, DisplayStyle::Number, "", {}, "Pitch", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.random_pitch_depth", "Random Pitch Depth", 14, ParameterEncoding::SevenBit, 1, 0, 30, 0, 1, DisplayStyle::Number, "", kLabels7, "Pitch", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2 note *3"},
    ParameterDescriptor{"note.pitch_envelope_depth", "Pitch Envelope Depth", 15, ParameterEncoding::SevenBit, 1, 0, 24, -12, 1, DisplayStyle::Number, "", {}, "Pitch Envelope", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.pitch_envelope_velocity_sens", "Pitch Envelope Velocity Sens", 16, ParameterEncoding::SevenBit, 1, 0, 125, -100, 2, DisplayStyle::Number, "", {}, "Pitch Envelope", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.pitch_envelope_velocity_time", "Pitch Envelope Velocity Time", 17, ParameterEncoding::SevenBit, 1, 0, 14, 0, 1, DisplayStyle::Number, "", kLabels8, "Pitch Envelope", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2 note *4"},
    ParameterDescriptor{"note.pitch_envelope_time_1", "Pitch Envelope Time 1", 18, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Pitch Envelope", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.pitch_envelope_time_2", "Pitch Envelope Time 2", 19, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Pitch Envelope", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.pitch_envelope_time_3", "Pitch Envelope Time 3", 20, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Pitch Envelope", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.pitch_envelope_time_4", "Pitch Envelope Time 4", 21, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Pitch Envelope", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.pitch_envelope_level_1", "Pitch Envelope Level 1", 22, ParameterEncoding::SevenBit, 1, 0, 126, -63, 1, DisplayStyle::Number, "", {}, "Pitch Envelope", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2 note *5"},
    ParameterDescriptor{"note.pitch_envelope_level_2", "Pitch Envelope Level 2", 23, ParameterEncoding::SevenBit, 1, 0, 126, -63, 1, DisplayStyle::Number, "", {}, "Pitch Envelope", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2 note *5"},
    ParameterDescriptor{"note.pitch_envelope_level_3", "Pitch Envelope Level 3", 24, ParameterEncoding::SevenBit, 1, 0, 126, -63, 1, DisplayStyle::Number, "", {}, "Pitch Envelope", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2 note *5"},
    ParameterDescriptor{"note.pitch_envelope_level_4", "Pitch Envelope Level 4", 25, ParameterEncoding::SevenBit, 1, 0, 126, -63, 1, DisplayStyle::Number, "", {}, "Pitch Envelope", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2 note *5"},
    ParameterDescriptor{"note.filter_type", "Filter Type", 26, ParameterEncoding::SevenBit, 1, 0, 4, 0, 1, DisplayStyle::Number, "", kLabels9, "TVF", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2 note *6"},
    ParameterDescriptor{"note.cutoff_frequency", "Cutoff Frequency", 27, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "TVF", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.resonance", "Resonance", 28, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "TVF", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.resonance_velocity_sens", "Resonance Velocity Sens", 29, ParameterEncoding::SevenBit, 1, 0, 125, -100, 2, DisplayStyle::Number, "", {}, "TVF", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.filter_envelope_depth", "Filter Envelope Depth", 30, ParameterEncoding::SevenBit, 1, 0, 126, -63, 1, DisplayStyle::Number, "", {}, "TVF Envelope", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2 note *5"},
    ParameterDescriptor{"note.filter_envelope_velocity_sens", "Filter Envelope Velocity Sens", 31, ParameterEncoding::SevenBit, 1, 0, 125, -100, 2, DisplayStyle::Number, "", {}, "TVF Envelope", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.filter_envelope_velocity_time", "Filter Envelope Velocity Time", 32, ParameterEncoding::SevenBit, 1, 0, 14, 0, 1, DisplayStyle::Number, "", kLabels8, "TVF Envelope", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2 note *4"},
    ParameterDescriptor{"note.filter_envelope_time_1", "Filter Envelope Time 1", 33, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "TVF Envelope", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.filter_envelope_time_2", "Filter Envelope Time 2", 34, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "TVF Envelope", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.filter_envelope_time_3", "Filter Envelope Time 3", 35, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "TVF Envelope", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.filter_envelope_time_4", "Filter Envelope Time 4", 36, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "TVF Envelope", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.filter_envelope_level_1", "Filter Envelope Level 1", 37, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "TVF Envelope", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.filter_envelope_level_2", "Filter Envelope Level 2", 38, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "TVF Envelope", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.filter_envelope_level_3", "Filter Envelope Level 3", 39, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "TVF Envelope", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.filter_envelope_level_4", "Filter Envelope Level 4", 40, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "TVF Envelope", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.tone_level", "Tone Level", 41, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "TVA", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.level_envelope_velocity_sens", "Level Envelope Velocity Sens", 42, ParameterEncoding::SevenBit, 1, 0, 125, -100, 2, DisplayStyle::Number, "", {}, "TVA Envelope", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.level_envelope_velocity_time", "Level Envelope Velocity Time", 43, ParameterEncoding::SevenBit, 1, 0, 14, 0, 1, DisplayStyle::Number, "", kLabels8, "TVA Envelope", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2 note *4"},
    ParameterDescriptor{"note.level_envelope_time_1", "Level Envelope Time 1", 44, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "TVA Envelope", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.level_envelope_time_2", "Level Envelope Time 2", 45, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "TVA Envelope", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.level_envelope_time_3", "Level Envelope Time 3", 46, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "TVA Envelope", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.level_envelope_time_4", "Level Envelope Time 4", 47, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "TVA Envelope", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.level_envelope_level_1", "Level Envelope Level 1", 48, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "TVA Envelope", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.level_envelope_level_2", "Level Envelope Level 2", 49, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "TVA Envelope", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.level_envelope_level_3", "Level Envelope Level 3", 50, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "TVA Envelope", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.tone_pan", "Tone Pan", 51, ParameterEncoding::SevenBit, 1, 0, 127, -64, 1, DisplayStyle::Pan, "", {}, "Pan", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.random_pan_depth", "Random Pan Depth", 52, ParameterEncoding::SevenBit, 1, 0, 63, 0, 1, DisplayStyle::Number, "", {}, "Pan", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.alternate_pan_depth", "Alternate Pan Depth", 53, ParameterEncoding::SevenBit, 1, 1, 127, -64, 1, DisplayStyle::Pan, "", {}, "Pan", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.output_assign", "Output Assign", 54, ParameterEncoding::SevenBit, 1, 0, 3, 0, 1, DisplayStyle::Number, "", kLabels10, "Output", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2 note *7"},
    ParameterDescriptor{"note.mix_efx_send_level", "Mix/EFX Send Level", 55, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "EFX", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.chorus_send_level", "Chorus Send Level", 56, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Chorus", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
    ParameterDescriptor{"note.reverb_send_level", "Reverb Send Level", 57, ParameterEncoding::SevenBit, 1, 0, 127, 0, 1, DisplayStyle::Number, "", {}, "Reverb", VerificationStatus::DocumentationDerived, "Parameter Address Map §1-4-2"},
}};

} // namespace

const ParameterTable& rhythmCommonTable() noexcept
{
    static const ParameterTable kTable{"XP-60 Rhythm Common", kRhythmCommonSize,
                                       std::span<const ParameterDescriptor>(kCommonRows.data(), kCommonRows.size()),
                                       TableCompleteness::Complete,
                                       "Roland XP-60/XP-80 MIDI Implementation, Parameter Address Map 1-4-1"};
    return kTable;
}

const ParameterTable& rhythmNoteTable() noexcept
{
    static const ParameterTable kTable{"XP-60 Rhythm Note", kRhythmNoteSize,
                                       std::span<const ParameterDescriptor>(kPartRows.data(), kPartRows.size()),
                                       TableCompleteness::Complete,
                                       "Roland XP-60/XP-80 MIDI Implementation, Parameter Address Map 1-4-2"};
    return kTable;
}

const ParameterDescriptor& descriptor(RhythmCommonParameter parameter) noexcept
{
    return kCommonRows[static_cast<std::size_t>(parameter)];
}

const ParameterDescriptor& descriptor(RhythmNoteParameter parameter) noexcept
{
    return kPartRows[static_cast<std::size_t>(parameter)];
}

} // namespace xp60studio::xpmodel::xp60rhythm
