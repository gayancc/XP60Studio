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

std::optional<WaveIdentifier> encodeExpansionWave(int waveGroupId, int displayNumber) noexcept
{
    if (waveGroupId < 0 || waveGroupId > 127) {
        return std::nullopt;
    }
    if (displayNumber < 1 || displayNumber > 256) {
        return std::nullopt;
    }
    return WaveIdentifier{kExpansionWaveGroupTypeRaw, waveGroupId, displayNumber - 1};
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

std::optional<ExpansionWaveReference> expansionWave(int groupTypeRaw, int groupIdRaw, int numberRaw) noexcept
{
    if (groupTypeRaw != kExpansionWaveGroupTypeRaw) {
        return std::nullopt;
    }
    // The documented field widths: group ID is one 7-bit byte, the wave number
    // two nibbles. Anything outside them did not come from an XP-60.
    if (groupIdRaw < 0 || groupIdRaw > 127 || numberRaw < 0 || numberRaw > 254) {
        return std::nullopt;
    }
    return ExpansionWaveReference{groupIdRaw, numberRaw};
}

} // namespace xp60studio::xpmodel
