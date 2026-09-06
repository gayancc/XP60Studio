#include "sounddna/SoundDnaAnalyzer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <sstream>

namespace xp60studio::sounddna {
namespace {

double evaluateCurve(const std::vector<CurvePoint>& curve, double x)
{
    if (curve.empty()) return 0.0;
    if (x <= curve.front().x) return curve.front().y;
    if (x >= curve.back().x) return curve.back().y;
    const auto upper = std::upper_bound(curve.begin(), curve.end(), x,
                                        [](double value, const auto& point) { return value < point.x; });
    const auto lower = upper - 1;
    const double span = upper->x - lower->x;
    return span <= 0.0 ? lower->y : lower->y + (upper->y - lower->y) * (x - lower->x) / span;
}

Confidence confidenceFor(const DimensionModel& dimension, double support, double categoryCertainty)
{
    const auto& evidence = dimension.evidence;
    Confidence confidence = Confidence::Low;
    if (evidence.independentPatchCount >= 100 && evidence.reliability >= 0.80
        && evidence.holdoutRankCorrelation >= 0.75 && evidence.calibratedIntervalWidth <= 10
        && evidence.categoryCoverage >= 0.75
        && (!dimension.usesWaveformMetadata || evidence.waveformMetadataQuality >= 0.75))
        confidence = Confidence::High;
    else if (evidence.independentPatchCount >= 50 && evidence.calibratedIntervalWidth <= 15)
        confidence = Confidence::Medium;
    if ((support < 0.80 || categoryCertainty < 0.75) && confidence == Confidence::High)
        confidence = Confidence::Medium;
    if (support < 0.50 || categoryCertainty < 0.50) confidence = Confidence::Low;
    return confidence;
}

int toneForFeature(const PatchFeatureVector& features, std::string_view id)
{
    const auto* feature = features.find(id);
    return feature ? feature->toneNumber : 0;
}

const FeatureTerm* termForFeature(const DimensionModel& dimension, std::string_view id)
{
    const auto found = std::find_if(dimension.terms.begin(), dimension.terms.end(),
                                    [id](const auto& term) { return term.featureId == id; });
    return found == dimension.terms.end() ? nullptr : &*found;
}

double categoryCertainty(const DimensionModel& dimension, const PatchContext& context)
{
    if (dimension.categories.empty() || !context.verifiedCategory.empty()) return 1.0;
    double certainty = 0.0;
    for (const auto& candidate : context.categoryCandidates) {
        if (std::find(dimension.categories.begin(), dimension.categories.end(), candidate.id)
            != dimension.categories.end()) certainty = std::max(certainty, candidate.probability);
    }
    return std::clamp(certainty, 0.0, 1.0);
}

const char* ordinalSuffix(int value)
{
    const int lastTwo = value % 100;
    if (lastTwo >= 11 && lastTwo <= 13) return "th";
    switch (value % 10) {
    case 1: return "st";
    case 2: return "nd";
    case 3: return "rd";
    default: return "th";
    }
}

} // namespace

SoundDnaProfile SoundDnaAnalyzer::analyze(const PatchFeatureVector& features) const
{
    SoundDnaProfile profile;
    profile.modelVersion = m_model.version();
    if (features.schemaVersion != m_model.featureSchema()) {
        profile.unavailableReason = "The Sound DNA model uses a different feature schema.";
        return profile;
    }
    profile.unavailableReason = m_model.unavailableReason();

    for (const auto* dimensionPtr : m_model.forContext(features.context)) {
        const auto& dimension = *dimensionPtr;
        double latent = dimension.latentIntercept;
        std::array<double, 4> perTone{};
        double patchWide = 0.0;
        int supported = 0;
        int relevant = 0;
        bool complete = true;
        for (const auto& term : dimension.terms) {
            const auto* feature = features.find(term.featureId);
            if (!feature || feature->kind == FeatureKind::Categorical) {
                complete = false;
                break;
            }
            // Disabled Tone bytes are retained in the feature record for
            // provenance, but are acoustically inactive and therefore neutral.
            ++relevant;
            if (feature->toneNumber > 0 && !feature->active) continue;
            const bool inside = feature->value >= term.supportLow && feature->value <= term.supportHigh;
            if (inside) ++supported;
            const double contribution = evaluateCurve(term.curve, feature->value)
                - evaluateCurve(term.curve, term.referenceValue);
            latent += contribution;
            if (feature->toneNumber >= 1 && feature->toneNumber <= 4)
                perTone[static_cast<std::size_t>(feature->toneNumber - 1)] += contribution;
            else
                patchWide += contribution;
        }
        if (!complete) continue;
        for (const auto& interaction : dimension.interactions) {
            const auto* first = features.find(interaction.firstFeatureId);
            const auto* second = features.find(interaction.secondFeatureId);
            const auto* firstTerm = termForFeature(dimension, interaction.firstFeatureId);
            const auto* secondTerm = termForFeature(dimension, interaction.secondFeatureId);
            if (!first || !second || !firstTerm || !secondTerm) {
                complete = false;
                break;
            }
            if ((first->toneNumber > 0 && !first->active) || (second->toneNumber > 0 && !second->active)) continue;
            const double contribution = interaction.weight
                * (first->value - firstTerm->referenceValue)
                * (second->value - secondTerm->referenceValue);
            latent += contribution;
            const int firstTone = toneForFeature(features, interaction.firstFeatureId);
            const int secondTone = toneForFeature(features, interaction.secondFeatureId);
            if (firstTone == secondTone && firstTone >= 1 && firstTone <= 4) {
                perTone[static_cast<std::size_t>(firstTone - 1)] += contribution;
            } else if (firstTone >= 1 && firstTone <= 4 && secondTone >= 1 && secondTone <= 4) {
                perTone[static_cast<std::size_t>(firstTone - 1)] += contribution / 2.0;
                perTone[static_cast<std::size_t>(secondTone - 1)] += contribution / 2.0;
            } else if (firstTone >= 1 && firstTone <= 4) {
                perTone[static_cast<std::size_t>(firstTone - 1)] += contribution / 2.0;
                patchWide += contribution / 2.0;
            } else if (secondTone >= 1 && secondTone <= 4) {
                perTone[static_cast<std::size_t>(secondTone - 1)] += contribution / 2.0;
                patchWide += contribution / 2.0;
            } else {
                patchWide += contribution;
            }
        }
        if (!complete) continue;

        const double percentile = std::clamp(evaluateCurve(dimension.percentileCurve, latent), 0.0, 100.0);
        const int score = std::clamp(static_cast<int>(std::lround(percentile)), 0, 100);
        const double support = relevant > 0 ? static_cast<double>(supported) / relevant : 1.0;
        const double certainty = categoryCertainty(dimension, features.context);
        const int widenedWidth = std::min(100, static_cast<int>(std::ceil(
            dimension.evidence.calibratedIntervalWidth * (1.0 + (1.0 - support) + (1.0 - certainty)))));
        const int halfWidth = (widenedWidth + 1) / 2;
        SoundDnaValue value;
        value.id = dimension.id;
        value.label = dimension.label;
        value.score = score;
        value.percentile = percentile;
        value.latentScore = latent;
        value.confidence = confidenceFor(dimension, support, certainty);
        value.intervalLow = std::max(0, score - halfWidth);
        value.intervalHigh = std::min(100, score + halfWidth);
        value.cohort = dimension.cohort;
        std::ostringstream reference;
        reference << "Approximately the " << score << ordinalSuffix(score)
                  << " percentile among " << dimension.cohort << ".";
        if (!dimension.referenceTemplate.empty()) reference << ' ' << dimension.referenceTemplate;
        value.referenceText = reference.str();
        value.inDistributionSupport = support;
        value.confidenceReason = support < 0.8 ? "Patch features fall partly outside the calibrated cohort or modeled Tones are inactive."
            : certainty < 0.75 ? "Patch category is uncertain."
            : dimension.evidence.categoryCoverage < 0.75 ? "The evidence has limited category coverage."
            : dimension.usesWaveformMetadata && dimension.evidence.waveformMetadataQuality < 0.75
                ? "Waveform characterization quality limits confidence."
                : "Supported by the selected cohort and calibrated feature range.";
        value.editable = SoundDnaKnowledgeModel::editEvidencePasses(dimension.evidence);
        const double magnitude = std::abs(patchWide) + std::accumulate(
            perTone.begin(), perTone.end(), 0.0,
            [](double sum, double item) { return sum + std::abs(item); });
        for (int tone = 1; tone <= 4; ++tone) {
            const double signedValue = perTone[static_cast<std::size_t>(tone - 1)];
            value.toneContributions.push_back({tone, signedValue,
                                                magnitude > 0.0 ? 100.0 * std::abs(signedValue) / magnitude : 0.0});
        }
        value.patchWideContributionPercent = magnitude > 0.0 ? 100.0 * std::abs(patchWide) / magnitude : 0.0;
        profile.values.push_back(std::move(value));
    }
    if (!profile.values.empty()) profile.unavailableReason.clear();
    else if (!m_model.dimensions().empty())
        profile.unavailableReason = "The validated Sound DNA model is incompatible with this Patch feature record.";
    return profile;
}

} // namespace xp60studio::sounddna
