// GENERATED FILE — DO NOT EDIT.
//
// Produced by tools/generate_system_tables.py from
// docs/protocol/XP60_SYSTEM_PARAMETER_MAP.md (Roland XP-60/XP-80 MIDI
// Implementation, Parameter Address Map §1-1). Edit the document and re-run
// the generator; tst_system_tables fails when this file is stale.
//
// Source digest: sha256:9bced45431a171e8
#pragma once

#include "xpmodel/ParameterTable.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace xp60studio::xpmodel::xp60system {

inline constexpr std::string_view kSourceDocument = "docs/protocol/XP60_SYSTEM_PARAMETER_MAP.md";
inline constexpr std::string_view kSourceDigest = "sha256:9bced45431a171e8";

inline constexpr std::uint32_t kSystemCommonSize = 96;
inline constexpr std::uint32_t kScaleTuneSize = 12;
// Seventeen Scale Tune blocks: one per Performance Part at Roland
// 10 00 .. 1F 00, and one for Patch mode at 20 00.
inline constexpr int kScaleTuneBlockCount = 17;
inline constexpr std::array<std::uint32_t, 17> kScaleTuneOffsets{{2048, 2176, 2304, 2432, 2560, 2688, 2816, 2944, 3072, 3200, 3328, 3456, 3584, 3712, 3840, 3968, 4096}};
// The Patch-mode block is the last of them.
inline constexpr std::uint32_t kPatchModeScaleTuneOffset = 4096;
// Distance from the System base to the byte after the last Scale Tune.
inline constexpr std::uint32_t kSystemSpan = 4108;

// One enumerator per Performance Common parameter, in table order.
enum class SystemCommonParameter : std::uint16_t {
    SoundMode, //   0  Sound Mode
    PerformanceNumber, //   1  Performance Number
    PatchGroupType, //   2  Patch Group Type
    PatchGroupId, //   3  Patch Group ID
    PatchNumber, //   4  Patch Number
    MasterTune, //   5  Master Tune
    ScaleTuneSwitch, //   6  Scale Tune Switch
    EfxSwitch, //   7  EFX Switch
    ChorusSwitch, //   8  Chorus Switch
    ReverbSwitch, //   9  Reverb Switch
    PatchRemain, //  10  Patch Remain
    ClockSource, //  11  Clock Source
    TapControlSource, //  12  TAP Control Source
    HoldControlSource, //  13  Hold Control Source
    PeakControlSource, //  14  Peak Control Source
    VolumeControlSource, //  15  Volume Control Source
    AftertouchSource, //  16  Aftertouch Source
    SystemControlSource1, //  17  System Control Source 1
    SystemControlSource2, //  18  System Control Source 2
    ReceiveProgramChange, //  19  Receive Program Change
    ReceiveBankSelect, //  20  Receive Bank Select
    ReceiveControlChange, //  21  Receive Control Change
    ReceiveModulation, //  22  Receive Modulation
    ReceiveVolume, //  23  Receive Volume
    ReceiveHold1, //  24  Receive Hold-1
    ReceiveBender, //  25  Receive Bender
    ReceiveAftertouch, //  26  Receive Aftertouch
    ControlChannel, //  27  Control Channel
    PatchReceiveChannel, //  28  Patch Receive Channel
    RhythmEditSource, //  29  Rhythm Edit Source
    PreviewSoundMode, //  30  Preview Sound Mode
    PreviewNoteSet1, //  31  Preview Note Set 1
    PreviewVelocitySet1, //  32  Preview Velocity Set 1
    PreviewNoteSet2, //  33  Preview Note Set 2
    PreviewVelocitySet2, //  34  Preview Velocity Set 2
    PreviewNoteSet3, //  35  Preview Note Set 3
    PreviewVelocitySet3, //  36  Preview Velocity Set 3
    PreviewNoteSet4, //  37  Preview Note Set 4
    PreviewVelocitySet4, //  38  Preview Velocity Set 4
    TransmitProgramChange, //  39  Transmit Program Change
    TransmitBankSelect, //  40  Transmit Bank Select
    PatchTransmitChannel, //  41  Patch Transmit Channel
    TransposeSwitch, //  42  Transpose Switch
    TransposeValue, //  43  Transpose Value
    OctaveShift, //  44  Octave Shift
    KeyboardVelocity, //  45  Keyboard Velocity
    KeyboardSens, //  46  Keyboard Sens
    AftertouchSens, //  47  Aftertouch Sens
    Pedal1Assign, //  48  Pedal1 Assign
    Pedal1OutputMode, //  49  Pedal1 Output Mode
    Pedal1Polarity, //  50  Pedal1 Polarity
    Pedal2Assign, //  51  Pedal2 Assign
    Pedal2OutputMode, //  52  Pedal2 Output Mode
    Pedal2Polarity, //  53  Pedal2 Polarity
    C1Assign, //  54  C1 Assign
    C1OutputMode, //  55  C1 Output Mode
    C2Assign, //  56  C2 Assign
    C2OutputMode, //  57  C2 Output Mode
    HoldPedalOutputMode, //  58  Hold Pedal Output Mode
    HoldPedalPolarity, //  59  Hold Pedal Polarity
    BankSelectGroup1Switch, //  60  Bank Select Group1 Switch
    BankSelectGroup1Msb, //  61  Bank Select Group1 MSB
    BankSelectGroup1Lsb, //  62  Bank Select Group1 LSB
    BankSelectGroup2Switch, //  63  Bank Select Group2 Switch
    BankSelectGroup2Msb, //  64  Bank Select Group2 MSB
    BankSelectGroup2Lsb, //  65  Bank Select Group2 LSB
    BankSelectGroup3Switch, //  66  Bank Select Group3 Switch
    BankSelectGroup3Msb, //  67  Bank Select Group3 MSB
    BankSelectGroup3Lsb, //  68  Bank Select Group3 LSB
    BankSelectGroup4Switch, //  69  Bank Select Group4 Switch
    BankSelectGroup4Msb, //  70  Bank Select Group4 MSB
    BankSelectGroup4Lsb, //  71  Bank Select Group4 LSB
    BankSelectGroup5Switch, //  72  Bank Select Group5 Switch
    BankSelectGroup5Msb, //  73  Bank Select Group5 MSB
    BankSelectGroup5Lsb, //  74  Bank Select Group5 LSB
    BankSelectGroup6Switch, //  75  Bank Select Group6 Switch
    BankSelectGroup6Msb, //  76  Bank Select Group6 MSB
    BankSelectGroup6Lsb, //  77  Bank Select Group6 LSB
    BankSelectGroup7Switch, //  78  Bank Select Group7 Switch
    BankSelectGroup7Msb, //  79  Bank Select Group7 MSB
    BankSelectGroup7Lsb, //  80  Bank Select Group7 LSB
    Pedal3Assign, //  81  Pedal3 Assign
    Pedal3OutputMode, //  82  Pedal3 Output Mode
    Pedal3Polarity, //  83  Pedal3 Polarity
    Pedal4Assign, //  84  Pedal4 Assign
    Pedal4OutputMode, //  85  Pedal4 Output Mode
    Pedal4Polarity, //  86  Pedal4 Polarity
    ArpeggioStyle, //  87  Arpeggio Style
    ArpeggioMotif, //  88  Arpeggio Motif
    ArpeggioBeatPattern, //  89  Arpeggio Beat Pattern
    ArpeggioAccentRate, //  90  Arpeggio Accent Rate
    ArpeggioShuffleRate, //  91  Arpeggio Shuffle Rate
    ArpeggioKeyboardVelocity, //  92  Arpeggio Keyboard Velocity
    ArpeggioOctaveRange, //  93  Arpeggio Octave Range
    ArpeggioPartNumber, //  94  Arpeggio Part Number
    Count,
};

// One enumerator per Performance Part parameter, in table order.
enum class ScaleTuneParameter : std::uint16_t {
    ScaleTuneC, //   0  Scale Tune for C
    ScaleTuneCSharp, //   1  Scale Tune for C#
    ScaleTuneD, //   2  Scale Tune for D
    ScaleTuneDSharp, //   3  Scale Tune for D#
    ScaleTuneE, //   4  Scale Tune for E
    ScaleTuneF, //   5  Scale Tune for F
    ScaleTuneFSharp, //   6  Scale Tune for F#
    ScaleTuneG, //   7  Scale Tune for G
    ScaleTuneGSharp, //   8  Scale Tune for G#
    ScaleTuneA, //   9  Scale Tune for A
    ScaleTuneASharp, //  10  Scale Tune for A#
    ScaleTuneB, //  11  Scale Tune for B
    Count,
};

[[nodiscard]] const ParameterTable& systemCommonTable() noexcept;
[[nodiscard]] const ParameterTable& scaleTuneTable() noexcept;
[[nodiscard]] const ParameterDescriptor& descriptor(SystemCommonParameter parameter) noexcept;
[[nodiscard]] const ParameterDescriptor& descriptor(ScaleTuneParameter parameter) noexcept;

} // namespace xp60studio::xpmodel::xp60system
