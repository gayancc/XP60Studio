#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace xp60studio::xpmodel {

// Where a User Patch lives, said the way the XP-60's front panel says it.
//
// The instrument does not ask for "patch 21". It asks for a PATCH GROUP, then
// a subgroup, then BANK 1-8, then NUMBER 1-8:
//
//     USER -> A -> BANK 3 -> NUMBER 5
//
// The User group holds 128 Patches, which is two subgroups of 8x8:
//
//     A11 = 001   A18 = 008   A21 = 009   ...   A88 = 064
//     B11 = 065   B18 = 072   B21 = 073   ...   B88 = 128
//
// This is a **selection convention of the front panel**, not a protocol fact.
// The protocol identity of a User Patch is its linear number 001-128, which is
// what `library::PatchProvenance::userNumber` records and what the User Patch
// bank address `11 nn 00 00` is derived from (docs/protocol/
// ROLAND_XP60_PROTOCOL_FACTS.md). Nothing here changes an address, invents one
// or reorders anything: it is one bijection between the number the instrument
// transmits and the three controls a player actually presses.
//
// Keeping it in one type means the mapping is written once and tested once.
// The musician never calculates it, and no screen re-derives it with its own
// arithmetic.
class Xp60BankLocation
{
public:
    static constexpr int kNumbersPerBank = 8;    // NUMBER 1-8
    static constexpr int kBanksPerSubgroup = 8;  // BANK 1-8
    static constexpr int kSubgroupCount = 2;     // A, B
    static constexpr int kPatchesPerSubgroup = kBanksPerSubgroup * kNumbersPerBank; // 64
    static constexpr int kUserPatchCount = kSubgroupCount * kPatchesPerSubgroup;    // 128

    // Subgroup A, BANK 1, NUMBER 1 — the first User Patch.
    constexpr Xp60BankLocation() = default;

    // Nullopt for any coordinate outside the panel's range, rather than a
    // clamped location that would silently point somewhere else.
    [[nodiscard]] static std::optional<Xp60BankLocation> fromPanel(int subgroup, int bank, int number) noexcept;
    // `userNumber` is 1-128 as the instrument numbers User Patches.
    [[nodiscard]] static std::optional<Xp60BankLocation> fromUserNumber(int userNumber) noexcept;
    // `slotIndex` is 0-127, the zero-based position inside a 128-Patch bank.
    [[nodiscard]] static std::optional<Xp60BankLocation> fromSlotIndex(int slotIndex) noexcept;
    // "A35", "B88". Case-insensitive; nullopt for anything else.
    [[nodiscard]] static std::optional<Xp60BankLocation> fromPanelLabel(std::string_view label);

    [[nodiscard]] constexpr int subgroup() const noexcept { return m_subgroup; } // 0 = A, 1 = B
    [[nodiscard]] constexpr int bank() const noexcept { return m_bank; }         // 1-8
    [[nodiscard]] constexpr int number() const noexcept { return m_number; }     // 1-8

    [[nodiscard]] constexpr int slotIndex() const noexcept
    {
        return m_subgroup * kPatchesPerSubgroup + (m_bank - 1) * kNumbersPerBank + (m_number - 1);
    }
    [[nodiscard]] constexpr int userNumber() const noexcept { return slotIndex() + 1; }

    // "A" or "B".
    [[nodiscard]] std::string subgroupLabel() const;
    // "A35" — the panel identity, and the primary one everywhere in the UI.
    [[nodiscard]] std::string panelLabel() const;
    // "021" — the linear identity, always shown as supporting information.
    [[nodiscard]] std::string linearLabel() const;
    // "A · BANK 3 · 5", for the instrument display.
    [[nodiscard]] std::string spokenLabel() const;

    friend constexpr bool operator==(const Xp60BankLocation&, const Xp60BankLocation&) noexcept = default;

    [[nodiscard]] static bool isValidSubgroup(int subgroup) noexcept
    {
        return subgroup >= 0 && subgroup < kSubgroupCount;
    }
    [[nodiscard]] static bool isValidBank(int bank) noexcept { return bank >= 1 && bank <= kBanksPerSubgroup; }
    [[nodiscard]] static bool isValidNumber(int number) noexcept { return number >= 1 && number <= kNumbersPerBank; }
    [[nodiscard]] static bool isValidSlotIndex(int slotIndex) noexcept
    {
        return slotIndex >= 0 && slotIndex < kUserPatchCount;
    }
    [[nodiscard]] static bool isValidUserNumber(int userNumber) noexcept
    {
        return isValidSlotIndex(userNumber - 1);
    }

    // "A" / "B" without constructing a location, for headers and pickers.
    [[nodiscard]] static std::string subgroupName(int subgroup);
    // The linear identity of a slot index, zero-padded to three digits.
    [[nodiscard]] static std::string linearLabelFor(int slotIndex);

private:
    constexpr Xp60BankLocation(int subgroup, int bank, int number) noexcept
        : m_subgroup(subgroup)
        , m_bank(bank)
        , m_number(number)
    {
    }

    int m_subgroup = 0;
    int m_bank = 1;
    int m_number = 1;
};

} // namespace xp60studio::xpmodel
