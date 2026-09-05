#pragma once

#include <optional>
#include <string_view>

namespace xp60studio::xpmodel {

// How a Tone's wave reference bytes name an internal XP-60 waveform.
//
// A Tone carries three bytes: Wave Group Type, Wave Group ID and Wave Number.
// This is the only place that turns them into a bank and a displayed number,
// and back. Everything here is hardware-verified on 2026-09-04 rather than
// derived from the manual, which does not state the assignment; see
// docs/PHASE_4_WAVE_BROWSER.md "Bank boundary evidence from the front panel"
// and docs/DEVICE_ACCEPTANCE.md area 9:
//
//   * group type 0 is INT; group ID 1 is INT-A and 2 is INT-B, confirmed by
//     selecting INT-B on the panel and watching the ID move to 2 in two
//     independent Tones;
//   * the wave number is zero-based, display = raw + 1, agreed by four panel
//     selections across both banks;
//   * INT-B 193 is selectable and reads raw 192, and its name reads "DC" on
//     both the instrument and in the catalog.
//
// Expansion waves are deliberately not representable. Group type 2 (EXP) was
// found on 61 of 512 references in User memory across group IDs 1, 5, 7 and 18;
// which board each ID denotes is unknown, so those references are preserved
// byte-for-byte elsewhere and never resolved here.
enum class InternalWaveBank {
    IntA,
    IntB,
};

// Raw byte values, as the instrument reports them.
inline constexpr int kInternalWaveGroupTypeRaw = 0; // "INT"
inline constexpr int kIntAGroupIdRaw = 1;
inline constexpr int kIntBGroupIdRaw = 2;

// Bank sizes. INT-B's is confirmed by selecting its last wave on the panel;
// INT-A's by 302 stored references reaching display 255, which no smaller bank
// could hold.
inline constexpr int kIntAWaveCount = 255;
inline constexpr int kIntBWaveCount = 193;

// A wave as a person picks it: a bank and the 1-based number on the display.
struct WaveSelection
{
    InternalWaveBank bank = InternalWaveBank::IntA;
    int displayNumber = 1; // 1 .. waveBankSize(bank)

    friend bool operator==(const WaveSelection&, const WaveSelection&) noexcept = default;
};

// The three raw Tone bytes that name it.
struct WaveIdentifier
{
    int groupTypeRaw = kInternalWaveGroupTypeRaw;
    int groupIdRaw = kIntAGroupIdRaw;
    int numberRaw = 0;

    friend bool operator==(const WaveIdentifier&, const WaveIdentifier&) noexcept = default;
};

[[nodiscard]] constexpr int waveBankSize(InternalWaveBank bank) noexcept
{
    return bank == InternalWaveBank::IntA ? kIntAWaveCount : kIntBWaveCount;
}

[[nodiscard]] std::string_view waveBankLabel(InternalWaveBank bank) noexcept;

// The bank a raw (group type, group ID) pair names, or nullopt when the pair is
// not an internal wave. Refuses group type 2 (EXP) and any group ID but 1 or 2,
// including the raw 0 observed as a transient while navigating the panel.
[[nodiscard]] std::optional<InternalWaveBank> internalWaveBank(int groupTypeRaw, int groupIdRaw) noexcept;

// "INT-A" / "INT-B" exactly; nothing else, and no case folding.
[[nodiscard]] std::optional<InternalWaveBank> internalWaveBankFromLabel(std::string_view label) noexcept;

// Selection -> bytes, and bytes -> selection. Both refuse anything outside the
// bank rather than clamping: a number the instrument cannot select is a caller
// error, and quietly moving it to the nearest legal one would write a wave
// nobody asked for.
[[nodiscard]] std::optional<WaveIdentifier> encodeWave(const WaveSelection& selection) noexcept;
[[nodiscard]] std::optional<WaveSelection> decodeWave(int groupTypeRaw, int groupIdRaw, int numberRaw) noexcept;

} // namespace xp60studio::xpmodel
