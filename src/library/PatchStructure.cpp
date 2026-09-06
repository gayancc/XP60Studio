#include "library/PatchStructure.h"

#include <algorithm>

namespace xp60studio::library {

using xpmodel::CommonParameter;
using xpmodel::ToneIndex;
using xpmodel::ToneParameter;

namespace {

std::string join(const std::vector<std::string>& parts, const char* separator)
{
    std::string out;
    for (const auto& part : parts) {
        if (!out.empty()) {
            out += separator;
        }
        out += part;
    }
    return out;
}

std::string listOf(const std::set<int>& values)
{
    std::vector<std::string> parts;
    parts.reserve(values.size());
    for (int value : values) {
        parts.push_back(std::to_string(value));
    }
    return join(parts, ", ");
}

} // namespace

PatchStructure PatchStructure::of(const xpmodel::Xp60Patch& patch)
{
    PatchStructure structure;
    structure.enabledToneCount = patch.enabledToneCount();
    structure.structureType12Raw = patch.raw(CommonParameter::StructureType12);
    structure.structureType34Raw = patch.raw(CommonParameter::StructureType34);
    structure.booster12 = patch.raw(CommonParameter::Booster12) != 0;
    structure.booster34 = patch.raw(CommonParameter::Booster34) != 0;
    structure.efxTypeRaw = patch.raw(CommonParameter::EfxType);
    structure.reverbTypeRaw = patch.raw(CommonParameter::ReverbType);
    structure.chorusLevel = patch.raw(CommonParameter::ChorusLevel);
    structure.reverbLevel = patch.raw(CommonParameter::ReverbLevel);
    structure.keyAssignModeRaw = patch.raw(CommonParameter::KeyAssignMode);
    structure.portamento = patch.raw(CommonParameter::PortamentoSwitch) != 0;

    for (const auto index : ToneIndex::all()) {
        auto& tone = structure.tones[index.index()];
        tone.number = index.number();
        tone.enabled = patch.toneEnabled(index);

        const auto wave = patch.wave(index);
        tone.waveGroupTypeRaw = wave.groupTypeRaw;
        tone.waveGroupId = wave.groupId;
        tone.waveNumberDisplay = wave.numberDisplay;

        tone.filterTypeRaw = patch.raw(index, ToneParameter::FilterType);
        tone.keyLow = patch.raw(index, ToneParameter::KeyboardRangeLower);
        tone.keyHigh = patch.raw(index, ToneParameter::KeyboardRangeUpper);
        tone.velocityLow = patch.raw(index, ToneParameter::VelocityRangeLower);
        tone.velocityHigh = patch.raw(index, ToneParameter::VelocityRangeUpper);

        // A disabled Tone's stored wave is real data and is kept above, but it
        // does not put a board in the list of what the Patch needs to play.
        if (tone.enabled && tone.usesExpansionWave()) {
            structure.expansionGroups.insert(tone.waveGroupId);
        }
    }
    return structure;
}

bool PatchStructure::hasKeyboardSplit() const noexcept
{
    return std::any_of(tones.begin(), tones.end(),
                       [](const Tone& tone) { return tone.enabled && tone.isKeyLimited(); });
}

bool PatchStructure::hasVelocitySwitching() const noexcept
{
    return std::any_of(tones.begin(), tones.end(),
                       [](const Tone& tone) { return tone.enabled && tone.isVelocityLimited(); });
}

bool PatchStructure::isLayered() const noexcept
{
    // More than one Tone sounding, and none of them confined to a corner of the
    // keyboard or the velocity range: Tones stacked rather than divided up.
    return enabledToneCount > 1 && !hasKeyboardSplit() && !hasVelocitySwitching();
}

std::string PatchStructure::summary() const
{
    std::vector<std::string> parts;
    parts.push_back(std::to_string(enabledToneCount)
                    + (enabledToneCount == 1 ? " Tone" : " Tones"));
    if (hasKeyboardSplit()) {
        parts.emplace_back("keyboard split");
    }
    if (hasVelocitySwitching()) {
        parts.emplace_back("velocity switching");
    }
    if (isLayered()) {
        parts.emplace_back("layered");
    }
    if (portamento) {
        parts.emplace_back("portamento");
    }
    if (expansionGroups.empty()) {
        parts.emplace_back("internal waves only");
    } else {
        parts.push_back("expansion group" + std::string(expansionGroups.size() == 1 ? " " : "s ")
                        + listOf(expansionGroups));
    }
    return join(parts, ", ") + ".";
}

// ---------------------------------------------------------------------------

bool StructuralQuery::isEmpty() const noexcept
{
    return !minimumEnabledTones && !maximumEnabledTones && !keyboardSplit && !velocitySwitching
        && !usesExpansion && !portamento && filterTypes.empty() && efxTypes.empty()
        && reverbTypes.empty() && structureTypes.empty() && expansionGroups.empty() && !wave;
}

bool StructuralQuery::matches(const PatchStructure& structure) const
{
    if (minimumEnabledTones && structure.enabledToneCount < *minimumEnabledTones) {
        return false;
    }
    if (maximumEnabledTones && structure.enabledToneCount > *maximumEnabledTones) {
        return false;
    }
    if (keyboardSplit && structure.hasKeyboardSplit() != *keyboardSplit) {
        return false;
    }
    if (velocitySwitching && structure.hasVelocitySwitching() != *velocitySwitching) {
        return false;
    }
    if (usesExpansion && structure.usesExpansion() != *usesExpansion) {
        return false;
    }
    if (portamento && structure.portamento != *portamento) {
        return false;
    }
    if (!efxTypes.empty() && !efxTypes.contains(structure.efxTypeRaw)) {
        return false;
    }
    if (!reverbTypes.empty() && !reverbTypes.contains(structure.reverbTypeRaw)) {
        return false;
    }
    if (!structureTypes.empty() && !structureTypes.contains(structure.structureType12Raw)
        && !structureTypes.contains(structure.structureType34Raw)) {
        return false;
    }
    if (!expansionGroups.empty()) {
        const bool any = std::any_of(expansionGroups.begin(), expansionGroups.end(),
                                     [&structure](int group) {
                                         return structure.expansionGroups.contains(group);
                                     });
        if (!any) {
            return false;
        }
    }
    if (!filterTypes.empty()) {
        const bool any = std::any_of(structure.tones.begin(), structure.tones.end(),
                                     [this](const PatchStructure::Tone& tone) {
                                         return tone.enabled && filterTypes.contains(tone.filterTypeRaw);
                                     });
        if (!any) {
            return false;
        }
    }
    if (wave) {
        const auto plays = [this](const PatchStructure::Tone& tone) {
            return tone.waveGroupTypeRaw == wave->groupTypeRaw && tone.waveGroupId == wave->groupId
                && tone.waveNumberDisplay == wave->numberDisplay;
        };
        const auto enabled = [](const PatchStructure::Tone& tone) { return tone.enabled; };
        if (allTonesMatchWave) {
            // Vacuously true for a Patch with no enabled Tone would be a lie
            // about a silent Patch, so it has to have one.
            if (structure.enabledToneCount == 0) {
                return false;
            }
            for (const auto& tone : structure.tones) {
                if (enabled(tone) && !plays(tone)) {
                    return false;
                }
            }
        } else {
            const bool any = std::any_of(structure.tones.begin(), structure.tones.end(),
                                         [&](const PatchStructure::Tone& tone) {
                                             return enabled(tone) && plays(tone);
                                         });
            if (!any) {
                return false;
            }
        }
    }
    return true;
}

bool StructuralQuery::matches(const xpmodel::Xp60Patch& patch) const
{
    return matches(PatchStructure::of(patch));
}

std::string StructuralQuery::describe() const
{
    if (isEmpty()) {
        return "Every Patch.";
    }
    std::vector<std::string> parts;
    if (minimumEnabledTones && maximumEnabledTones && *minimumEnabledTones == *maximumEnabledTones) {
        parts.push_back("exactly " + std::to_string(*minimumEnabledTones)
                        + (*minimumEnabledTones == 1 ? " Tone" : " Tones"));
    } else {
        if (minimumEnabledTones) {
            parts.push_back("at least " + std::to_string(*minimumEnabledTones) + " Tones");
        }
        if (maximumEnabledTones) {
            parts.push_back("at most " + std::to_string(*maximumEnabledTones) + " Tones");
        }
    }
    if (keyboardSplit) {
        parts.emplace_back(*keyboardSplit ? "a keyboard split" : "no keyboard split");
    }
    if (velocitySwitching) {
        parts.emplace_back(*velocitySwitching ? "velocity switching" : "no velocity switching");
    }
    if (usesExpansion) {
        parts.emplace_back(*usesExpansion ? "an expansion wave" : "internal waves only");
    }
    if (portamento) {
        parts.emplace_back(*portamento ? "portamento on" : "portamento off");
    }
    if (!filterTypes.empty()) {
        parts.push_back("filter type " + listOf(filterTypes));
    }
    if (!efxTypes.empty()) {
        parts.push_back("EFX type " + listOf(efxTypes));
    }
    if (!reverbTypes.empty()) {
        parts.push_back("reverb type " + listOf(reverbTypes));
    }
    if (!structureTypes.empty()) {
        parts.push_back("structure type " + listOf(structureTypes));
    }
    if (!expansionGroups.empty()) {
        parts.push_back("expansion group " + listOf(expansionGroups));
    }
    if (wave) {
        parts.push_back(std::string(allTonesMatchWave ? "every Tone playing" : "a Tone playing")
                        + " wave " + std::to_string(wave->numberDisplay) + " of group "
                        + std::to_string(wave->groupId));
    }
    return "Patches with " + join(parts, ", ") + ".";
}

} // namespace xp60studio::library
