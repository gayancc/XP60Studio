#pragma once

#include "xpmodel/Xp60Patch.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace xp60studio::sounddna {

enum class FeatureKind { Continuous, Ordinal, Categorical, Derived };
enum class Confidence { Low, Medium, High };

struct PatchContext
{
    // Empty means unknown. A user-verified category is never inferred from a
    // Patch name; offline classifiers may supply weighted candidates instead.
    std::string verifiedCategory;
    struct CategoryCandidate { std::string id; double probability = 0.0; };
    std::vector<CategoryCandidate> categoryCandidates;
};

struct FeatureValue
{
    std::string id;
    FeatureKind kind = FeatureKind::Continuous;
    double value = 0.0;       // normalized for numeric features; raw for categorical
    int raw = 0;
    int rawMinimum = 0;
    int rawMaximum = 0;
    int toneNumber = 0;       // 0 = Patch common/derived, 1..4 = Tone
    bool editable = false;    // safe continuous parameter, not permission by itself
    bool active = true;       // false = stored data from a disabled Tone
    std::string categoricalValue; // stable identity/label; never numerically ordered
};

struct PatchFeatureVector
{
    std::string schemaVersion;
    std::vector<FeatureValue> values;
    PatchContext context;

    [[nodiscard]] const FeatureValue* find(std::string_view id) const noexcept;
};

struct ToneContribution
{
    int toneNumber = 0;
    double signedContribution = 0.0;
    double magnitudePercent = 0.0;
};

struct SoundDnaValue
{
    std::string id;
    std::string label;
    int score = 0;                       // cohort percentile, never an XP raw value
    double percentile = 0.0;             // unrounded value used by optimization
    double latentScore = 0.0;
    Confidence confidence = Confidence::Low;
    int intervalLow = 0;
    int intervalHigh = 100;
    std::string cohort;
    std::string referenceText;
    std::string confidenceReason;
    double inDistributionSupport = 1.0;
    bool editable = false;
    std::vector<ToneContribution> toneContributions;
    double patchWideContributionPercent = 0.0;
};

struct SoundDnaProfile
{
    std::string modelVersion;
    std::vector<SoundDnaValue> values;
    std::string unavailableReason;

    [[nodiscard]] bool available() const noexcept { return !values.empty(); }
    [[nodiscard]] const SoundDnaValue* find(std::string_view id) const noexcept;
};

struct ParameterChange
{
    std::string parameterId;
    std::string parameterName;
    int toneNumber = 0;
    int beforeRaw = 0;
    int afterRaw = 0;
};

struct SecondaryEffect
{
    std::string dimensionId;
    std::string label;
    int beforeScore = 0;
    int afterScore = 0;
};

struct SoundDnaChangeRequest
{
    std::string dimensionId;
    int targetScore = 50;
    int maximumParameterChanges = 12;
    int maximumCandidateEvaluations = 1024;
    double collateralWeight = 2.0;
};

struct SoundDnaTransformationResult
{
    std::optional<xpmodel::Xp60Patch> patch;
    SoundDnaProfile before;
    SoundDnaProfile after;
    std::vector<ParameterChange> parameterChanges;
    std::vector<SecondaryEffect> secondaryEffects;
    std::string explanation;
    std::string limitation;
    bool reachedTarget = false;
    int candidateEvaluations = 0;
};

} // namespace xp60studio::sounddna
