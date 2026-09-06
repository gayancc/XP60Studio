#include "sounddna/PatchFeatureExtractor.h"

#include "xpmodel/generated/Xp60PatchTables.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace xp60studio::sounddna {
namespace {

double normalized(const xpmodel::ParameterDescriptor& descriptor, int raw)
{
    const int span = descriptor.rawMax - descriptor.rawMin;
    return span > 0 ? static_cast<double>(raw - descriptor.rawMin) / span : 0.0;
}

bool protectedParameter(std::string_view id)
{
    // V1 transforms continuous, semantically established values only. The
    // complete raw record is still retained for analysis.
    return id.starts_with("common.name.") || id.starts_with("common.efx_parameter_")
        || id == "common.structure_type_1_2" || id == "common.structure_type_3_4"
        || id.starts_with("common.efx_control_source") || id.starts_with("common.patch_control_source")
        || id == "tone.tone_switch" || id.starts_with("tone.wave_")
        || id.starts_with("tone.keyboard_range_") || id.starts_with("tone.velocity_range_")
        || id.starts_with("tone.controller_") || id == "tone.output_assign";
}

FeatureKind kindOf(const xpmodel::ParameterDescriptor& descriptor)
{
    // Wave selectors are codes, even where the Roland table stores them in a
    // numeric-looking field. Their ordering and distance have no acoustic
    // meaning and must never enter a regression as continuous quantities.
    if (descriptor.isText() || descriptor.isEnumeration()
        || std::string_view(descriptor.id).starts_with("tone.wave_"))
        return FeatureKind::Categorical;
    return descriptor.displayStyle == xpmodel::DisplayStyle::Number ? FeatureKind::Continuous
                                                                    : FeatureKind::Ordinal;
}

std::string toneFeatureId(int tone, std::string_view descriptorId)
{
    std::string id = "tone." + std::to_string(tone) + ".";
    if (descriptorId.starts_with("tone.")) descriptorId.remove_prefix(5);
    id.append(descriptorId);
    return id;
}

void appendDerived(PatchFeatureVector& result, std::string id, double value)
{
    result.values.push_back({std::move(id), FeatureKind::Derived, value, 0, 0, 0, 0, false, true, {}});
}

void appendWaveIdentity(PatchFeatureVector& result, const xpmodel::Xp60Patch& patch,
                        xpmodel::ToneIndex tone, bool active)
{
    const auto wave = patch.wave(tone);
    std::string identity = std::to_string(wave.groupTypeRaw) + ":"
        + std::to_string(wave.groupId) + ":" + std::to_string(wave.numberRaw);
    result.values.push_back({"tone." + std::to_string(tone.number()) + ".wave_identity",
                             FeatureKind::Categorical, 0.0, 0, 0, 0, tone.number(), false,
                             active, std::move(identity)});
}

double standardDeviation(const std::vector<double>& values)
{
    if (values.empty()) return 0.0;
    const double count = static_cast<double>(values.size());
    const double mean = std::accumulate(values.begin(), values.end(), 0.0) / count;
    double sum = 0.0;
    for (const double value : values) sum += (value - mean) * (value - mean);
    return std::sqrt(sum / count);
}

} // namespace

const FeatureValue* PatchFeatureVector::find(std::string_view id) const noexcept
{
    const auto found = std::find_if(values.begin(), values.end(), [id](const auto& value) { return value.id == id; });
    return found == values.end() ? nullptr : &*found;
}

PatchFeatureVector PatchFeatureExtractor::extract(const xpmodel::Xp60Patch& patch, PatchContext context) const
{
    PatchFeatureVector result{std::string(kSchemaVersion), {}, std::move(context)};
    const auto& commonTable = xpmodel::xp60tables::patchCommonTable();
    result.values.reserve(commonTable.size() + xpmodel::ToneIndex::kCount * xpmodel::xp60tables::patchToneTable().size() + 8);

    for (std::size_t index = 0; index < commonTable.size(); ++index) {
        const auto& descriptor = commonTable.parameters()[index];
        const int raw = patch.common().rawAt(index);
        const auto kind = kindOf(descriptor);
        result.values.push_back({std::string(descriptor.id), kind,
                                 kind == FeatureKind::Categorical ? static_cast<double>(raw) : normalized(descriptor, raw),
                                 raw, descriptor.rawMin, descriptor.rawMax, 0,
                                 kind != FeatureKind::Categorical && !protectedParameter(descriptor.id), true, {}});
    }

    std::vector<double> levels;
    std::vector<double> pans;
    std::vector<double> tuning;
    for (const auto tone : xpmodel::ToneIndex::all()) {
        const bool enabled = patch.toneEnabled(tone);
        const auto& table = xpmodel::xp60tables::patchToneTable();
        for (std::size_t index = 0; index < table.size(); ++index) {
            const auto& descriptor = table.parameters()[index];
            const int raw = patch.tone(tone).rawAt(index);
            const auto kind = kindOf(descriptor);
            result.values.push_back({toneFeatureId(tone.number(), descriptor.id), kind,
                                     kind == FeatureKind::Categorical ? static_cast<double>(raw) : normalized(descriptor, raw),
                                     raw, descriptor.rawMin, descriptor.rawMax, tone.number(),
                                     enabled && kind != FeatureKind::Categorical && !protectedParameter(descriptor.id),
                                     enabled, {}});
        }
        appendWaveIdentity(result, patch, tone, enabled);
        if (enabled) {
            levels.push_back(normalized(xpmodel::xp60tables::descriptor(xpmodel::ToneParameter::ToneLevel),
                                        patch.raw(tone, xpmodel::ToneParameter::ToneLevel)));
            pans.push_back(normalized(xpmodel::xp60tables::descriptor(xpmodel::ToneParameter::TonePan),
                                      patch.raw(tone, xpmodel::ToneParameter::TonePan)));
            const double coarse = patch.display(tone, xpmodel::ToneParameter::CoarseTune) / 48.0;
            const double fine = patch.display(tone, xpmodel::ToneParameter::FineTune) / 100.0;
            tuning.push_back(coarse + fine / 12.0);
        }
    }

    appendDerived(result, "patch.active_tone_count", patch.enabledToneCount() / 4.0);
    appendDerived(result, "patch.tone_level_spread", standardDeviation(levels));
    appendDerived(result, "patch.pan_spread", standardDeviation(pans));
    appendDerived(result, "patch.tuning_spread", standardDeviation(tuning));
    return result;
}

} // namespace xp60studio::sounddna
