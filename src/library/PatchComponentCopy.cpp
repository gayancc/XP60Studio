#include "library/PatchComponentCopy.h"

#include "library/ExpansionBoardCatalog.h"
#include "xpmodel/Xp60PatchLayout.h"

#include <algorithm>

namespace xp60studio::library {

using xpmodel::ToneIndex;
using xpmodel::Xp60Patch;
using xpmodel::Xp60PatchLayout;

namespace {

bool isNameCategory(std::string_view category) noexcept
{
    return category == "Name";
}

} // namespace

std::string_view patchComponentName(PatchComponent component) noexcept
{
    switch (component) {
    case PatchComponent::WholeTone:
        return "Whole Tone";
    case PatchComponent::Wave:
        return "Wave";
    case PatchComponent::Pitch:
        return "Pitch";
    case PatchComponent::PitchEnvelope:
        return "Pitch Envelope";
    case PatchComponent::Filter:
        return "Filter (TVF)";
    case PatchComponent::FilterEnvelope:
        return "Filter Envelope";
    case PatchComponent::Amplifier:
        return "Amplifier (TVA)";
    case PatchComponent::AmplifierEnvelope:
        return "Amplifier Envelope";
    case PatchComponent::Lfo1:
        return "LFO 1";
    case PatchComponent::Lfo2:
        return "LFO 2";
    case PatchComponent::Controllers:
        return "Controllers";
    case PatchComponent::ControlSwitches:
        return "Control Switches";
    case PatchComponent::KeyVelocityRange:
        return "Key and Velocity Range";
    case PatchComponent::ToneDelay:
        return "Tone Delay";
    case PatchComponent::TonePan:
        return "Tone Pan";
    case PatchComponent::ToneEffectSends:
        return "Tone Effect Sends";
    case PatchComponent::PatchEffects:
        return "Patch Effects";
    case PatchComponent::PatchCommonSettings:
        return "Patch Common Settings";
    case PatchComponent::Structure:
        return "Structure";
    }
    return "Unknown";
}

bool patchComponentIsToneScoped(PatchComponent component) noexcept
{
    switch (component) {
    case PatchComponent::PatchEffects:
    case PatchComponent::PatchCommonSettings:
    case PatchComponent::Structure:
        return false;
    default:
        return true;
    }
}

std::vector<PatchComponent> allPatchComponents()
{
    return {PatchComponent::WholeTone,          PatchComponent::Wave,
            PatchComponent::Pitch,              PatchComponent::PitchEnvelope,
            PatchComponent::Filter,             PatchComponent::FilterEnvelope,
            PatchComponent::Amplifier,          PatchComponent::AmplifierEnvelope,
            PatchComponent::Lfo1,               PatchComponent::Lfo2,
            PatchComponent::Controllers,        PatchComponent::ControlSwitches,
            PatchComponent::KeyVelocityRange,   PatchComponent::ToneDelay,
            PatchComponent::TonePan,            PatchComponent::ToneEffectSends,
            PatchComponent::PatchEffects,       PatchComponent::PatchCommonSettings,
            PatchComponent::Structure};
}

std::vector<std::string_view> PatchComponentCopy::categoriesOf(PatchComponent component)
{
    switch (component) {
    case PatchComponent::WholeTone:
        // Every Tone category. Listed rather than special-cased so that a UI
        // showing "what this will touch" shows the same thing the copy does.
        return {"Tone",  "Wave",       "Pitch",  "Pitch Envelope",   "TVF",
                "TVF Envelope",        "TVA",    "TVA Envelope",     "LFO1",
                "LFO2",  "Controllers", "Control Switches",          "Range",
                "Tone Delay",          "Pan",    "EFX",              "Chorus",
                "Reverb", "Output"};
    case PatchComponent::Wave:
        return {"Wave"};
    case PatchComponent::Pitch:
        return {"Pitch"};
    case PatchComponent::PitchEnvelope:
        return {"Pitch Envelope"};
    case PatchComponent::Filter:
        return {"TVF"};
    case PatchComponent::FilterEnvelope:
        return {"TVF Envelope"};
    case PatchComponent::Amplifier:
        return {"TVA"};
    case PatchComponent::AmplifierEnvelope:
        return {"TVA Envelope"};
    case PatchComponent::Lfo1:
        return {"LFO1"};
    case PatchComponent::Lfo2:
        return {"LFO2"};
    case PatchComponent::Controllers:
        return {"Controllers"};
    case PatchComponent::ControlSwitches:
        return {"Control Switches"};
    case PatchComponent::KeyVelocityRange:
        return {"Range"};
    case PatchComponent::ToneDelay:
        return {"Tone Delay"};
    case PatchComponent::TonePan:
        return {"Pan"};
    case PatchComponent::ToneEffectSends:
        return {"EFX", "Chorus", "Reverb", "Output"};
    case PatchComponent::PatchEffects:
        return {"EFX", "Chorus", "Reverb"};
    case PatchComponent::PatchCommonSettings:
        return {"Common", "Portamento", "Pan", "Range"};
    case PatchComponent::Structure:
        return {"Structure"};
    }
    return {};
}

std::string PatchCopyResult::summary() const
{
    if (!ok) {
        return reason;
    }
    std::string out = "Copied " + std::to_string(parametersCopied)
        + (parametersCopied == 1 ? " parameter" : " parameters") + "; "
        + std::to_string(parametersChanged)
        + (parametersChanged == 1 ? " differed." : " differed.");
    for (const auto& note : notes) {
        out += " " + note;
    }
    return out;
}

PatchCopyResult PatchComponentCopy::apply(const Xp60Patch& destination, const Xp60Patch& source,
                                          const PatchCopyRequest& request)
{
    PatchCopyResult result;
    const bool toneScoped = patchComponentIsToneScoped(request.component);
    if (toneScoped && (!request.fromTone || !request.toTone)) {
        result.reason = std::string(patchComponentName(request.component))
            + " belongs to a Tone, so a source and a destination Tone are needed.";
        return result;
    }

    const auto categories = categoriesOf(request.component);
    const auto wanted = [&categories](std::string_view category) {
        return std::find(categories.begin(), categories.end(), category) != categories.end();
    };

    // Work on a copy: a component half applied would leave a Patch nobody
    // designed, so nothing is returned unless everything was accepted.
    Xp60Patch working = destination;
    const auto& table = toneScoped ? Xp60PatchLayout::patchToneTable()
                                   : Xp60PatchLayout::patchCommonTable();
    const auto& sourceValues
        = toneScoped ? source.tone(*request.fromTone) : source.common();
    auto& targetValues = toneScoped ? working.tone(*request.toTone) : working.common();
    const auto& beforeValues
        = toneScoped ? destination.tone(*request.toTone) : destination.common();

    const auto parameters = table.parameters();
    for (std::size_t i = 0; i < parameters.size(); ++i) {
        // The Patch name is never copied: a Patch that took on the name of the
        // one a filter came from is a Patch nobody could find again.
        if (isNameCategory(parameters[i].category) || !wanted(parameters[i].category)) {
            continue;
        }
        const int value = sourceValues.rawAt(i);
        if (!targetValues.setRawAt(i, value)) {
            result.reason = "The destination refused " + std::string(parameters[i].name)
                + " = " + std::to_string(value)
                + ", so nothing was copied. The value is outside the documented range.";
            return result;
        }
        ++result.parametersCopied;
        if (beforeValues.rawAt(i) != value) {
            ++result.parametersChanged;
        }
    }

    if (result.parametersCopied == 0) {
        result.reason = std::string(patchComponentName(request.component))
            + " covers no parameters in this block.";
        return result;
    }

    // ── What stayed behind ─────────────────────────────────────────────────

    if (toneScoped) {
        const auto sourceWave = source.wave(*request.fromTone);
        const bool broughtWave = wanted("Wave");
        if (broughtWave && sourceWave.groupTypeRaw == 2) {
            result.notes.push_back("This Tone plays a wave from expansion group "
                                   + std::to_string(sourceWave.groupId) + " ("
                                   + std::string(describeWaveGroup(sourceWave.groupId))
                                   + "), which the destination will need too.");
        }
        // Structure Type pairs Tones 1&2 and 3&4 and lives in Patch Common, so
        // it is not part of any Tone and does not travel with one.
        result.notes.emplace_back(
            "Structure Type and the boosters pair Tones 1&2 and 3&4 in Patch Common, so they "
            "stayed with the destination Patch. Copy Structure separately if the pairing matters.");
        if (wanted("EFX")) {
            result.notes.emplace_back(
                "The Tone's effect sends came across, but the Patch's own EFX, Chorus and Reverb "
                "settings did not; the sends route into those.");
        }
        if (request.component == PatchComponent::WholeTone) {
            const bool wasOn = destination.toneEnabled(*request.toTone);
            const bool nowOn = working.toneEnabled(*request.toTone);
            if (wasOn != nowOn) {
                result.notes.push_back(nowOn
                    ? "The Tone switch came with it, so this Tone is now on."
                    : "The Tone switch came with it, so this Tone is now off.");
            }
        }
    } else if (request.component == PatchComponent::Structure) {
        result.notes.emplace_back(
            "Structure changes how the Tones already in the destination combine; it did not bring "
            "those Tones with it.");
    }

    result.ok = true;
    result.patch = std::move(working);
    return result;
}

} // namespace xp60studio::library
