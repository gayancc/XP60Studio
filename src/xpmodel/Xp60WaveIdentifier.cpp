#include "xpmodel/Xp60WaveIdentifier.h"

namespace xp60studio::xpmodel {

std::string_view waveBankLabel(InternalWaveBank bank) noexcept
{
    return bank == InternalWaveBank::IntA ? "INT-A" : "INT-B";
}

std::optional<InternalWaveBank> internalWaveBank(int groupTypeRaw, int groupIdRaw) noexcept
{
    if (groupTypeRaw != kInternalWaveGroupTypeRaw) {
        return std::nullopt; // EXP, or a group type the instrument has not shown us
    }
    switch (groupIdRaw) {
    case kIntAGroupIdRaw:
        return InternalWaveBank::IntA;
    case kIntBGroupIdRaw:
        return InternalWaveBank::IntB;
    default:
        return std::nullopt;
    }
}

std::optional<InternalWaveBank> internalWaveBankFromLabel(std::string_view label) noexcept
{
    if (label == waveBankLabel(InternalWaveBank::IntA)) {
        return InternalWaveBank::IntA;
    }
    if (label == waveBankLabel(InternalWaveBank::IntB)) {
        return InternalWaveBank::IntB;
    }
    return std::nullopt;
}

std::optional<WaveIdentifier> encodeWave(const WaveSelection& selection) noexcept
{
    if (selection.displayNumber < 1 || selection.displayNumber > waveBankSize(selection.bank)) {
        return std::nullopt;
    }
    WaveIdentifier identifier;
    identifier.groupTypeRaw = kInternalWaveGroupTypeRaw;
    identifier.groupIdRaw = selection.bank == InternalWaveBank::IntA ? kIntAGroupIdRaw : kIntBGroupIdRaw;
    identifier.numberRaw = selection.displayNumber - 1;
    return identifier;
}

std::optional<WaveSelection> decodeWave(int groupTypeRaw, int groupIdRaw, int numberRaw) noexcept
{
    const auto bank = internalWaveBank(groupTypeRaw, groupIdRaw);
    if (!bank) {
        return std::nullopt;
    }
    if (numberRaw < 0 || numberRaw >= waveBankSize(*bank)) {
        return std::nullopt;
    }
    return WaveSelection{*bank, numberRaw + 1};
}

} // namespace xp60studio::xpmodel
