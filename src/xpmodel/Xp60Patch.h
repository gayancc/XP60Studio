#pragma once

#include "xpmodel/BlockCodec.h"
#include "xpmodel/PatchName.h"
#include "xpmodel/Xp60PatchLayout.h"
#include "xpmodel/generated/Xp60PatchTables.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace xp60studio::xpmodel {

using xp60tables::CommonParameter;
using xp60tables::ToneParameter;

// A complete XP-60 Patch: Patch Common plus Tone 1-4.
//
// The Patch owns the decoded BlockValues of its five blocks (raw values and
// the original bytes). Typed access goes through the generated parameter
// enumerations so an identifier typo is a compile error, and grouped views
// (envelopes, wave reference) give the editor musically meaningful handles
// without duplicating any offset or range.
class Xp60Patch
{
public:
    Xp60Patch(BlockValues common, std::array<BlockValues, ToneIndex::kCount> tones);

    // Blocks ------------------------------------------------------------------
    [[nodiscard]] const BlockValues& common() const noexcept { return m_common; }
    [[nodiscard]] BlockValues& common() noexcept { return m_common; }
    [[nodiscard]] const BlockValues& tone(ToneIndex tone) const noexcept { return m_tones[tone.index()]; }
    [[nodiscard]] BlockValues& tone(ToneIndex tone) noexcept { return m_tones[tone.index()]; }

    // Typed parameter access ---------------------------------------------------
    [[nodiscard]] int raw(CommonParameter parameter) const noexcept;
    [[nodiscard]] int raw(ToneIndex tone, ToneParameter parameter) const noexcept;
    [[nodiscard]] int display(CommonParameter parameter) const noexcept;
    [[nodiscard]] int display(ToneIndex tone, ToneParameter parameter) const noexcept;
    [[nodiscard]] std::string displayText(CommonParameter parameter) const;
    [[nodiscard]] std::string displayText(ToneIndex tone, ToneParameter parameter) const;
    // False when the raw value is outside the documented range; nothing is clamped.
    bool setRaw(CommonParameter parameter, int raw) noexcept;
    bool setRaw(ToneIndex tone, ToneParameter parameter, int raw) noexcept;

    // Musical views --------------------------------------------------------------
    [[nodiscard]] PatchName name() const;
    bool setName(const PatchName& name) noexcept;
    [[nodiscard]] bool toneEnabled(ToneIndex tone) const noexcept;
    [[nodiscard]] int enabledToneCount() const noexcept;

    struct WaveReference
    {
        int groupTypeRaw = 0;            // 0 INT, 1 <PCM>, 2 EXP
        std::string_view groupTypeLabel;
        int groupId = 0;
        int numberRaw = 0;               // 0..254
        int numberDisplay = 1;           // 1..255
        int gainRaw = 0;
        std::string_view gainLabel;      // -6, 0, +6, +12
    };
    [[nodiscard]] WaveReference wave(ToneIndex tone) const;

    struct Envelope
    {
        std::array<int, 4> timeRaw{};    // T1..T4, 0..127
        std::array<int, 4> levelRaw{};   // L1..L4 (L4 unused for the level envelope)
        std::array<int, 4> levelDisplay{};
        int levelCount = 4;
        std::string_view name;
    };
    [[nodiscard]] Envelope pitchEnvelope(ToneIndex tone) const;
    [[nodiscard]] Envelope filterEnvelope(ToneIndex tone) const;
    [[nodiscard]] Envelope levelEnvelope(ToneIndex tone) const; // Roland's TVA "Level Envelope": 3 levels

    // Compact human-readable line used by diagnostics ("Warm Orchest · tones 1,2 · ...").
    [[nodiscard]] std::string summary() const;

    friend bool operator==(const Xp60Patch& lhs, const Xp60Patch& rhs) noexcept;

private:
    BlockValues m_common;
    std::array<BlockValues, ToneIndex::kCount> m_tones;
};

} // namespace xp60studio::xpmodel
