#include "sounddna/SoundDnaKnowledgeModel.h"

#include "sounddna/PatchFeatureExtractor.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace xp60studio::sounddna {

const SoundDnaValue* SoundDnaProfile::find(std::string_view id) const noexcept
{
    const auto found = std::find_if(values.begin(), values.end(), [id](const auto& value) { return value.id == id; });
    return found == values.end() ? nullptr : &*found;
}

SoundDnaKnowledgeModel::SoundDnaKnowledgeModel(std::string version, std::string featureSchema,
                                               std::vector<DimensionModel> dimensions,
                                               std::string unavailableReason)
    : m_version(std::move(version))
    , m_featureSchema(std::move(featureSchema))
    , m_dimensions(std::move(dimensions))
    , m_unavailableReason(std::move(unavailableReason))
{
    // A model artifact may contain rejected research candidates, but runtime
    // never exposes them. This makes the evidence gate impossible to bypass in QML.
    std::set<std::string> identities;
    std::erase_if(m_dimensions, [&](const auto& dimension) {
        auto categories = dimension.categories;
        std::sort(categories.begin(), categories.end());
        std::string identity = dimension.id;
        for (const auto& category : categories) identity += "\x1f" + category;
        return !analysisEvidencePasses(dimension.evidence) || !structurePasses(dimension)
            || !identities.insert(std::move(identity)).second;
    });
    if (m_dimensions.empty() && m_unavailableReason.empty())
        m_unavailableReason = "No structurally valid perceptual dimension passed the evidence gate.";
}

SoundDnaKnowledgeModel SoundDnaKnowledgeModel::evidenceGatedDefault()
{
    return {"sound-dna/unvalidated-0", std::string(PatchFeatureExtractor::kSchemaVersion), {},
            "No perceptual dimensions have passed the corpus, listener-reliability, holdout, and transformation gates yet."};
}

const DimensionModel* SoundDnaKnowledgeModel::find(std::string_view id) const noexcept
{
    // Context-free lookup may only return a global model. Returning the first
    // category variant would make ordering silently select an instrument family.
    const auto found = std::find_if(m_dimensions.begin(), m_dimensions.end(), [id](const auto& dimension) {
        return dimension.id == id && dimension.categories.empty();
    });
    return found == m_dimensions.end() ? nullptr : &*found;
}

const DimensionModel* SoundDnaKnowledgeModel::find(std::string_view id, const PatchContext& context) const noexcept
{
    const auto matchesCategory = [](const DimensionModel& dimension, std::string_view category) {
        return std::find(dimension.categories.begin(), dimension.categories.end(), category) != dimension.categories.end();
    };
    if (!context.verifiedCategory.empty()) {
        const auto exact = std::find_if(m_dimensions.begin(), m_dimensions.end(), [&](const auto& dimension) {
            return dimension.id == id && matchesCategory(dimension, context.verifiedCategory);
        });
        if (exact != m_dimensions.end()) return &*exact;
    } else if (!context.categoryCandidates.empty()) {
        const PatchContext::CategoryCandidate* best = nullptr;
        double secondProbability = 0.0;
        for (const auto& candidate : context.categoryCandidates) {
            if (candidate.id.empty() || !std::isfinite(candidate.probability)
                || candidate.probability < 0.0 || candidate.probability > 1.0) continue;
            if (!best || candidate.probability > best->probability
                || (candidate.probability == best->probability && candidate.id < best->id)) {
                if (best) secondProbability = std::max(secondProbability, best->probability);
                best = &candidate;
            } else {
                secondProbability = std::max(secondProbability, candidate.probability);
            }
        }
        // Do not snap a Patch to an instrument-specific distribution on a weak
        // plurality. Ambiguous classifiers use a validated global fallback.
        if (best && best->probability >= 0.60 && best->probability - secondProbability >= 0.15) {
            const auto exact = std::find_if(m_dimensions.begin(), m_dimensions.end(), [&](const auto& dimension) {
                return dimension.id == id && matchesCategory(dimension, best->id);
            });
            if (exact != m_dimensions.end()) return &*exact;
        }
    }
    const auto global = std::find_if(m_dimensions.begin(), m_dimensions.end(), [&](const auto& dimension) {
        return dimension.id == id && dimension.categories.empty();
    });
    return global == m_dimensions.end() ? nullptr : &*global;
}

std::vector<const DimensionModel*> SoundDnaKnowledgeModel::forContext(const PatchContext& context) const
{
    std::vector<const DimensionModel*> selected;
    for (const auto& dimension : m_dimensions) {
        if (std::none_of(selected.begin(), selected.end(), [&](const auto* value) { return value->id == dimension.id; })) {
            if (const auto* chosen = find(dimension.id, context)) selected.push_back(chosen);
        }
    }
    return selected;
}

bool SoundDnaKnowledgeModel::analysisEvidencePasses(const DimensionEvidence& evidence) noexcept
{
    return evidence.independentPatchCount >= kMinimumPatchCount
        && evidence.raterCount >= kMinimumRaterCount
        && evidence.reliability >= kMinimumReliability
        && evidence.reliability <= 1.0
        && evidence.holdoutRankCorrelation >= kMinimumRankCorrelation
        && evidence.holdoutRankCorrelation <= 1.0
        && evidence.holdoutPairwiseAccuracy >= kMinimumPairwiseAccuracy
        && evidence.holdoutPairwiseAccuracy <= 1.0
        && evidence.calibratedIntervalWidth >= 0
        && evidence.calibratedIntervalWidth <= kMaximumIntervalWidth
        && evidence.categoryCoverage >= 0.0 && evidence.categoryCoverage <= 1.0
        && evidence.waveformMetadataQuality >= 0.0 && evidence.waveformMetadataQuality <= 1.0
        && evidence.independentFromOtherDimensions;
}

bool SoundDnaKnowledgeModel::editEvidencePasses(const DimensionEvidence& evidence) noexcept
{
    return analysisEvidencePasses(evidence) && evidence.transformationValidated
        && evidence.transformationDirectionAccuracy >= kMinimumTransformationAccuracy
        && evidence.transformationDirectionAccuracy <= 1.0
        && evidence.medianIdentityPreservation >= kMinimumIdentityPreservation
        && evidence.medianIdentityPreservation <= 5.0;
}

bool SoundDnaKnowledgeModel::structurePasses(const DimensionModel& dimension) noexcept
{
    const auto validCurve = [](const std::vector<CurvePoint>& curve, bool percentile) {
        if (curve.size() < 2) return false;
        bool changes = false;
        for (std::size_t index = 0; index < curve.size(); ++index) {
            const auto& point = curve[index];
            if (!std::isfinite(point.x) || !std::isfinite(point.y)) return false;
            if (percentile && (point.y < 0.0 || point.y > 100.0)) return false;
            if (index > 0) {
                if (curve[index - 1].x >= point.x) return false;
                if (percentile && curve[index - 1].y > point.y) return false;
                changes = changes || curve[index - 1].y != point.y;
            }
        }
        if (percentile && (curve.front().y > 1.0 || curve.back().y < 99.0)) return false;
        return changes;
    };
    if (dimension.id.empty() || dimension.label.empty() || dimension.cohort.empty()
        || !std::isfinite(dimension.latentIntercept) || dimension.terms.empty()
        || !validCurve(dimension.percentileCurve, true)) return false;

    std::set<std::string> featureIds;
    for (const auto& term : dimension.terms) {
        if (term.featureId.empty() || !featureIds.insert(term.featureId).second
            || !validCurve(term.curve, false) || !std::isfinite(term.referenceValue)
            || !std::isfinite(term.supportLow) || !std::isfinite(term.supportHigh)
            || !std::isfinite(term.transformationCost) || term.transformationCost <= 0.0
            || !std::isfinite(term.maximumNormalizedDelta) || term.maximumNormalizedDelta <= 0.0
            || term.maximumNormalizedDelta > 1.0
            || term.supportLow > term.referenceValue || term.referenceValue > term.supportHigh
            || term.supportLow < term.curve.front().x || term.supportHigh > term.curve.back().x) return false;
    }
    std::set<std::pair<std::string, std::string>> interactionPairs;
    for (const auto& interaction : dimension.interactions) {
        auto pair = std::minmax(interaction.firstFeatureId, interaction.secondFeatureId);
        if (interaction.firstFeatureId.empty() || interaction.secondFeatureId.empty()
            || interaction.firstFeatureId == interaction.secondFeatureId
            || !featureIds.contains(interaction.firstFeatureId)
            || !featureIds.contains(interaction.secondFeatureId)
            || !std::isfinite(interaction.weight)
            || !interactionPairs.emplace(pair.first, pair.second).second) return false;
    }
    std::set<std::string> categories;
    for (const auto& category : dimension.categories) {
        if (category.empty() || !categories.insert(category).second) return false;
    }
    return true;
}

} // namespace xp60studio::sounddna
