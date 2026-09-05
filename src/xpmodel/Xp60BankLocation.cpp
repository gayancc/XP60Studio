#include "xpmodel/Xp60BankLocation.h"

#include <cctype>

namespace xp60studio::xpmodel {
namespace {

std::string padded(int value, std::size_t width)
{
    std::string text = std::to_string(value);
    while (text.size() < width) {
        text.insert(text.begin(), '0');
    }
    return text;
}

} // namespace

std::optional<Xp60BankLocation> Xp60BankLocation::fromPanel(int subgroup, int bank, int number) noexcept
{
    if (!isValidSubgroup(subgroup) || !isValidBank(bank) || !isValidNumber(number)) {
        return std::nullopt;
    }
    return Xp60BankLocation(subgroup, bank, number);
}

std::optional<Xp60BankLocation> Xp60BankLocation::fromSlotIndex(int slotIndex) noexcept
{
    if (!isValidSlotIndex(slotIndex)) {
        return std::nullopt;
    }
    const int subgroup = slotIndex / kPatchesPerSubgroup;
    const int within = slotIndex % kPatchesPerSubgroup;
    return Xp60BankLocation(subgroup, within / kNumbersPerBank + 1, within % kNumbersPerBank + 1);
}

std::optional<Xp60BankLocation> Xp60BankLocation::fromUserNumber(int userNumber) noexcept
{
    return fromSlotIndex(userNumber - 1);
}

std::optional<Xp60BankLocation> Xp60BankLocation::fromPanelLabel(std::string_view label)
{
    if (label.size() != 3) {
        return std::nullopt;
    }
    const char letter = static_cast<char>(std::toupper(static_cast<unsigned char>(label[0])));
    int subgroup = -1;
    if (letter == 'A') {
        subgroup = 0;
    } else if (letter == 'B') {
        subgroup = 1;
    } else {
        return std::nullopt;
    }
    const auto digit = [](char c) -> int {
        return (c >= '0' && c <= '9') ? c - '0' : -1;
    };
    return fromPanel(subgroup, digit(label[1]), digit(label[2]));
}

std::string Xp60BankLocation::subgroupName(int subgroup)
{
    switch (subgroup) {
    case 0:
        return "A";
    case 1:
        return "B";
    default:
        return "?";
    }
}

std::string Xp60BankLocation::linearLabelFor(int slotIndex)
{
    if (!isValidSlotIndex(slotIndex)) {
        return "---";
    }
    return padded(slotIndex + 1, 3);
}

std::string Xp60BankLocation::subgroupLabel() const
{
    return subgroupName(m_subgroup);
}

std::string Xp60BankLocation::panelLabel() const
{
    return subgroupLabel() + std::to_string(m_bank) + std::to_string(m_number);
}

std::string Xp60BankLocation::linearLabel() const
{
    return padded(userNumber(), 3);
}

std::string Xp60BankLocation::spokenLabel() const
{
    return subgroupLabel() + " \xC2\xB7 BANK " + std::to_string(m_bank) + " \xC2\xB7 " + std::to_string(m_number);
}

} // namespace xp60studio::xpmodel
