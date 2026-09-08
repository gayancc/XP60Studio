// Phase 10 — constrained variation.
//
// "Vary this Patch" is where invention creeps into a synthesizer editor, so the
// tests are about the constraints rather than the nudging: what is never
// touched, what is reported, and that the same seed gives the same result.

#include "library/PatchVariation.h"

#include "xp60/Xp60Device.h"
#include "xpmodel/SysExStream.h"
#include "xpmodel/Xp60PatchCodec.h"
#include "xpmodel/Xp60PatchDiff.h"
#include "xpmodel/Xp60PatchLayout.h"

#include <QtTest>

#include <algorithm>
#include <fstream>
#include <set>

using namespace xp60studio;
using namespace xp60studio::library;
using namespace xp60studio::roland;
using namespace xp60studio::xpmodel;

namespace {

MemoryImage fixtureImage()
{
    std::ifstream in(XP60STUDIO_FIXTURE_DIR "/user-bank-amal.syx", std::ios::binary);
    const ByteVector data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const std::vector<RolandModelId> models{xp60::modelId()};
    return imageFromStream(parseSysExStream(data, models));
}

Xp60Patch userPatch(const MemoryImage& image, int n)
{
    return *Xp60PatchCodec::decode(image, *Xp60PatchLayout::userPatchAddress(n)).patch;
}

} // namespace

class PatchVariationTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        const auto image = fixtureImage();
        for (int n = 1; n <= 128; ++n) {
            m_patches.push_back(userPatch(image, n));
        }
    }

    // --- what is never touched ---------------------------------------------

    void identitiesAreNeverVaried()
    {
        // Every parameter the instrument's own tables describe, checked against
        // the rule rather than against a list somebody typed.
        for (const auto* table :
             {&Xp60PatchLayout::patchCommonTable(), &Xp60PatchLayout::patchToneTable()}) {
            for (const auto& parameter : table->parameters()) {
                if (parameter.isEnumeration() || parameter.isText()
                    || (parameter.rawMin == 0 && parameter.rawMax == 1)
                    || parameter.category == "Wave") {
                    QVERIFY2(!PatchVariation::isContinuous(parameter),
                             qPrintable(QString::fromUtf8(parameter.name.data(),
                                                          qsizetype(parameter.name.size()))));
                    QVERIFY(!PatchVariation::whyNotVaried(parameter).empty());
                }
            }
        }
        // ...and something plainly continuous is allowed.
        const auto* cutoff = Xp60PatchLayout::patchToneTable().find("tone.cutoff_frequency");
        QVERIFY(cutoff != nullptr);
        QVERIFY(PatchVariation::isContinuous(*cutoff));
        QVERIFY(PatchVariation::whyNotVaried(*cutoff).empty());
    }

    void aVariationMovesNoIdentityAndNoSwitch()
    {
        PatchVariationOptions options;
        options.amountPercent = 20;
        options.densityPercent = 100;  // touch everything it is allowed to

        for (std::size_t i = 0; i < 12; ++i) {
            options.seed = 1000 + i;
            const auto result = PatchVariation::apply(m_patches[i], options);
            QVERIFY2(result.ok, result.reason.c_str());

            const auto diff = Xp60PatchDiff::compare(m_patches[i], *result.patch);
            for (const auto& difference : diff.differences()) {
                // Not the name, not a wave, and nothing with an enumeration or
                // a two-value range behind it.
                QVERIFY(difference.category != "Name");
                QVERIFY(difference.category != "Wave");
            }
            // The Tone switches are untouched, so the same Tones sound.
            for (const auto tone : ToneIndex::all()) {
                QCOMPARE(result.patch->toneEnabled(tone), m_patches[i].toneEnabled(tone));
            }
            QCOMPARE(result.patch->name(), m_patches[i].name());
        }
    }

    void aSilentToneIsNeverVaried()
    {
        // Find a Patch with at least one Tone switched off.
        const Xp60Patch* patch = nullptr;
        ToneIndex silent = ToneIndex::tone1();
        for (const auto& candidate : m_patches) {
            for (const auto tone : ToneIndex::all()) {
                if (!candidate.toneEnabled(tone)) {
                    patch = &candidate;
                    silent = tone;
                    break;
                }
            }
            if (patch != nullptr) {
                break;
            }
        }
        QVERIFY2(patch != nullptr, "the fixture has Patches with fewer than four Tones");

        PatchVariationOptions options;
        options.densityPercent = 100;
        options.amountPercent = 25;
        const auto result = PatchVariation::apply(*patch, options);
        QVERIFY(result.ok);

        // Changing what nobody hears is change without effect.
        const std::string block = "Tone " + std::to_string(silent.number());
        for (const auto& change : result.changes) {
            QVERIFY2(change.block != block, "a Tone that is switched off must be left alone");
        }
    }

    // --- determinism --------------------------------------------------------

    void theSameSeedGivesTheSameVariation()
    {
        PatchVariationOptions options;
        options.seed = 12345;
        const auto first = PatchVariation::apply(m_patches[3], options);
        const auto second = PatchVariation::apply(m_patches[3], options);

        QVERIFY(first.ok && second.ok);
        QCOMPARE(first.changedParameters, second.changedParameters);
        QVERIFY(Xp60PatchDiff::compare(*first.patch, *second.patch).identical());
    }

    void adifferentSeedGivesADifferentVariation()
    {
        PatchVariationOptions options;
        options.seed = 1;
        const auto first = PatchVariation::apply(m_patches[3], options);
        options.seed = 2;
        const auto second = PatchVariation::apply(m_patches[3], options);

        QVERIFY(first.ok && second.ok);
        QVERIFY(!Xp60PatchDiff::compare(*first.patch, *second.patch).identical());
    }

    // --- the amount means what it says --------------------------------------

    void everyMoveStaysWithinTheAskedFractionOfItsOwnRange()
    {
        PatchVariationOptions options;
        options.amountPercent = 10;
        options.densityPercent = 100;
        options.seed = 77;

        const auto result = PatchVariation::apply(m_patches[6], options);
        QVERIFY(result.ok);
        QVERIFY(result.changedParameters > 0);

        for (const auto& change : result.changes) {
            const auto* table = change.block == "Patch Common"
                ? &Xp60PatchLayout::patchCommonTable()
                : &Xp60PatchLayout::patchToneTable();
            const ParameterDescriptor* parameter = nullptr;
            for (const auto& candidate : table->parameters()) {
                if (candidate.name == change.parameterName) {
                    parameter = &candidate;
                    break;
                }
            }
            QVERIFY(parameter != nullptr);

            // Within the documented range...
            QVERIFY(parameter->isRawInRange(change.after));
            // ...and no further than the asked fraction of this parameter's own
            // range, which is what makes a coarse field and a fine one move by
            // comparable musical amounts.
            const int span = parameter->rawMax - parameter->rawMin;
            const int reach = std::max(1, (span * options.amountPercent + 50) / 100);
            QVERIFY2(std::abs(change.after - change.before) <= reach,
                     qPrintable(QString::fromStdString(change.parameterName)));
        }
    }

    void densityBoundsHowMuchMoves()
    {
        PatchVariationOptions sparse;
        sparse.densityPercent = 5;
        sparse.seed = 9;
        PatchVariationOptions dense = sparse;
        dense.densityPercent = 100;

        const auto few = PatchVariation::apply(m_patches[8], sparse);
        const auto many = PatchVariation::apply(m_patches[8], dense);
        QVERIFY(few.ok && many.ok);
        QCOMPARE(few.eligibleParameters, many.eligibleParameters);
        QVERIFY(few.changedParameters < many.changedParameters);
        QVERIFY(many.changedParameters <= many.eligibleParameters);
    }

    void clampingIsCountedRatherThanHidden()
    {
        // Drive a parameter to the top of its range, then ask for a big
        // variation: some moves must run into the ceiling.
        auto patch = m_patches[0];
        for (const auto tone : ToneIndex::all()) {
            QVERIFY(patch.setRaw(tone, ToneParameter::CutoffFrequency, 127));
            QVERIFY(patch.setRaw(tone, ToneParameter::Resonance, 127));
        }
        PatchVariationOptions options;
        options.amountPercent = 50;
        options.densityPercent = 100;
        options.seed = 4;
        options.components = {PatchComponent::Filter};

        const auto result = PatchVariation::apply(patch, options);
        QVERIFY(result.ok);
        QVERIFY2(result.clampedParameters > 0,
                 "a parameter already at its ceiling cannot move up, and saying otherwise would "
                 "overstate the variation's reach");
        QVERIFY(result.summary().find("cut short") != std::string::npos);
        for (const auto& change : result.changes) {
            QVERIFY(change.after >= 0 && change.after <= 127);
        }
    }

    // --- scope ---------------------------------------------------------------

    void scopingToAComponentTouchesOnlyThatComponent()
    {
        PatchVariationOptions options;
        options.components = {PatchComponent::FilterEnvelope};
        options.densityPercent = 100;
        options.seed = 21;

        const auto result = PatchVariation::apply(m_patches[2], options);
        QVERIFY(result.ok);
        QVERIFY(result.changedParameters > 0);
        for (const auto& change : result.changes) {
            QCOMPARE(change.category, std::string("TVF Envelope"));
            QVERIFY(change.block != "Patch Common");
        }
    }

    void scopingToOneToneLeavesTheOthers()
    {
        PatchVariationOptions options;
        options.tones = {ToneIndex::tone1()};
        options.densityPercent = 100;
        options.seed = 31;

        // Take a Patch whose Tone 1 is on, so there is something to vary.
        const Xp60Patch* patch = nullptr;
        for (const auto& candidate : m_patches) {
            if (candidate.toneEnabled(ToneIndex::tone1())) {
                patch = &candidate;
                break;
            }
        }
        QVERIFY(patch != nullptr);

        const auto result = PatchVariation::apply(*patch, options);
        QVERIFY(result.ok);
        const auto diff = Xp60PatchDiff::compare(*patch, *result.patch);
        for (const auto& difference : diff.differences()) {
            QVERIFY(difference.block == "Tone 1" || difference.block == "Patch Common");
        }
    }

    // --- refusals -------------------------------------------------------------

    void anImpossibleAmountIsRefused()
    {
        PatchVariationOptions options;
        options.amountPercent = 0;
        auto result = PatchVariation::apply(m_patches[0], options);
        QVERIFY(!result.ok);
        QVERIFY(!result.patch.has_value());
        QCOMPARE(result.summary(), result.reason);

        options.amountPercent = 5;
        options.densityPercent = 0;
        result = PatchVariation::apply(m_patches[0], options);
        QVERIFY(!result.ok);
    }

    void aScopeWithNothingContinuousSaysSo()
    {
        PatchVariationOptions options;
        // Wave is entirely identities, so there is nothing in it to vary.
        options.components = {PatchComponent::Wave};
        const auto result = PatchVariation::apply(m_patches[0], options);
        QVERIFY(!result.ok);
        QVERIFY(!result.patch.has_value());
        QVERIFY(result.reason.find("identit") != std::string::npos);
    }

    // --- across the bank -------------------------------------------------------

    void everyPatchInTheBankVariesWithoutLeavingItsRanges()
    {
        PatchVariationOptions options;
        options.amountPercent = 15;
        options.densityPercent = 60;

        for (std::size_t i = 0; i < m_patches.size(); ++i) {
            options.seed = i;
            const auto result = PatchVariation::apply(m_patches[i], options);
            QVERIFY2(result.ok, result.reason.c_str());
            QVERIFY(result.eligibleParameters > 0);
            QVERIFY(result.changedParameters <= result.eligibleParameters);
            // The result re-encodes: a variation that produced a Patch the
            // codec could not write would be useless.
            QVERIFY(result.patch->name() == m_patches[i].name());
        }
    }

private:
    std::vector<Xp60Patch> m_patches;
};

QTEST_MAIN(PatchVariationTest)
#include "tst_patch_variation.moc"
