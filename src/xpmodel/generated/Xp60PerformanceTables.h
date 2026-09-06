// GENERATED FILE — DO NOT EDIT.
//
// Produced by tools/generate_performance_tables.py from
// docs/protocol/XP60_PERFORMANCE_PARAMETER_MAP.md (Roland XP-60/XP-80 MIDI
// Implementation, Parameter Address Map §1-2). Edit the document and re-run
// the generator; tst_performance_tables fails when this file is stale.
//
// Source digest: sha256:1b4012b07f799209
#pragma once

#include "xpmodel/ParameterTable.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace xp60studio::xpmodel::xp60performance {

inline constexpr std::string_view kSourceDocument = "docs/protocol/XP60_PERFORMANCE_PARAMETER_MAP.md";
inline constexpr std::string_view kSourceDigest = "sha256:1b4012b07f799209";

inline constexpr std::uint32_t kPerformanceCommonSize = 66;
inline constexpr std::uint32_t kPerformancePartSize = 25;
inline constexpr int kPartCount = 16;
// Byte offsets of Part 1..16 within a Performance (Roland 10 00 .. 1F 00).
inline constexpr std::array<std::uint32_t, 16> kPartOffsets{{2048, 2176, 2304, 2432, 2560, 2688, 2816, 2944, 3072, 3200, 3328, 3456, 3584, 3712, 3840, 3968}};
// Distance from the Performance base to the byte after Part 16.
inline constexpr std::uint32_t kPerformanceSpan = 3993;

// One enumerator per Performance Common parameter, in table order.
enum class PerformanceCommonParameter : std::uint16_t {
    PerformanceName1, //   0  Performance Name 1
    PerformanceName2, //   1  Performance Name 2
    PerformanceName3, //   2  Performance Name 3
    PerformanceName4, //   3  Performance Name 4
    PerformanceName5, //   4  Performance Name 5
    PerformanceName6, //   5  Performance Name 6
    PerformanceName7, //   6  Performance Name 7
    PerformanceName8, //   7  Performance Name 8
    PerformanceName9, //   8  Performance Name 9
    PerformanceName10, //   9  Performance Name 10
    PerformanceName11, //  10  Performance Name 11
    PerformanceName12, //  11  Performance Name 12
    EfxSource, //  12  EFX Source
    EfxType, //  13  EFX Type
    EfxParameter1, //  14  EFX Parameter 1
    EfxParameter2, //  15  EFX Parameter 2
    EfxParameter3, //  16  EFX Parameter 3
    EfxParameter4, //  17  EFX Parameter 4
    EfxParameter5, //  18  EFX Parameter 5
    EfxParameter6, //  19  EFX Parameter 6
    EfxParameter7, //  20  EFX Parameter 7
    EfxParameter8, //  21  EFX Parameter 8
    EfxParameter9, //  22  EFX Parameter 9
    EfxParameter10, //  23  EFX Parameter 10
    EfxParameter11, //  24  EFX Parameter 11
    EfxParameter12, //  25  EFX Parameter 12
    EfxOutputAssign, //  26  EFX Output Assign
    EfxMixOutSendLevel, //  27  EFX Mix Out Send Level
    EfxChorusSendLevel, //  28  EFX Chorus Send Level
    EfxReverbSendLevel, //  29  EFX Reverb Send Level
    EfxControlSource1, //  30  EFX Control Source 1
    EfxControlDepth1, //  31  EFX Control Depth 1
    EfxControlSource2, //  32  EFX Control Source 2
    EfxControlDepth2, //  33  EFX Control Depth 2
    ChorusLevel, //  34  Chorus Level
    ChorusRate, //  35  Chorus Rate
    ChorusDepth, //  36  Chorus Depth
    ChorusPreDelay, //  37  Chorus Pre-Delay
    ChorusFeedback, //  38  Chorus Feedback
    ChorusOutput, //  39  Chorus Output
    ReverbType, //  40  Reverb Type
    ReverbLevel, //  41  Reverb Level
    ReverbTime, //  42  Reverb Time
    ReverbHfDamp, //  43  Reverb HF Damp
    DelayFeedback, //  44  Delay Feedback
    PerformanceTempo, //  45  Performance Tempo
    KeyboardRangeSwitch, //  46  Keyboard Range Switch
    VoiceReserve1, //  47  Voice Reserve 1
    VoiceReserve2, //  48  Voice Reserve 2
    VoiceReserve3, //  49  Voice Reserve 3
    VoiceReserve4, //  50  Voice Reserve 4
    VoiceReserve5, //  51  Voice Reserve 5
    VoiceReserve6, //  52  Voice Reserve 6
    VoiceReserve7, //  53  Voice Reserve 7
    VoiceReserve8, //  54  Voice Reserve 8
    VoiceReserve9, //  55  Voice Reserve 9
    VoiceReserve10, //  56  Voice Reserve 10
    VoiceReserve11, //  57  Voice Reserve 11
    VoiceReserve12, //  58  Voice Reserve 12
    VoiceReserve13, //  59  Voice Reserve 13
    VoiceReserve14, //  60  Voice Reserve 14
    VoiceReserve15, //  61  Voice Reserve 15
    VoiceReserve16, //  62  Voice Reserve 16
    KeyboardMode, //  63  Keyboard Mode
    ClockSource, //  64  Clock Source
    Count,
};

// One enumerator per Performance Part parameter, in table order.
enum class PerformancePartParameter : std::uint16_t {
    ReceiveSwitch, //   0  Receive Switch
    MidiChannel, //   1  MIDI Channel
    PatchGroupType, //   2  Patch Group Type
    PatchGroupId, //   3  Patch Group ID
    PatchNumber, //   4  Patch Number
    PartLevel, //   5  Part Level
    PartPan, //   6  Part Pan
    PartCoarseTune, //   7  Part Coarse Tune
    PartFineTune, //   8  Part Fine Tune
    OutputAssign, //   9  Output Assign
    MixEfxSendLevel, //  10  Mix/EFX Send Level
    ChorusSendLevel, //  11  Chorus Send Level
    ReverbSendLevel, //  12  Reverb Send Level
    ReceiveProgramChangeSwitch, //  13  Receive Program Change Switch
    ReceiveVolumeSwitch, //  14  Receive Volume Switch
    ReceiveHold1Switch, //  15  Receive Hold-1 Switch
    KeyboardRangeLower, //  16  Keyboard Range Lower
    KeyboardRangeUpper, //  17  Keyboard Range Upper
    OctaveShift, //  18  Octave Shift
    LocalSwitch, //  19  Local Switch
    TransmitSwitch, //  20  Transmit Switch
    TransmitBankSelectGroup, //  21  Transmit Bank Select Group
    TransmitVolume, //  22  Transmit Volume
    Count,
};

[[nodiscard]] const ParameterTable& performanceCommonTable() noexcept;
[[nodiscard]] const ParameterTable& performancePartTable() noexcept;
[[nodiscard]] const ParameterDescriptor& descriptor(PerformanceCommonParameter parameter) noexcept;
[[nodiscard]] const ParameterDescriptor& descriptor(PerformancePartParameter parameter) noexcept;

} // namespace xp60studio::xpmodel::xp60performance
