// GENERATED FILE — DO NOT EDIT.
//
// Produced by tools/generate_rhythm_tables.py from
// docs/protocol/XP60_RHYTHM_PARAMETER_MAP.md (Roland XP-60/XP-80 MIDI
// Implementation, Parameter Address Map §1-4). Edit the document and re-run
// the generator; tst_rhythm_tables fails when this file is stale.
//
// Source digest: sha256:4938876e8039d2b4
#pragma once

#include "xpmodel/ParameterTable.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace xp60studio::xpmodel::xp60rhythm {

inline constexpr std::string_view kSourceDocument = "docs/protocol/XP60_RHYTHM_PARAMETER_MAP.md";
inline constexpr std::string_view kSourceDigest = "sha256:4938876e8039d2b4";

inline constexpr std::uint32_t kRhythmCommonSize = 12;
inline constexpr std::uint32_t kRhythmNoteSize = 58;
inline constexpr int kFirstRhythmKey = 35;
inline constexpr int kLastRhythmKey = 98;
inline constexpr int kRhythmNoteCount = 64;
// Byte offsets of the Notes, indexed by key - kFirstRhythmKey. A Note's
// offset is its MIDI note number: Key# 35 lives at Roland 23 00.
inline constexpr std::array<std::uint32_t, 64> kRhythmNoteOffsets{{4480, 4608, 4736, 4864, 4992, 5120, 5248, 5376, 5504, 5632, 5760, 5888, 6016, 6144, 6272, 6400, 6528, 6656, 6784, 6912, 7040, 7168, 7296, 7424, 7552, 7680, 7808, 7936, 8064, 8192, 8320, 8448, 8576, 8704, 8832, 8960, 9088, 9216, 9344, 9472, 9600, 9728, 9856, 9984, 10112, 10240, 10368, 10496, 10624, 10752, 10880, 11008, 11136, 11264, 11392, 11520, 11648, 11776, 11904, 12032, 12160, 12288, 12416, 12544}};
// Distance from the Rhythm Setup base to the byte after the last Note.
inline constexpr std::uint32_t kRhythmSetupSpan = 12602;

// One enumerator per Performance Common parameter, in table order.
enum class RhythmCommonParameter : std::uint16_t {
    RhythmName1, //   0  Rhythm Name 1
    RhythmName2, //   1  Rhythm Name 2
    RhythmName3, //   2  Rhythm Name 3
    RhythmName4, //   3  Rhythm Name 4
    RhythmName5, //   4  Rhythm Name 5
    RhythmName6, //   5  Rhythm Name 6
    RhythmName7, //   6  Rhythm Name 7
    RhythmName8, //   7  Rhythm Name 8
    RhythmName9, //   8  Rhythm Name 9
    RhythmName10, //   9  Rhythm Name 10
    RhythmName11, //  10  Rhythm Name 11
    RhythmName12, //  11  Rhythm Name 12
    Count,
};

// One enumerator per Performance Part parameter, in table order.
enum class RhythmNoteParameter : std::uint16_t {
    ToneSwitch, //   0  Tone Switch
    WaveGroupType, //   1  Wave Group Type
    WaveGroupId, //   2  Wave Group ID
    WaveNumber, //   3  Wave Number
    WaveGain, //   4  Wave Gain
    BendRange, //   5  Bend Range
    MuteGroup, //   6  Mute Group
    EnvelopeMode, //   7  Envelope Mode
    VolumeControlSwitch, //   8  Volume Control Switch
    Hold1ControlSwitch, //   9  Hold-1 Control Switch
    PanControlSwitch, //  10  Pan Control Switch
    SourceKey, //  11  Source Key
    FineTune, //  12  Fine Tune
    RandomPitchDepth, //  13  Random Pitch Depth
    PitchEnvelopeDepth, //  14  Pitch Envelope Depth
    PitchEnvelopeVelocitySens, //  15  Pitch Envelope Velocity Sens
    PitchEnvelopeVelocityTime, //  16  Pitch Envelope Velocity Time
    PitchEnvelopeTime1, //  17  Pitch Envelope Time 1
    PitchEnvelopeTime2, //  18  Pitch Envelope Time 2
    PitchEnvelopeTime3, //  19  Pitch Envelope Time 3
    PitchEnvelopeTime4, //  20  Pitch Envelope Time 4
    PitchEnvelopeLevel1, //  21  Pitch Envelope Level 1
    PitchEnvelopeLevel2, //  22  Pitch Envelope Level 2
    PitchEnvelopeLevel3, //  23  Pitch Envelope Level 3
    PitchEnvelopeLevel4, //  24  Pitch Envelope Level 4
    FilterType, //  25  Filter Type
    CutoffFrequency, //  26  Cutoff Frequency
    Resonance, //  27  Resonance
    ResonanceVelocitySens, //  28  Resonance Velocity Sens
    FilterEnvelopeDepth, //  29  Filter Envelope Depth
    FilterEnvelopeVelocitySens, //  30  Filter Envelope Velocity Sens
    FilterEnvelopeVelocityTime, //  31  Filter Envelope Velocity Time
    FilterEnvelopeTime1, //  32  Filter Envelope Time 1
    FilterEnvelopeTime2, //  33  Filter Envelope Time 2
    FilterEnvelopeTime3, //  34  Filter Envelope Time 3
    FilterEnvelopeTime4, //  35  Filter Envelope Time 4
    FilterEnvelopeLevel1, //  36  Filter Envelope Level 1
    FilterEnvelopeLevel2, //  37  Filter Envelope Level 2
    FilterEnvelopeLevel3, //  38  Filter Envelope Level 3
    FilterEnvelopeLevel4, //  39  Filter Envelope Level 4
    ToneLevel, //  40  Tone Level
    LevelEnvelopeVelocitySens, //  41  Level Envelope Velocity Sens
    LevelEnvelopeVelocityTime, //  42  Level Envelope Velocity Time
    LevelEnvelopeTime1, //  43  Level Envelope Time 1
    LevelEnvelopeTime2, //  44  Level Envelope Time 2
    LevelEnvelopeTime3, //  45  Level Envelope Time 3
    LevelEnvelopeTime4, //  46  Level Envelope Time 4
    LevelEnvelopeLevel1, //  47  Level Envelope Level 1
    LevelEnvelopeLevel2, //  48  Level Envelope Level 2
    LevelEnvelopeLevel3, //  49  Level Envelope Level 3
    TonePan, //  50  Tone Pan
    RandomPanDepth, //  51  Random Pan Depth
    AlternatePanDepth, //  52  Alternate Pan Depth
    OutputAssign, //  53  Output Assign
    MixEfxSendLevel, //  54  Mix/EFX Send Level
    ChorusSendLevel, //  55  Chorus Send Level
    ReverbSendLevel, //  56  Reverb Send Level
    Count,
};

[[nodiscard]] const ParameterTable& rhythmCommonTable() noexcept;
[[nodiscard]] const ParameterTable& rhythmNoteTable() noexcept;
[[nodiscard]] const ParameterDescriptor& descriptor(RhythmCommonParameter parameter) noexcept;
[[nodiscard]] const ParameterDescriptor& descriptor(RhythmNoteParameter parameter) noexcept;

} // namespace xp60studio::xpmodel::xp60rhythm
