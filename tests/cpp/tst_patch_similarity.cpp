#include "library/PatchFingerprint.h"
#include "library/PatchSimilarity.h"

#include "xp60/Xp60Device.h"
#include "xpmodel/PatchName.h"
#include "xpmodel/SysExStream.h"
#include "xpmodel/Xp60PatchCodec.h"
#include "xpmodel/Xp60PatchLayout.h"

#include <QtTest>

#include <fstream>

using namespace xp60studio;
using namespace xp60studio::library;
using namespace xp60studio::roland;
using namespace xp60studio::xpmodel;

namespace {

// The golden fixture again: a similarity score is a claim about real sounds, so
// it is measured against 128 real Patches rather than synthetic byte patterns.
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

class PatchSimilarityTest : public QObject
{
    Q_OBJECT

private slots:
    void aPatchIsIdenticalToItself()
    {
        const auto image = fixtureImage();
        const auto patch = userPatch(image, 1);
        const auto similarity = PatchSimilarity::compare(patch, patch);

        QVERIFY(similarity.identical());
        QVERIFY(similarity.identicalSound());
        QVERIFY(similarity.nameEqual());
        QCOMPARE(similarity.differingParameters(), 0);
        QCOMPARE(similarity.equalParameters(), similarity.comparedParameters());
        QCOMPARE(similarity.score(), 1.0);
        QCOMPARE(similarity.percent(), 100);
        QCOMPARE(similarity.summary(), std::string("Identical."));
    }

    void everyDocumentedSoundParameterIsCounted()
    {
        const auto image = fixtureImage();
        const auto similarity = PatchSimilarity::compare(userPatch(image, 1), userPatch(image, 2));

        // Patch Common then Tone 1..4, in layout order, and nothing else.
        QCOMPARE(similarity.blocks().size(), std::size_t(5));
        QCOMPARE(similarity.blocks()[0].block, std::string("Patch Common"));
        QCOMPARE(similarity.blocks()[4].block, std::string("Tone 4"));

        // The blocks partition the comparison: no parameter counted twice, none
        // dropped. This is the property the whole score rests on.
        int compared = 0;
        int equal = 0;
        for (const auto& block : similarity.blocks()) {
            compared += block.comparedParameters;
            equal += block.equalParameters;
            QVERIFY(block.equalParameters >= 0);
            QVERIFY(block.equalParameters <= block.comparedParameters);
        }
        QCOMPARE(compared, similarity.comparedParameters());
        QCOMPARE(equal, similarity.equalParameters());

        // The four Tones are the same table, so they contribute equally.
        const int tone = similarity.blocks()[1].comparedParameters;
        QVERIFY(tone > 0);
        for (int i = 2; i <= 4; ++i) {
            QCOMPARE(similarity.blocks()[std::size_t(i)].comparedParameters, tone);
        }

        // Counted per parameter, not per byte: the Patch is 2816 bytes.
        QVERIFY(similarity.comparedParameters() > 0);
        QVERIFY(similarity.comparedParameters() < 2816);
    }

    void theTwelveNameBytesAreExcludedFromTheCount()
    {
        const auto image = fixtureImage();
        const auto patch = userPatch(image, 1);
        auto renamed = patch;
        QVERIFY(renamed.setName(*PatchName::fromText("Renamed")));
        QVERIFY(renamed.name() != patch.name());

        const auto similarity = PatchSimilarity::compare(patch, renamed);

        // The same sound under a different name: the score must not move, or a
        // rename would hide exactly the near-duplicate this feature hunts.
        QCOMPARE(similarity.score(), 1.0);
        QCOMPARE(similarity.percent(), 100);
        QVERIFY(similarity.identicalSound());
        QVERIFY(!similarity.identical());
        QVERIFY(!similarity.nameEqual());
        QCOMPARE(similarity.differingParameters(), 0);
        QCOMPARE(similarity.summary(), std::string("The same sound under a different name."));

        // Nothing is hidden: the underlying diff still reports the name bytes.
        QVERIFY(!similarity.differences().identical());
    }

    void oneChangedParameterIsOneDifference()
    {
        const auto image = fixtureImage();
        const auto original = userPatch(image, 1);
        auto edited = original;
        const int before = edited.raw(ToneIndex::tone3(), ToneParameter::CutoffFrequency);
        QVERIFY(edited.setRaw(ToneIndex::tone3(), ToneParameter::CutoffFrequency,
                              before == 127 ? 126 : before + 1));

        const auto similarity = PatchSimilarity::compare(original, edited);

        QCOMPARE(similarity.differingParameters(), 1);
        QVERIFY(!similarity.identicalSound());
        QVERIFY(similarity.nameEqual());
        QCOMPARE(similarity.blocks()[3].differingParameters(), 1);
        for (std::size_t i : {std::size_t(0), std::size_t(1), std::size_t(2), std::size_t(4)}) {
            QCOMPARE(similarity.blocks()[i].differingParameters(), 0);
        }

        // One parameter out of the whole Patch rounds to 100%, but 100% must
        // mean identical, so it is reported as 99%.
        QVERIFY(similarity.score() > 0.99);
        QCOMPARE(similarity.percent(), 99);

        const auto summary = similarity.summary();
        QVERIFY(summary.find("99% alike") != std::string::npos);
        QVERIFY(summary.find("1 parameter differs") != std::string::npos);
        QVERIFY(summary.find("Tone 3 1") != std::string::npos);
        QVERIFY(summary.find("names differ") == std::string::npos);
    }

    void summaryRanksTheBlocksThatMovedMost()
    {
        const auto image = fixtureImage();
        const auto original = userPatch(image, 1);
        auto edited = original;

        const auto bump = [&edited](ToneIndex tone, ToneParameter parameter) {
            const int before = edited.raw(tone, parameter);
            QVERIFY(edited.setRaw(tone, parameter, before == 127 ? 126 : before + 1));
        };
        bump(ToneIndex::tone2(), ToneParameter::CutoffFrequency);
        bump(ToneIndex::tone2(), ToneParameter::Resonance);
        bump(ToneIndex::tone4(), ToneParameter::CutoffFrequency);
        QVERIFY(edited.setName(*PatchName::fromText("Edited")));

        const auto similarity = PatchSimilarity::compare(original, edited);

        QCOMPARE(similarity.differingParameters(), 3);
        const auto summary = similarity.summary();
        QVERIFY(summary.find("3 parameters differ") != std::string::npos);
        // Worst block first, so a musician reads where to look.
        QVERIFY(summary.find("Tone 2 2") < summary.find("Tone 4 1"));
        QVERIFY(summary.find("The names differ too.") != std::string::npos);
    }

    void similarityIsSymmetric()
    {
        const auto image = fixtureImage();
        const auto a = userPatch(image, 7);
        const auto b = userPatch(image, 42);

        const auto forward = PatchSimilarity::compare(a, b);
        const auto backward = PatchSimilarity::compare(b, a);

        QCOMPARE(forward.score(), backward.score());
        QCOMPARE(forward.differingParameters(), backward.differingParameters());
        QCOMPARE(forward.nameEqual(), backward.nameEqual());
        QCOMPARE(forward.summary(), backward.summary());
    }

    void everyPairInTheFixtureScoresInRange()
    {
        const auto image = fixtureImage();
        std::vector<Xp60Patch> patches;
        patches.reserve(128);
        for (int n = 1; n <= 128; ++n) {
            patches.push_back(userPatch(image, n));
        }

        int identicalSound = 0;
        int renamedDuplicates = 0;
        for (std::size_t i = 0; i < patches.size(); ++i) {
            for (std::size_t j = i + 1; j < patches.size(); ++j) {
                const auto similarity = PatchSimilarity::compare(patches[i], patches[j]);
                QVERIFY(similarity.score() >= 0.0 && similarity.score() <= 1.0);
                QVERIFY(similarity.percent() >= 0 && similarity.percent() <= 100);
                // 100 is reserved for genuinely identical sounds.
                QVERIFY(similarity.percent() < 100 || similarity.identicalSound());
                QCOMPARE(similarity.differingParameters(),
                         similarity.comparedParameters() - similarity.equalParameters());
                if (similarity.identicalSound()) {
                    ++identicalSound;
                    if (!similarity.nameEqual()) {
                        ++renamedDuplicates;
                    }
                }
            }
        }

        // This bank really does carry duplicates: eleven exact pairs, plus one
        // pair that is the same sound under a different name. The count is
        // pinned rather than bounded, so a change in what "the same sound"
        // means shows up here instead of passing quietly.
        QCOMPARE(identicalSound, 12);
        QCOMPARE(renamedDuplicates, 1);
    }

    void aRenamedDuplicateIsWhatTheFingerprintCannotSee()
    {
        const auto image = fixtureImage();
        // User 39 "Vocal Fall 1" and User 40 "Vocal Fall 2" differ in exactly
        // one byte of the whole 2816-byte Patch: the twelfth name character.
        const auto a = userPatch(image, 39);
        const auto b = userPatch(image, 40);
        QVERIFY(a.name() != b.name());

        // The fingerprint hashes the Patch as stored, name included, so it says
        // these are two different Patches — correctly, for its own question.
        QVERIFY(PatchFingerprint::of(a) != PatchFingerprint::of(b));

        // Similarity answers the librarian's question instead, and this is the
        // case the whole feature exists for.
        const auto similarity = PatchSimilarity::compare(a, b);
        QVERIFY(similarity.identicalSound());
        QVERIFY(!similarity.identical());
        QVERIFY(!similarity.nameEqual());
        QCOMPARE(similarity.differingParameters(), 0);
        QCOMPARE(similarity.summary(), std::string("The same sound under a different name."));
        // One difference, and it is the name.
        QCOMPARE(similarity.differences().count(), std::size_t(1));
        QCOMPARE(similarity.differences().differences()[0].category, std::string("Name"));
    }

    void unrelatedPatchesStillShareTheirDefaults()
    {
        const auto image = fixtureImage();
        const auto similarity = PatchSimilarity::compare(userPatch(image, 1), userPatch(image, 64));

        // Two different sounds are never near zero: an XP-60 Patch has hundreds
        // of parameters most of which sit at their usual values. The score is a
        // fraction of documented parameters, not a perceptual distance, and the
        // header says so — this test pins that reading down.
        QVERIFY(!similarity.identicalSound());
        QVERIFY(similarity.score() > 0.2);
        QVERIFY(similarity.score() < 1.0);
        QVERIFY(similarity.summary().find("% alike") != std::string::npos);
    }
};

QTEST_MAIN(PatchSimilarityTest)
#include "tst_patch_similarity.moc"
