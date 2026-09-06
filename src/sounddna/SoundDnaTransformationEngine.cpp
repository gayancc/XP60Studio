#include "sounddna/SoundDnaTransformationEngine.h"

#include "xpmodel/generated/Xp60PatchTables.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <sstream>
#include <vector>

namespace xp60studio::sounddna {
namespace {

struct CandidatePolicy
{
    double cost = 1.0;
    double maximumNormalizedDelta = 0.25;
    bool contributesToTarget = false;
};

double objective(const SoundDnaProfile& initial, const SoundDnaProfile& candidate,
                 std::string_view targetId, int targetScore, double editCost,
                 int changedParameters, const SoundDnaChangeRequest& request)
{
    const auto* target = candidate.find(targetId);
    if (!target) return 1.0e9;
    if (changedParameters > std::max(1, request.maximumParameterChanges)) return 1.0e9;
    double cost = std::abs(target->percentile - targetScore) * 8.0 + editCost
        + 0.25 * changedParameters;
    for (const auto& original : initial.values) {
        if (original.id == targetId) continue;
        if (const auto* changed = candidate.find(original.id))
            cost += std::abs(changed->percentile - original.percentile)
                * std::max(0.0, request.collateralWeight);
    }
    return cost;
}

std::pair<double, int> editDistance(const PatchFeatureVector& initial, const PatchFeatureVector& candidate,
                                    const std::map<std::string, CandidatePolicy>& policies)
{
    double cost = 0.0;
    int count = 0;
    for (const auto& original : initial.values) {
        if (!original.editable) continue;
        if (const auto* changed = candidate.find(original.id); changed && changed->raw != original.raw) {
            const int span = std::max(1, original.rawMaximum - original.rawMinimum);
            const auto policy = policies.find(original.id);
            const double weight = policy == policies.end() ? 1.0 : policy->second.cost;
            cost += 5.0 * weight * std::abs(changed->raw - original.raw) / span;
            ++count;
        }
    }
    return {cost, count};
}

std::vector<int> candidateRawValues(const FeatureValue& feature, const FeatureValue& original,
                                    const CandidatePolicy& policy)
{
    const int span = feature.rawMaximum - feature.rawMinimum;
    const int maximumDelta = std::max(1, static_cast<int>(std::floor(span * policy.maximumNormalizedDelta)));
    const int allowedMinimum = std::max(feature.rawMinimum, original.raw - maximumDelta);
    const int allowedMaximum = std::min(feature.rawMaximum, original.raw + maximumDelta);
    std::set<int> values{allowedMinimum, allowedMaximum};
    for (int part = 1; part < 16; ++part)
        values.insert(allowedMinimum + static_cast<int>(std::lround((allowedMaximum - allowedMinimum) * part / 16.0)));
    for (const int delta : {1, 2, 4, 8, 16, 32}) {
        values.insert(std::clamp(feature.raw - delta, allowedMinimum, allowedMaximum));
        values.insert(std::clamp(feature.raw + delta, allowedMinimum, allowedMaximum));
    }
    values.erase(feature.raw);
    return {values.begin(), values.end()};
}

bool setFeatureRaw(xpmodel::Xp60Patch& patch, const FeatureValue& feature, int raw)
{
    if (feature.toneNumber == 0) return patch.common().setRaw(feature.id, raw);
    const auto tone = xpmodel::ToneIndex::fromNumber(feature.toneNumber);
    if (!tone) return false;
    const std::string prefix = "tone." + std::to_string(feature.toneNumber) + ".";
    if (!feature.id.starts_with(prefix)) return false;
    return patch.tone(*tone).setRaw("tone." + feature.id.substr(prefix.size()), raw);
}

std::string descriptorName(const FeatureValue& feature)
{
    if (feature.toneNumber == 0) {
        if (const auto* descriptor = xpmodel::xp60tables::patchCommonTable().find(feature.id))
            return std::string(descriptor->name);
        return feature.id;
    }
    const std::string prefix = "tone." + std::to_string(feature.toneNumber) + ".";
    const std::string id = "tone." + feature.id.substr(prefix.size());
    if (const auto* descriptor = xpmodel::xp60tables::patchToneTable().find(id)) return std::string(descriptor->name);
    return feature.id;
}

} // namespace

SoundDnaTransformationResult SoundDnaTransformationEngine::transform(const xpmodel::Xp60Patch& patch,
                                                                      const PatchContext& context,
                                                                      const SoundDnaChangeRequest& request) const
{
    SoundDnaTransformationResult result;
    const auto initialFeatures = m_extractor.extract(patch, context);
    result.before = m_analyzer.analyze(initialFeatures);
    const auto* model = m_model.find(request.dimensionId, context);
    const auto* initialTarget = result.before.find(request.dimensionId);
    if (!model || !initialTarget) {
        result.limitation = "That Sound DNA dimension is not available in the validated knowledge model.";
        return result;
    }
    if (!initialTarget->editable) {
        result.limitation = "This dimension can be analyzed, but its transformation has not passed listening validation.";
        return result;
    }
    if (request.maximumParameterChanges <= 0 || request.maximumCandidateEvaluations <= 0) {
        result.limitation = "The change request permits no parameter changes or optimization work.";
        return result;
    }

    const int requested = std::clamp(request.targetScore, 0, 100);
    if (requested == initialTarget->score) {
        result.patch = patch;
        result.after = result.before;
        result.reachedTarget = true;
        result.explanation = model->label + " is already at the requested percentile.";
        return result;
    }
    std::map<std::string, CandidatePolicy> candidates;
    // Only controlled-listening-validated terms are eligible. Correlated
    // analysis terms remain read-only. Other editable dimensions may offer
    // validated compensation parameters to reduce collateral movement.
    for (const auto* dimension : m_model.forContext(context)) {
        if (dimension != model && !SoundDnaKnowledgeModel::editEvidencePasses(dimension->evidence)) continue;
        for (const auto& term : dimension->terms) {
            if (!term.transformable) continue;
            const auto [position, inserted] = candidates.try_emplace(
                term.featureId, CandidatePolicy{term.transformationCost, term.maximumNormalizedDelta,
                                                dimension == model});
            if (!inserted) {
                // A parameter used by several validated transformations keeps
                // the most conservative combined safety envelope.
                position->second.cost = std::max(position->second.cost, term.transformationCost);
                position->second.maximumNormalizedDelta = std::min(
                    position->second.maximumNormalizedDelta, term.maximumNormalizedDelta);
                position->second.contributesToTarget = position->second.contributesToTarget || dimension == model;
            }
        }
    }

    std::vector<std::string> candidateOrder;
    candidateOrder.reserve(candidates.size());
    for (const auto& [id, policy] : candidates) candidateOrder.push_back(id);
    std::stable_sort(candidateOrder.begin(), candidateOrder.end(), [&candidates](const auto& left, const auto& right) {
        const auto& leftPolicy = candidates.at(left);
        const auto& rightPolicy = candidates.at(right);
        if (leftPolicy.contributesToTarget != rightPolicy.contributesToTarget)
            return leftPolicy.contributesToTarget > rightPolicy.contributesToTarget;
        if (leftPolicy.cost != rightPolicy.cost) return leftPolicy.cost < rightPolicy.cost;
        return left < right;
    });

    xpmodel::Xp60Patch best = patch;
    SoundDnaProfile bestProfile = result.before;
    double bestEditCost = 0.0;
    int bestChanged = 0;
    double bestObjective = objective(result.before, bestProfile, request.dimensionId, requested,
                                     bestEditCost, bestChanged, request);

    // Deterministic bounded coordinate search. A multi-resolution candidate
    // grid crosses score plateaus without scanning every raw value on every
    // pointer event. Stable feature/raw ordering breaks exact ties.
    bool budgetExhausted = false;
    for (int iteration = 0; iteration < 64; ++iteration) {
        const auto currentFeatures = m_extractor.extract(best, context);
        bool improved = false;
        xpmodel::Xp60Patch iterationBest = best;
        SoundDnaProfile iterationProfile = bestProfile;
        double iterationEditCost = bestEditCost;
        int iterationChanged = bestChanged;
        double iterationObjective = bestObjective;

        for (const auto& id : candidateOrder) {
            if (result.candidateEvaluations >= request.maximumCandidateEvaluations) {
                budgetExhausted = true;
                break;
            }
            const auto& policy = candidates.at(id);
            const auto* feature = currentFeatures.find(id);
            const auto* original = initialFeatures.find(id);
            if (!feature || !original || !feature->editable) continue;
            for (const int raw : candidateRawValues(*feature, *original, policy)) {
                if (result.candidateEvaluations >= request.maximumCandidateEvaluations) {
                    budgetExhausted = true;
                    break;
                }
                auto candidatePatch = best;
                if (!setFeatureRaw(candidatePatch, *feature, raw)) continue;
                const auto candidateFeatures = m_extractor.extract(candidatePatch, context);
                const auto profile = m_analyzer.analyze(candidateFeatures);
                ++result.candidateEvaluations;
                const auto [editCost, changedCount] = editDistance(initialFeatures, candidateFeatures, candidates);
                const double score = objective(result.before, profile, request.dimensionId, requested,
                                               editCost, changedCount, request);
                if (score + 1.0e-9 < iterationObjective) {
                    improved = true;
                    iterationBest = std::move(candidatePatch);
                    iterationProfile = profile;
                    iterationEditCost = editCost;
                    iterationChanged = changedCount;
                    iterationObjective = score;
                }
            }
        }
        if (!improved) break;
        best = std::move(iterationBest);
        bestProfile = std::move(iterationProfile);
        bestEditCost = iterationEditCost;
        bestChanged = iterationChanged;
        bestObjective = iterationObjective;
        const auto* achieved = bestProfile.find(request.dimensionId);
        if (achieved && std::abs(achieved->percentile - requested) <= 0.5) break;
        if (budgetExhausted) break;
    }

    const auto finalFeatures = m_extractor.extract(best, context);
    for (const auto& before : initialFeatures.values) {
        const auto* after = finalFeatures.find(before.id);
        if (!after || before.raw == after->raw || before.kind == FeatureKind::Derived) continue;
        result.parameterChanges.push_back({before.id, descriptorName(before), before.toneNumber, before.raw, after->raw});
    }
    result.after = std::move(bestProfile);
    for (const auto& before : result.before.values) {
        if (before.id == request.dimensionId) continue;
        const auto* after = result.after.find(before.id);
        if (after && after->score != before.score)
            result.secondaryEffects.push_back({before.id, before.label, before.score, after->score});
    }

    const auto* achieved = result.after.find(request.dimensionId);
    result.reachedTarget = achieved && std::abs(achieved->percentile - requested) <= 2.0;
    if (result.parameterChanges.empty()) {
        result.limitation = "No safe, evidence-backed parameter movement improved the requested target.";
        return result;
    }
    result.patch = best;
    std::ostringstream explanation;
    explanation << model->label << " moved from " << initialTarget->score << " to "
                << (achieved ? achieved->score : initialTarget->score) << " using "
                << result.parameterChanges.size() << " validated parameter change";
    if (result.parameterChanges.size() != 1) explanation << 's';
    explanation << ".";
    result.explanation = explanation.str();
    if (!result.reachedTarget)
        result.limitation = budgetExhausted
            ? "The real-time optimization budget was reached; the nearest safe intermediate result was used."
            : "The requested percentile was limited to the nearest safe result by patch and evidence constraints.";
    return result;
}

} // namespace xp60studio::sounddna
