// GENERATED FILE — DO NOT EDIT.
//
// Produced by tools/generate_patch_tables.py from
// docs/protocol/XP60_PATCH_PARAMETER_MAP.md (Roland XP-60/XP-80 MIDI
// Implementation, Parameter Address Map, pp.223-225). Edit the document and
// re-run the generator; tst_generated_tables fails when this file is stale.
//
// Source digest: sha256:a30c743a97ba9d89
#pragma once

#include "xpmodel/ParameterTable.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace xp60studio::xpmodel::xp60tables {

inline constexpr std::string_view kSourceDocument = "docs/protocol/XP60_PATCH_PARAMETER_MAP.md";
inline constexpr std::string_view kSourceDigest = "sha256:a30c743a97ba9d89";

inline constexpr std::uint32_t kPatchCommonSize = 73;
inline constexpr std::uint32_t kPatchToneSize = 129;
// Byte offsets of Tone 1..4 within a Patch (Roland 10 00, 12 00, 14 00, 16 00).
inline constexpr std::array<std::uint32_t, 4> kToneOffsets{{2048, 2304, 2560, 2816}};
// Distance from the Patch base to the byte after Tone 4.
inline constexpr std::uint32_t kPatchSpan = 2945;

// One enumerator per Patch Common parameter, in table order.
enum class CommonParameter : std::uint16_t {
    PatchName1, //   0  Patch Name 1
    PatchName2, //   1  Patch Name 2
    PatchName3, //   2  Patch Name 3
    PatchName4, //   3  Patch Name 4
    PatchName5, //   4  Patch Name 5
    PatchName6, //   5  Patch Name 6
    PatchName7, //   6  Patch Name 7
    PatchName8, //   7  Patch Name 8
    PatchName9, //   8  Patch Name 9
    PatchName10, //   9  Patch Name 10
    PatchName11, //  10  Patch Name 11
    PatchName12, //  11  Patch Name 12
    EfxType, //  12  EFX Type
    EfxParameter1, //  13  EFX Parameter 1
    EfxParameter2, //  14  EFX Parameter 2
    EfxParameter3, //  15  EFX Parameter 3
    EfxParameter4, //  16  EFX Parameter 4
    EfxParameter5, //  17  EFX Parameter 5
    EfxParameter6, //  18  EFX Parameter 6
    EfxParameter7, //  19  EFX Parameter 7
    EfxParameter8, //  20  EFX Parameter 8
    EfxParameter9, //  21  EFX Parameter 9
    EfxParameter10, //  22  EFX Parameter 10
    EfxParameter11, //  23  EFX Parameter 11
    EfxParameter12, //  24  EFX Parameter 12
    EfxOutputAssign, //  25  EFX Output Assign
    EfxMixOutSendLevel, //  26  EFX Mix Out Send Level
    EfxChorusSendLevel, //  27  EFX Chorus Send Level
    EfxReverbSendLevel, //  28  EFX Reverb Send Level
    EfxControlSource1, //  29  EFX Control Source 1
    EfxControlDepth1, //  30  EFX Control Depth 1
    EfxControlSource2, //  31  EFX Control Source 2
    EfxControlDepth2, //  32  EFX Control Depth 2
    ChorusLevel, //  33  Chorus Level
    ChorusRate, //  34  Chorus Rate
    ChorusDepth, //  35  Chorus Depth
    ChorusPreDelay, //  36  Chorus Pre-Delay
    ChorusFeedback, //  37  Chorus Feedback
    ChorusOutput, //  38  Chorus Output
    ReverbType, //  39  Reverb Type
    ReverbLevel, //  40  Reverb Level
    ReverbTime, //  41  Reverb Time
    ReverbHfDamp, //  42  Reverb HF Damp
    DelayFeedback, //  43  Delay Feedback
    PatchTempo, //  44  Patch Tempo
    PatchLevel, //  46  Patch Level
    PatchPan, //  47  Patch Pan
    AnalogFeel, //  48  Analog Feel
    BendRangeUp, //  49  Bend Range Up
    BendRangeDown, //  50  Bend Range Down
    KeyAssignMode, //  51  Key Assign Mode
    SoloLegato, //  52  Solo Legato
    PortamentoSwitch, //  53  Portamento Switch
    PortamentoMode, //  54  Portamento Mode
    PortamentoType, //  55  Portamento Type
    PortamentoStart, //  56  Portamento Start
    PortamentoTime, //  57  Portamento Time
    PatchControlSource2, //  58  Patch Control Source 2
    PatchControlSource3, //  59  Patch Control Source 3
    EfxControlHoldPeak, //  60  EFX Control Hold/Peak
    Control1HoldPeak, //  61  Control 1 Hold/Peak
    Control2HoldPeak, //  62  Control 2 Hold/Peak
    Control3HoldPeak, //  63  Control 3 Hold/Peak
    VelocityRangeSwitch, //  64  Velocity Range Switch
    OctaveShift, //  65  Octave Shift
    StretchTuneDepth, //  66  Stretch Tune Depth
    VoicePriority, //  67  Voice Priority
    StructureType12, //  68  Structure Type 1&2
    Booster12, //  69  Booster 1&2
    StructureType34, //  70  Structure Type 3&4
    Booster34, //  71  Booster 3&4
    ClockSource, //  72  Clock Source
    Count,
};

// One enumerator per Patch Tone parameter, in table order.
enum class ToneParameter : std::uint16_t {
    ToneSwitch, //   0  Tone Switch
    WaveGroupType, //   1  Wave Group Type
    WaveGroupId, //   2  Wave Group ID
    WaveNumber, //   3  Wave Number
    WaveGain, //   5  Wave Gain
    FxmSwitch, //   6  FXM Switch
    FxmColor, //   7  FXM Color
    FxmDepth, //   8  FXM Depth
    ToneDelayMode, //   9  Tone Delay Mode
    ToneDelayTime, //  10  Tone Delay Time
    VelocityCrossFade, //  11  Velocity Cross Fade
    VelocityRangeLower, //  12  Velocity Range Lower
    VelocityRangeUpper, //  13  Velocity Range Upper
    KeyboardRangeLower, //  14  Keyboard Range Lower
    KeyboardRangeUpper, //  15  Keyboard Range Upper
    RedamperControlSwitch, //  16  Redamper Control Switch
    VolumeControlSwitch, //  17  Volume Control Switch
    Hold1ControlSwitch, //  18  Hold-1 Control Switch
    BenderControlSwitch, //  19  Bender Control Switch
    PanControlSwitch, //  20  Pan Control Switch
    Controller1Destination1, //  21  Controller 1 Destination 1
    Controller1Depth1, //  22  Controller 1 Depth 1
    Controller1Destination2, //  23  Controller 1 Destination 2
    Controller1Depth2, //  24  Controller 1 Depth 2
    Controller1Destination3, //  25  Controller 1 Destination 3
    Controller1Depth3, //  26  Controller 1 Depth 3
    Controller1Destination4, //  27  Controller 1 Destination 4
    Controller1Depth4, //  28  Controller 1 Depth 4
    Controller2Destination1, //  29  Controller 2 Destination 1
    Controller2Depth1, //  30  Controller 2 Depth 1
    Controller2Destination2, //  31  Controller 2 Destination 2
    Controller2Depth2, //  32  Controller 2 Depth 2
    Controller2Destination3, //  33  Controller 2 Destination 3
    Controller2Depth3, //  34  Controller 2 Depth 3
    Controller2Destination4, //  35  Controller 2 Destination 4
    Controller2Depth4, //  36  Controller 2 Depth 4
    Controller3Destination1, //  37  Controller 3 Destination 1
    Controller3Depth1, //  38  Controller 3 Depth 1
    Controller3Destination2, //  39  Controller 3 Destination 2
    Controller3Depth2, //  40  Controller 3 Depth 2
    Controller3Destination3, //  41  Controller 3 Destination 3
    Controller3Depth3, //  42  Controller 3 Depth 3
    Controller3Destination4, //  43  Controller 3 Destination 4
    Controller3Depth4, //  44  Controller 3 Depth 4
    Lfo1Waveform, //  45  LFO1 Waveform
    Lfo1KeyTrigger, //  46  LFO1 Key Trigger
    Lfo1Rate, //  47  LFO1 Rate
    Lfo1Offset, //  48  LFO1 Offset
    Lfo1DelayTime, //  49  LFO1 Delay Time
    Lfo1FadeMode, //  50  LFO1 Fade Mode
    Lfo1FadeTime, //  51  LFO1 Fade Time
    Lfo1ExternalSync, //  52  LFO1 External Sync
    Lfo2Waveform, //  53  LFO2 Waveform
    Lfo2KeyTrigger, //  54  LFO2 Key Trigger
    Lfo2Rate, //  55  LFO2 Rate
    Lfo2Offset, //  56  LFO2 Offset
    Lfo2DelayTime, //  57  LFO2 Delay Time
    Lfo2FadeMode, //  58  LFO2 Fade Mode
    Lfo2FadeTime, //  59  LFO2 Fade Time
    Lfo2ExternalSync, //  60  LFO2 External Sync
    CoarseTune, //  61  Coarse Tune
    FineTune, //  62  Fine Tune
    RandomPitchDepth, //  63  Random Pitch Depth
    PitchKeyfollow, //  64  Pitch Keyfollow
    PitchEnvelopeDepth, //  65  Pitch Envelope Depth
    PitchEnvelopeVelocitySens, //  66  Pitch Envelope Velocity Sens
    PitchEnvelopeVelocityTime1, //  67  Pitch Envelope Velocity Time1
    PitchEnvelopeVelocityTime4, //  68  Pitch Envelope Velocity Time4
    PitchEnvelopeTimeKeyfollow, //  69  Pitch Envelope Time Keyfollow
    PitchEnvelopeTime1, //  70  Pitch Envelope Time 1
    PitchEnvelopeTime2, //  71  Pitch Envelope Time 2
    PitchEnvelopeTime3, //  72  Pitch Envelope Time 3
    PitchEnvelopeTime4, //  73  Pitch Envelope Time 4
    PitchEnvelopeLevel1, //  74  Pitch Envelope Level 1
    PitchEnvelopeLevel2, //  75  Pitch Envelope Level 2
    PitchEnvelopeLevel3, //  76  Pitch Envelope Level 3
    PitchEnvelopeLevel4, //  77  Pitch Envelope Level 4
    PitchLfo1Depth, //  78  Pitch LFO1 Depth
    PitchLfo2Depth, //  79  Pitch LFO2 Depth
    FilterType, //  80  Filter Type
    CutoffFrequency, //  81  Cutoff Frequency
    CutoffKeyfollow, //  82  Cutoff Keyfollow
    Resonance, //  83  Resonance
    ResonanceVelocitySens, //  84  Resonance Velocity Sens
    FilterEnvelopeDepth, //  85  Filter Envelope Depth
    FilterEnvelopeVelocityCurve, //  86  Filter Envelope Velocity Curve
    FilterEnvelopeVelocitySens, //  87  Filter Envelope Velocity Sens
    FilterEnvelopeVelocityTime1, //  88  Filter Envelope Velocity Time1
    FilterEnvelopeVelocityTime4, //  89  Filter Envelope Velocity Time4
    FilterEnvelopeTimeKeyfollow, //  90  Filter Envelope Time Keyfollow
    FilterEnvelopeTime1, //  91  Filter Envelope Time 1
    FilterEnvelopeTime2, //  92  Filter Envelope Time 2
    FilterEnvelopeTime3, //  93  Filter Envelope Time 3
    FilterEnvelopeTime4, //  94  Filter Envelope Time 4
    FilterEnvelopeLevel1, //  95  Filter Envelope Level 1
    FilterEnvelopeLevel2, //  96  Filter Envelope Level 2
    FilterEnvelopeLevel3, //  97  Filter Envelope Level 3
    FilterEnvelopeLevel4, //  98  Filter Envelope Level 4
    FilterLfo1Depth, //  99  Filter LFO1 Depth
    FilterLfo2Depth, // 100  Filter LFO2 Depth
    ToneLevel, // 101  Tone Level
    BiasDirection, // 102  Bias Direction
    BiasPosition, // 103  Bias Position
    BiasLevel, // 104  Bias Level
    LevelEnvelopeVelocityCurve, // 105  Level Envelope Velocity Curve
    LevelEnvelopeVelocitySens, // 106  Level Envelope Velocity Sens
    LevelEnvelopeVelocityTime1, // 107  Level Envelope Velocity Time1
    LevelEnvelopeVelocityTime4, // 108  Level Envelope Velocity Time4
    LevelEnvelopeTimeKeyfollow, // 109  Level Envelope Time Keyfollow
    LevelEnvelopeTime1, // 110  Level Envelope Time 1
    LevelEnvelopeTime2, // 111  Level Envelope Time 2
    LevelEnvelopeTime3, // 112  Level Envelope Time 3
    LevelEnvelopeTime4, // 113  Level Envelope Time 4
    LevelEnvelopeLevel1, // 114  Level Envelope Level 1
    LevelEnvelopeLevel2, // 115  Level Envelope Level 2
    LevelEnvelopeLevel3, // 116  Level Envelope Level 3
    LevelLfo1Depth, // 117  Level LFO1 Depth
    LevelLfo2Depth, // 118  Level LFO2 Depth
    TonePan, // 119  Tone Pan
    PanKeyfollow, // 120  Pan Keyfollow
    RandomPanDepth, // 121  Random Pan Depth
    AlternatePanDepth, // 122  Alternate Pan Depth
    PanLfo1Depth, // 123  Pan LFO1 Depth
    PanLfo2Depth, // 124  Pan LFO2 Depth
    OutputAssign, // 125  Output Assign
    MixEfxSendLevel, // 126  Mix/EFX Send Level
    ChorusSendLevel, // 127  Chorus Send Level
    ReverbSendLevel, // 128  Reverb Send Level
    Count,
};

[[nodiscard]] const ParameterTable& patchCommonTable() noexcept;
[[nodiscard]] const ParameterTable& patchToneTable() noexcept;
[[nodiscard]] const ParameterDescriptor& descriptor(CommonParameter parameter) noexcept;
[[nodiscard]] const ParameterDescriptor& descriptor(ToneParameter parameter) noexcept;

} // namespace xp60studio::xpmodel::xp60tables
