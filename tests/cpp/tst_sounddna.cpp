#include "support/FakeXp60.h"

#include "sounddna/PatchFeatureExtractor.h"
#include "sounddna/SoundDnaAnalyzer.h"
#include "sounddna/SoundDnaTransformationEngine.h"

#include <QtTest>

using namespace xp60studio;

namespace {

sounddna::DimensionEvidence passingEvidence(bool transformation = true)
{
    return {128, 6, 0.84, 0.76, 0.78, 10, true, transformation, 0.82, 4.4, 0.90, 0.80};
}

sounddna::DimensionModel bodyModel()
{
    return {"body", "Body", "comparable test patches", "Upper range of comparable patches",
            0.0,
            {{"tone.1.tone_level", {{0.0, -1.0}, {1.0, 1.0}}, 0.5, 0.0, 1.0, true, 1.0, 1.0},
             {"tone.2.tone_level", {{0.0, -0.5}, {1.0, 0.5}}, 0.5, 0.0, 1.0, true, 1.0, 1.0},
             {"common.patch_level", {{0.0, -0.25}, {1.0, 0.25}}, 0.5, 0.0, 1.0, false, 1.0, 1.0}},
            {{"tone.1.tone_level", "tone.2.tone_level", 0.25}},
            {{-2.0, 0.0}, {0.0, 50.0}, {2.0, 100.0}}, passingEvidence(), false, {}};
}

xpmodel::Xp60Patch fixturePatch()
{
    const auto image = testsupport::fixtureImage();
    const auto address = xpmodel::Xp60PatchLayout::userPatchAddress(1);
    if (!address) throw std::runtime_error("missing fixture address");
    return testsupport::patchFrom(image, *address);
}

} // namespace

class SoundDnaTest : public QObject
{
    Q_OBJECT

private slots:
    void extractorRetainsEveryDocumentedParameterAndRelations()
    {
        const auto patch = fixturePatch();
        const auto features = sounddna::PatchFeatureExtractor{}.extract(patch);
        QCOMPARE(features.schemaVersion, std::string(sounddna::PatchFeatureExtractor::kSchemaVersion));
        QCOMPARE(features.values.size(), std::size_t(72 + 4 * 128 + 4 + 4));
        QVERIFY(features.find("common.patch_level"));
        QVERIFY(features.find("tone.4.cutoff_frequency"));
        QVERIFY(features.find("patch.active_tone_count"));
        QVERIFY(!features.find("tone.1.wave_identity")->categoricalValue.empty());
        QCOMPARE(features.find("tone.1.wave_number")->kind, sounddna::FeatureKind::Categorical);
        QVERIFY(!features.find("tone.1.wave_number")->editable);
        QVERIFY(!features.find("common.efx_parameter_1")->editable);
        QVERIFY(features.find("tone.1.tone_level")->editable == patch.toneEnabled(xpmodel::ToneIndex::tone1()));
        QCOMPARE(features.find("tone.1.tone_level")->active, patch.toneEnabled(xpmodel::ToneIndex::tone1()));
    }

    void evidenceGateCannotBeBypassedByTheUi()
    {
        auto rejected = bodyModel();
        rejected.evidence.reliability = 0.69;
        sounddna::SoundDnaKnowledgeModel model("test/1", std::string(sounddna::PatchFeatureExtractor::kSchemaVersion),
                                               {bodyModel(), rejected});
        QCOMPARE(model.dimensions().size(), std::size_t(1));
        QVERIFY(model.find("body"));

        const auto empty = sounddna::SoundDnaKnowledgeModel::evidenceGatedDefault();
        const auto profile = sounddna::SoundDnaAnalyzer(empty).analyze(sounddna::PatchFeatureExtractor{}.extract(fixturePatch()));
        QVERIFY(!profile.available());
        QVERIFY(!profile.unavailableReason.empty());
    }

    void analyzerProducesCohortScoresIntervalsAndToneAttribution()
    {
        sounddna::SoundDnaKnowledgeModel model("test/1", std::string(sounddna::PatchFeatureExtractor::kSchemaVersion),
                                               {bodyModel()});
        const auto profile = sounddna::SoundDnaAnalyzer(model).analyze(sounddna::PatchFeatureExtractor{}.extract(fixturePatch()));
        QVERIFY(profile.available());
        const auto* body = profile.find("body");
        QVERIFY(body);
        QVERIFY(body->score >= 0 && body->score <= 100);
        QCOMPARE(body->intervalHigh - body->intervalLow, 10);
        QVERIFY(body->editable);
        QCOMPARE(body->toneContributions.size(), std::size_t(4));
        double share = 0.0;
        for (const auto& tone : body->toneContributions) share += tone.magnitudePercent;
        QVERIFY(body->patchWideContributionPercent > 0.0);
        QVERIFY(std::abs(share + body->patchWideContributionPercent - 100.0) < 0.01);
    }

    void disabledToneBytesAreRetainedButCannotAffectAnalysis()
    {
        sounddna::SoundDnaKnowledgeModel model("test/1", std::string(sounddna::PatchFeatureExtractor::kSchemaVersion),
                                               {bodyModel()});
        auto first = fixturePatch();
        QVERIFY(first.setRaw(xpmodel::ToneIndex::tone2(), xpmodel::ToneParameter::ToneSwitch, 0));
        auto second = first;
        QVERIFY(second.setRaw(xpmodel::ToneIndex::tone2(), xpmodel::ToneParameter::ToneLevel, 0));
        const sounddna::PatchFeatureExtractor extractor;
        const auto firstFeatures = extractor.extract(first);
        const auto secondFeatures = extractor.extract(second);
        QVERIFY(!firstFeatures.find("tone.2.tone_level")->active);
        QCOMPARE(firstFeatures.find("tone.2.tone_level")->raw, 127);
        const sounddna::SoundDnaAnalyzer analyzer(model);
        QCOMPARE(analyzer.analyze(firstFeatures).find("body")->score,
                 analyzer.analyze(secondFeatures).find("body")->score);
    }

    void malformedOrIncompleteModelsNeverProduceScores()
    {
        auto constant = bodyModel();
        constant.percentileCurve = {{0.0, 50.0}, {1.0, 50.0}};
        sounddna::SoundDnaKnowledgeModel rejected("test/1", std::string(sounddna::PatchFeatureExtractor::kSchemaVersion),
                                                  {constant});
        QVERIFY(rejected.dimensions().empty());

        auto missing = bodyModel();
        missing.terms.front().featureId = "tone.1.not_a_real_parameter";
        missing.interactions.front().firstFeatureId = "tone.1.not_a_real_parameter";
        sounddna::SoundDnaKnowledgeModel acceptedStructure(
            "test/1", std::string(sounddna::PatchFeatureExtractor::kSchemaVersion), {missing});
        const auto profile = sounddna::SoundDnaAnalyzer(acceptedStructure).analyze(
            sounddna::PatchFeatureExtractor{}.extract(fixturePatch()));
        QVERIFY(!profile.available());
        QVERIFY(profile.unavailableReason.find("incompatible") != std::string::npos);
    }

    void uncertainCategoryPredictionsUseTheGlobalFallback()
    {
        auto strings = bodyModel();
        strings.categories = {"strings"};
        strings.cohort = "verified string patches";
        strings.latentIntercept = -0.5;
        const auto global = bodyModel();
        sounddna::SoundDnaKnowledgeModel model("test/1", std::string(sounddna::PatchFeatureExtractor::kSchemaVersion),
                                               {strings, global});
        sounddna::PatchContext uncertain;
        uncertain.categoryCandidates = {{"strings", 0.45}, {"brass", 0.40}};
        QCOMPARE(model.find("body", uncertain)->cohort, global.cohort);
        sounddna::PatchContext confident;
        confident.categoryCandidates = {{"strings", 0.80}, {"brass", 0.10}};
        QCOMPARE(model.find("body", confident)->cohort, strings.cohort);
    }

    void outOfDistributionFeaturesReduceConfidenceAndWidenTheInterval()
    {
        auto dimension = bodyModel();
        dimension.terms.front().supportHigh = 0.75;
        sounddna::SoundDnaKnowledgeModel model("test/1", std::string(sounddna::PatchFeatureExtractor::kSchemaVersion),
                                               {dimension});
        const auto value = *sounddna::SoundDnaAnalyzer(model).analyze(
            sounddna::PatchFeatureExtractor{}.extract(fixturePatch())).find("body");
        QCOMPARE(value.confidence, sounddna::Confidence::Medium);
        QVERIFY(value.intervalHigh - value.intervalLow > 10);
        QVERIFY(std::abs(value.inDistributionSupport - (2.0 / 3.0)) < 0.000001);
    }

    void transformationMovesTargetWithAValidMinimalDeterministicPatch()
    {
        sounddna::SoundDnaKnowledgeModel model("test/1", std::string(sounddna::PatchFeatureExtractor::kSchemaVersion),
                                               {bodyModel()});
        const auto patch = fixturePatch();
        sounddna::SoundDnaTransformationEngine engine(model);
        const auto before = sounddna::SoundDnaAnalyzer(model).analyze(sounddna::PatchFeatureExtractor{}.extract(patch));
        const int initialScore = before.find("body")->score;
        // The fixture's first two Tone levels are both already 127, so choose
        // whichever direction has headroom instead of assuming "more" is a
        // legal transformation for every real Patch.
        const int target = initialScore > 50 ? std::max(0, initialScore - 8)
                                             : std::min(100, initialScore + 8);
        const auto first = engine.transform(patch, {}, {"body", target});
        const auto second = engine.transform(patch, {}, {"body", target});
        QVERIFY(first.patch.has_value());
        QVERIFY(second.patch.has_value());
        QVERIFY(std::abs(first.after.find("body")->score - target)
                < std::abs(initialScore - target));
        QCOMPARE(*first.patch, *second.patch);
        QVERIFY(!first.parameterChanges.empty());
        QVERIFY(first.candidateEvaluations > 0);
        QVERIFY(first.candidateEvaluations <= 1024);
        for (const auto& change : first.parameterChanges) {
            QVERIFY(change.parameterId == "tone.1.tone_level" || change.parameterId == "tone.2.tone_level");
        }
        // The transformation is a Patch-model edit only. Protected architecture
        // and waveform identity survive byte-for-byte at their typed values.
        for (const auto tone : xpmodel::ToneIndex::all()) {
            QCOMPARE(first.patch->raw(tone, xpmodel::ToneParameter::WaveNumber),
                     patch.raw(tone, xpmodel::ToneParameter::WaveNumber));
            QCOMPARE(first.patch->raw(tone, xpmodel::ToneParameter::ToneSwitch),
                     patch.raw(tone, xpmodel::ToneParameter::ToneSwitch));
        }
    }

    void requestingTheCurrentScoreIsAStableNoOp()
    {
        sounddna::SoundDnaKnowledgeModel model("test/1", std::string(sounddna::PatchFeatureExtractor::kSchemaVersion),
                                               {bodyModel()});
        const auto patch = fixturePatch();
        const auto profile = sounddna::SoundDnaAnalyzer(model).analyze(
            sounddna::PatchFeatureExtractor{}.extract(patch));
        const auto result = sounddna::SoundDnaTransformationEngine(model).transform(
            patch, {}, {"body", profile.find("body")->score});
        QVERIFY(result.patch.has_value());
        QCOMPARE(*result.patch, patch);
        QVERIFY(result.parameterChanges.empty());
        QVERIFY(result.reachedTarget);
    }

    void correlationAloneNeverAuthorizesAParameterEdit()
    {
        auto dimension = bodyModel();
        for (auto& term : dimension.terms) term.transformable = false;
        sounddna::SoundDnaKnowledgeModel model("test/1", std::string(sounddna::PatchFeatureExtractor::kSchemaVersion),
                                               {dimension});
        const auto result = sounddna::SoundDnaTransformationEngine(model).transform(
            fixturePatch(), {}, {"body", 20});
        QVERIFY(!result.patch);
        QVERIFY(result.parameterChanges.empty());
    }

    void transformationRespectsPerParameterDeltaAndRequestSizeLimits()
    {
        auto dimension = bodyModel();
        for (auto& term : dimension.terms) term.maximumNormalizedDelta = 0.05;
        sounddna::SoundDnaKnowledgeModel model("test/1", std::string(sounddna::PatchFeatureExtractor::kSchemaVersion),
                                               {dimension});
        auto request = sounddna::SoundDnaChangeRequest{"body", 0};
        request.maximumParameterChanges = 1;
        const auto patch = fixturePatch();
        const auto result = sounddna::SoundDnaTransformationEngine(model).transform(patch, {}, request);
        QVERIFY(result.patch);
        QVERIFY(result.parameterChanges.size() <= 1);
        for (const auto& change : result.parameterChanges) {
            QVERIFY(std::abs(change.afterRaw - change.beforeRaw) <= 6);
        }
        QVERIFY(!result.reachedTarget);
    }

    void transformationHonorsTheRealTimeEvaluationBudget()
    {
        sounddna::SoundDnaKnowledgeModel model("test/1", std::string(sounddna::PatchFeatureExtractor::kSchemaVersion),
                                               {bodyModel()});
        auto request = sounddna::SoundDnaChangeRequest{"body", 0};
        request.maximumCandidateEvaluations = 1;
        const auto result = sounddna::SoundDnaTransformationEngine(model).transform(fixturePatch(), {}, request);
        QCOMPARE(result.candidateEvaluations, 1);
    }

    void analysisOnlyDimensionRefusesToMutate()
    {
        auto dimension = bodyModel();
        dimension.evidence.transformationValidated = false;
        sounddna::SoundDnaKnowledgeModel model("test/1", std::string(sounddna::PatchFeatureExtractor::kSchemaVersion),
                                               {dimension});
        const auto result = sounddna::SoundDnaTransformationEngine(model).transform(fixturePatch(), {}, {"body", 80});
        QVERIFY(!result.patch);
        QVERIFY(!result.limitation.empty());
    }
};

QTEST_MAIN(SoundDnaTest)
#include "tst_sounddna.moc"
