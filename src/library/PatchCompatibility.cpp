#include "library/PatchCompatibility.h"

#include "library/ExpansionBoardCatalog.h"

#include "xpmodel/Xp60WaveIdentifier.h"

#include <algorithm>

namespace xp60studio::library {

using xpmodel::ToneIndex;

std::string_view toneCompatibilityName(ToneCompatibility value) noexcept
{
    switch (value) {
    case ToneCompatibility::Disabled:
        return "disabled";
    case ToneCompatibility::Internal:
        return "internal";
    case ToneCompatibility::ExpansionAvailable:
        return "expansion-available";
    case ToneCompatibility::ExpansionMissing:
        return "expansion-missing";
    case ToneCompatibility::ExpansionUnknown:
        return "expansion-unknown";
    }
    return "unknown";
}

std::string_view toneCompatibilityLabel(ToneCompatibility value) noexcept
{
    switch (value) {
    case ToneCompatibility::Disabled:
        return "OFF";
    case ToneCompatibility::Internal:
        return "INTERNAL";
    case ToneCompatibility::ExpansionAvailable:
        return "EXPANSION";
    case ToneCompatibility::ExpansionMissing:
        return "BOARD MISSING";
    case ToneCompatibility::ExpansionUnknown:
        return "BOARD UNKNOWN";
    }
    return "";
}

int PatchCompatibilityReport::missingEnabledTones() const noexcept
{
    return static_cast<int>(std::count_if(tones.begin(), tones.end(), [](const ToneCompatibilityReport& tone) {
        return tone.enabled && tone.status == ToneCompatibility::ExpansionMissing;
    }));
}

int PatchCompatibilityReport::unknownTones() const noexcept
{
    return static_cast<int>(std::count_if(tones.begin(), tones.end(), [](const ToneCompatibilityReport& tone) {
        return tone.status == ToneCompatibility::ExpansionUnknown;
    }));
}

namespace {

// Groups are named where a board of that number is known — "5 (SR-JV80-05
// World)" reads better than "5" and is what a musician can act on. The number
// leads, because it is the fact the Tone actually carries; the board name is the
// inference resting on it (ROLAND_XP60_PROTOCOL_FACTS.md §7).
std::string joinGroups(const std::set<int>& groups)
{
    std::string out;
    for (const int group : groups) {
        if (!out.empty()) {
            out += ", ";
        }
        out += std::to_string(group);
        if (const auto board = srJv80BoardName(group)) {
            out += " (" + *board + ")";
        }
    }
    return out;
}

} // namespace

std::string PatchCompatibilityReport::summary() const
{
    if (!usesExpansion()) {
        return "Internal waves only — plays on any XP-60.";
    }
    const std::string needs = "Needs expansion group" + std::string(requiredGroups.size() == 1 ? " " : "s ")
        + joinGroups(requiredGroups);

    if (undecided()) {
        return needs + " — XP60Studio cannot tell whether you have "
            + std::string(requiredGroups.size() == 1 ? "it" : "them")
            + " until the expansion slots are filled in.";
    }
    if (missingGroups.empty()) {
        return needs + " — all installed.";
    }
    const std::string missing = "group" + std::string(missingGroups.size() == 1 ? " " : "s ")
        + joinGroups(missingGroups) + (missingGroups.size() == 1 ? " is" : " are") + " not installed";
    if (missingEnabledTones() == 0) {
        // Only switched-off Tones want the missing board, so the Patch plays as
        // it stands. Saying "incompatible" would be wrong and would train the
        // musician to ignore the warning.
        return needs + " — " + missing + ", but only switched-off Tones use "
            + std::string(missingGroups.size() == 1 ? "it" : "them") + ".";
    }
    return needs + " — " + missing + ".";
}

PatchCompatibilityReport analysePatch(const xpmodel::Xp60Patch& patch, const ExpansionProfile& profile)
{
    PatchCompatibilityReport report;

    // With nothing declared, every expansion reference is undecidable. A
    // musician who has not filled the profile in has not told us they lack a
    // board, and reporting "missing" would be an invention.
    const bool profileUsable = !profile.isEmpty();

    for (const auto tone : ToneIndex::all()) {
        auto& row = report.tones[static_cast<std::size_t>(tone.index())];
        row.toneNumber = tone.number();
        row.enabled = patch.toneEnabled(tone);

        const auto wave = patch.wave(tone);
        const auto expansion = xpmodel::expansionWave(wave.groupTypeRaw, wave.groupId, wave.numberRaw);
        if (!expansion) {
            // Internal, or Roland's `<PCM>` group type the XP-60 ignores.
            // Either way no board is required for it.
            row.status = row.enabled ? ToneCompatibility::Internal : ToneCompatibility::Disabled;
            continue;
        }

        row.waveGroupId = expansion->groupIdRaw;
        row.waveNumberRaw = expansion->numberRaw;
        // A disabled Tone's requirement is still recorded: switching it on is a
        // normal edit, and the musician deserves to know what that would need.
        report.requiredGroups.insert(expansion->groupIdRaw);

        const auto slot = profile.slotProviding(expansion->groupIdRaw);
        if (slot) {
            row.providedBySlot = slot;
            row.status = ToneCompatibility::ExpansionAvailable;
            continue;
        }
        if (!profileUsable || profile.anyGroupUnknown()) {
            // Either nothing is declared, or a declared board might be this one
            // and we cannot yet tell. Both are honestly "cannot say".
            row.status = ToneCompatibility::ExpansionUnknown;
            continue;
        }
        row.status = ToneCompatibility::ExpansionMissing;
        report.missingGroups.insert(expansion->groupIdRaw);
    }

    // A Tone that is switched off still reports Disabled in preference to
    // Internal, but an expansion verdict is more informative than "off", so the
    // expansion states above are left as they are.
    for (auto& row : report.tones) {
        if (!row.enabled && row.status == ToneCompatibility::Internal) {
            row.status = ToneCompatibility::Disabled;
        }
    }

    return report;
}

std::set<int> requiredExpansionGroups(const xpmodel::Xp60Patch& patch)
{
    std::set<int> groups;
    for (const auto tone : ToneIndex::all()) {
        const auto wave = patch.wave(tone);
        if (const auto expansion = xpmodel::expansionWave(wave.groupTypeRaw, wave.groupId, wave.numberRaw)) {
            groups.insert(expansion->groupIdRaw);
        }
    }
    return groups;
}

std::set<int> requiredExpansionGroups(const std::vector<xpmodel::Xp60Patch>& patches)
{
    std::set<int> groups;
    for (const auto& patch : patches) {
        const auto one = requiredExpansionGroups(patch);
        groups.insert(one.begin(), one.end());
    }
    return groups;
}

} // namespace xp60studio::library
