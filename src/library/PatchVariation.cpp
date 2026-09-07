#include "library/PatchVariation.h"

#include "xpmodel/Xp60PatchLayout.h"

#include <algorithm>
#include <random>

namespace xp60studio::library {

using xpmodel::ParameterDescriptor;
using xpmodel::ToneIndex;
using xpmodel::Xp60Patch;
using xpmodel::Xp60PatchLayout;

namespace {

// A parameter whose two values are off and on. Roland gives many of these no
// enumeration, so the range is what identifies them.
bool isSwitch(const ParameterDescriptor& parameter) noexcept
{
    return parameter.rawMin == 0 && parameter.rawMax == 1;
}

} // namespace

bool PatchVariation::isContinuous(const ParameterDescriptor& parameter) noexcept
{
    return whyNotVaried(parameter).empty();
}

std::string_view PatchVariation::whyNotVaried(const ParameterDescriptor& parameter) noexcept
{
    if (parameter.isText() || parameter.category == "Name") {
        return "a Patch name is text, and naming a variation is the musician's job";
    }
    if (parameter.category == "Wave") {
        // Wave Number is a plain number in the data and an identity in the
        // instrument: wave 118 is not "near" wave 119.
        return "a wave is an identity, not a quantity";
    }
    if (parameter.isEnumeration()) {
        return "this parameter selects one of a list, so there is no 'slightly more'";
    }
    if (isSwitch(parameter)) {
        return "a switch is on or off; nudging it would just toggle it";
    }
    if (parameter.rawMax <= parameter.rawMin) {
        return "this parameter has no range to move within";
    }
    return {};
}

std::string PatchVariationResult::summary() const
{
    if (!ok) {
        return reason;
    }
    std::string out = "Moved " + std::to_string(changedParameters) + " of "
        + std::to_string(eligibleParameters) + " continuous parameters.";
    if (clampedParameters > 0) {
        out += " " + std::to_string(clampedParameters)
            + (clampedParameters == 1 ? " move was" : " moves were")
            + " cut short by a documented range edge.";
    }
    return out;
}

PatchVariationResult PatchVariation::apply(const Xp60Patch& patch,
                                           const PatchVariationOptions& options)
{
    PatchVariationResult result;
    if (options.amountPercent <= 0 || options.amountPercent > 100) {
        result.reason = "The variation amount must be between 1 and 100 percent of each "
                        "parameter's own range.";
        return result;
    }
    if (options.densityPercent <= 0 || options.densityPercent > 100) {
        result.reason = "The variation density must be between 1 and 100 percent.";
        return result;
    }

    // Which components are in scope, as the set of categories they cover.
    auto components = options.components;
    if (components.empty()) {
        components = allPatchComponents();
    }
    std::vector<std::string_view> toneCategories;
    std::vector<std::string_view> commonCategories;
    for (const auto component : components) {
        auto& into = patchComponentIsToneScoped(component) ? toneCategories : commonCategories;
        for (const auto category : PatchComponentCopy::categoriesOf(component)) {
            if (std::find(into.begin(), into.end(), category) == into.end()) {
                into.push_back(category);
            }
        }
    }

    auto tones = options.tones;
    if (tones.empty()) {
        const auto all = ToneIndex::all();
        tones.assign(all.begin(), all.end());
    }

    Xp60Patch working = patch;
    std::mt19937_64 random(options.seed);
    std::uniform_int_distribution<int> percent(1, 100);

    const auto vary = [&](const ParameterDescriptor& parameter, int before) {
        // The move is a fraction of this parameter's own range, so a coarse
        // field and a fine one are varied by comparable musical amounts rather
        // than by the same number of steps.
        const int span = parameter.rawMax - parameter.rawMin;
        const int reach = std::max(1, (span * options.amountPercent + 50) / 100);
        std::uniform_int_distribution<int> step(-reach, reach);
        int delta = step(random);
        if (delta == 0) {
            delta = 1;  // asked to move it, so move it
        }
        const int wanted = before + delta;
        const int clamped = std::clamp(wanted, parameter.rawMin, parameter.rawMax);
        return std::pair<int, bool>{clamped, clamped != wanted};
    };

    const auto runBlock = [&](const std::vector<std::string_view>& categories,
                              const xpmodel::ParameterTable& table, auto& values,
                              const auto& before, const std::string& blockName) {
        const auto parameters = table.parameters();
        for (std::size_t i = 0; i < parameters.size(); ++i) {
            const auto& parameter = parameters[i];
            if (std::find(categories.begin(), categories.end(), parameter.category)
                == categories.end()) {
                continue;
            }
            if (!isContinuous(parameter)) {
                continue;
            }
            ++result.eligibleParameters;
            // Draw for every eligible parameter whether or not it is chosen, so
            // the sequence depends only on the seed and the scope — not on how
            // many earlier parameters happened to be picked.
            const bool chosen = percent(random) <= options.densityPercent;
            const int original = before.rawAt(i);
            const auto [next, wasClamped] = vary(parameter, original);
            if (!chosen || next == original) {
                continue;
            }
            if (!values.setRawAt(i, next)) {
                continue;  // refused rather than clamped silently; leave it alone
            }
            if (wasClamped) {
                ++result.clampedParameters;
            }
            ++result.changedParameters;
            result.changes.push_back(VariationChange{blockName, std::string(parameter.name),
                                                     std::string(parameter.category), original,
                                                     next});
        }
    };

    if (!commonCategories.empty()) {
        runBlock(commonCategories, Xp60PatchLayout::patchCommonTable(), working.common(),
                 patch.common(), "Patch Common");
    }
    for (const auto tone : tones) {
        // A Tone nobody hears is not varied: it would be change without effect,
        // and it would make two variations that sound identical compare as
        // different.
        if (!patch.toneEnabled(tone)) {
            continue;
        }
        runBlock(toneCategories, Xp60PatchLayout::patchToneTable(), working.tone(tone),
                 patch.tone(tone), "Tone " + std::to_string(tone.number()));
    }

    if (result.eligibleParameters == 0) {
        result.reason = "Nothing in that scope can be varied: every parameter it covers is a "
                        "switch, a selection or an identity.";
        return result;
    }

    result.ok = true;
    result.patch = std::move(working);
    return result;
}

} // namespace xp60studio::library
