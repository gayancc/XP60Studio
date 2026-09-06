#pragma once

#include "xpmodel/BlockCodec.h"
#include "xpmodel/PatchName.h"
#include "xpmodel/Xp60PerformanceLayout.h"
#include "xpmodel/generated/Xp60PerformanceTables.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace xp60studio::xpmodel {

using xp60performance::PerformanceCommonParameter;
using xp60performance::PerformancePartParameter;

// A complete XP-60 Performance: Performance Common plus Parts 1-16.
//
// The Performance counterpart of Xp60Patch, and deliberately the same shape:
// it owns the decoded BlockValues of its seventeen blocks, typed access goes
// through the generated parameter enumerations so an identifier typo is a
// compile error, and nothing here re-derives an offset or a range.
//
// A Performance references Patches rather than containing them. Each Part names
// one by group type, group ID and number, so the model can say *which* Patch a
// Part plays without owning it — that is `PartAssignment`.
class Xp60Performance
{
public:
    Xp60Performance(BlockValues common, std::array<BlockValues, PartIndex::kCount> parts);

    // Blocks ------------------------------------------------------------------
    [[nodiscard]] const BlockValues& common() const noexcept { return m_common; }
    [[nodiscard]] BlockValues& common() noexcept { return m_common; }
    [[nodiscard]] const BlockValues& part(PartIndex part) const noexcept { return m_parts[part.index()]; }
    [[nodiscard]] BlockValues& part(PartIndex part) noexcept { return m_parts[part.index()]; }

    // Typed parameter access ---------------------------------------------------
    [[nodiscard]] int raw(PerformanceCommonParameter parameter) const noexcept;
    [[nodiscard]] int raw(PartIndex part, PerformancePartParameter parameter) const noexcept;
    [[nodiscard]] int display(PerformanceCommonParameter parameter) const noexcept;
    [[nodiscard]] int display(PartIndex part, PerformancePartParameter parameter) const noexcept;
    [[nodiscard]] std::string displayText(PerformanceCommonParameter parameter) const;
    [[nodiscard]] std::string displayText(PartIndex part, PerformancePartParameter parameter) const;
    // False when the raw value is outside the documented range; nothing is clamped.
    bool setRaw(PerformanceCommonParameter parameter, int raw) noexcept;
    bool setRaw(PartIndex part, PerformancePartParameter parameter, int raw) noexcept;

    // Musical views --------------------------------------------------------------
    [[nodiscard]] PatchName name() const;
    bool setName(const PatchName& name) noexcept;

    // Which Patch a Part plays, as the Part's own bytes name it. Deliberately
    // raw: resolving group type and ID to a bank is the same problem the Patch
    // editor's wave reference has, and is not this type's to guess at.
    struct PartAssignment
    {
        int groupTypeRaw = 0;            // 0 USER&PRESET, 1 <PCM>, 2 EXP
        std::string_view groupTypeLabel;
        int groupId = 0;
        int numberRaw = 0;               // 0..254
        int numberDisplay = 1;           // 1..255
    };
    [[nodiscard]] PartAssignment assignment(PartIndex part) const;

    // How a Part sits in the mix. The 16-Part mixer reads exactly this.
    struct PartMix
    {
        bool receives = true;            // Receive Switch
        int midiChannel = 1;             // 1..16
        int level = 0;                   // 0..127
        int pan = 0;                     // raw 0..127
        std::string panText;             // Roland's L64..0..63R
        int outputAssignRaw = 0;
        std::string_view outputAssignLabel;
        int chorusSend = 0;
        int reverbSend = 0;
        int voiceReserve = 0;            // from Performance Common, indexed by Part
    };
    [[nodiscard]] PartMix mix(PartIndex part) const;

    // The keyboard span and transposition a Part answers to.
    struct PartRange
    {
        int lowerRaw = 0;
        int upperRaw = 127;
        std::string lowerNote;           // C-1 .. G9
        std::string upperNote;
        int octaveShift = 0;             // -3..+3
        int coarseTune = 0;              // -48..+48
        int fineTune = 0;                // -50..+50
    };
    [[nodiscard]] PartRange range(PartIndex part) const;

    // Parts that would sound: switched on and reserving at least one voice.
    [[nodiscard]] int activePartCount() const noexcept;

    // Compact human-readable line used by diagnostics.
    [[nodiscard]] std::string summary() const;

    friend bool operator==(const Xp60Performance& lhs, const Xp60Performance& rhs) noexcept;

private:
    BlockValues m_common;
    std::array<BlockValues, PartIndex::kCount> m_parts;
};

} // namespace xp60studio::xpmodel
