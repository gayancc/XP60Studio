#pragma once

#include "sounddna/SoundDnaTypes.h"

#include <string>
#include <vector>

namespace xp60studio::sounddna {

struct CurvePoint { double x = 0.0; double y = 0.0; };
struct FeatureTerm
{
    std::string featureId;
    std::vector<CurvePoint> curve;
    double referenceValue = 0.5;
    double supportLow = 0.0;
    double supportHigh = 1.0;
    // Statistical association never grants mutation authority. This becomes
    // true only after controlled before/after listening validation.
    bool transformable = false;
    double transformationCost = 1.0;
    double maximumNormalizedDelta = 0.25;
};
struct InteractionTerm { std::string firstFeatureId; std::string secondFeatureId; double weight = 0.0; };

struct DimensionEvidence
{
    int independentPatchCount = 0;
    int raterCount = 0;
    double reliability = 0.0;
    double holdoutRankCorrelation = 0.0;
    double holdoutPairwiseAccuracy = 0.0;
    int calibratedIntervalWidth = 100;
    bool independentFromOtherDimensions = false;
    bool transformationValidated = false;
    double transformationDirectionAccuracy = 0.0;
    double medianIdentityPreservation = 0.0;
    double categoryCoverage = 0.0;
    double waveformMetadataQuality = 0.0;
};

struct DimensionModel
{
    std::string id;
    std::string label;
    std::string cohort;
    std::string referenceTemplate;
    double latentIntercept = 0.0;
    std::vector<FeatureTerm> terms;
    std::vector<InteractionTerm> interactions;
    // Empirical, cohort-conditioned latent value -> percentile calibration.
    // Runtime never interprets an arbitrary linear raw scale as a percentile.
    std::vector<CurvePoint> percentileCurve;
    DimensionEvidence evidence;
    bool usesWaveformMetadata = false;
    // Empty = global fallback. Otherwise this is a category-conditioned model
    // for exactly the named verified/coarse categories.
    std::vector<std::string> categories;
};

class SoundDnaKnowledgeModel
{
public:
    static constexpr int kMinimumPatchCount = 30;
    static constexpr int kMinimumRaterCount = 5;
    static constexpr double kMinimumReliability = 0.70;
    static constexpr double kMinimumRankCorrelation = 0.65;
    static constexpr double kMinimumPairwiseAccuracy = 0.70;
    static constexpr int kMaximumIntervalWidth = 20;
    static constexpr double kMinimumTransformationAccuracy = 0.75;
    static constexpr double kMinimumIdentityPreservation = 4.0;

    SoundDnaKnowledgeModel(std::string version, std::string featureSchema,
                           std::vector<DimensionModel> dimensions,
                           std::string unavailableReason = {});

    [[nodiscard]] static SoundDnaKnowledgeModel evidenceGatedDefault();
    [[nodiscard]] const std::string& version() const noexcept { return m_version; }
    [[nodiscard]] const std::string& featureSchema() const noexcept { return m_featureSchema; }
    [[nodiscard]] const std::string& unavailableReason() const noexcept { return m_unavailableReason; }
    [[nodiscard]] const std::vector<DimensionModel>& dimensions() const noexcept { return m_dimensions; }
    [[nodiscard]] const DimensionModel* find(std::string_view id) const noexcept;
    [[nodiscard]] const DimensionModel* find(std::string_view id, const PatchContext& context) const noexcept;
    [[nodiscard]] std::vector<const DimensionModel*> forContext(const PatchContext& context) const;
    [[nodiscard]] static bool analysisEvidencePasses(const DimensionEvidence& evidence) noexcept;
    [[nodiscard]] static bool editEvidencePasses(const DimensionEvidence& evidence) noexcept;
    [[nodiscard]] static bool structurePasses(const DimensionModel& dimension) noexcept;

private:
    std::string m_version;
    std::string m_featureSchema;
    std::vector<DimensionModel> m_dimensions;
    std::string m_unavailableReason;
};

} // namespace xp60studio::sounddna
