// Phase 9 — structural search.
//
// Every number asserted here was measured from tests/fixtures/xp60/
// user-bank-amal.syx, a real XP-60 user bank, rather than assumed. They are
// pinned rather than bounded so that a change in what a structural fact means
// fails here instead of passing quietly.

#include "library/PatchStructure.h"

#include "xp60/Xp60Device.h"
#include "xpmodel/SysExStream.h"
#include "xpmodel/Xp60PatchCodec.h"
#include "xpmodel/Xp60PatchLayout.h"

#include <QtTest>

#include <algorithm>
#include <fstream>

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

} // namespace

class PatchStructureTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        const auto image = fixtureImage();
        for (int n = 1; n <= 128; ++n) {
            m_patches.push_back(*Xp60PatchCodec::decode(image, *Xp60PatchLayout::userPatchAddress(n)).patch);
            m_structures.push_back(PatchStructure::of(m_patches.back()));
        }
        QCOMPARE(m_structures.size(), std::size_t(128));
    }

    // --- what the structure reads ----------------------------------------

    void readsWhatTheParameterMapAlreadySays()
    {
        for (std::size_t i = 0; i < m_structures.size(); ++i) {
            const auto& structure = m_structures[i];
            const auto& patch = m_patches[i];

            QCOMPARE(structure.enabledToneCount, patch.enabledToneCount());
            QVERIFY(structure.enabledToneCount >= 0 && structure.enabledToneCount <= 4);
            for (const auto tone : ToneIndex::all()) {
                const auto& read = structure.tones[tone.index()];
                QCOMPARE(read.number, tone.number());
                QCOMPARE(read.enabled, patch.toneEnabled(tone));
                QCOMPARE(read.waveNumberDisplay, patch.wave(tone).numberDisplay);
                QCOMPARE(read.waveGroupId, patch.wave(tone).groupId);
                QCOMPARE(read.filterTypeRaw, patch.raw(tone, ToneParameter::FilterType));
            }
        }
    }

    void countsTheBankAsItActuallyIs()
    {
        const auto countWhere = [this](auto predicate) {
            return static_cast<int>(std::count_if(m_structures.begin(), m_structures.end(), predicate));
        };

        // Measured from the fixture, not guessed.
        QCOMPARE(countWhere([](const PatchStructure& s) { return s.enabledToneCount == 1; }), 30);
        QCOMPARE(countWhere([](const PatchStructure& s) { return s.enabledToneCount == 2; }), 37);
        QCOMPARE(countWhere([](const PatchStructure& s) { return s.enabledToneCount == 3; }), 19);
        QCOMPARE(countWhere([](const PatchStructure& s) { return s.enabledToneCount == 4; }), 42);

        QCOMPARE(countWhere([](const PatchStructure& s) { return s.hasKeyboardSplit(); }), 12);
        QCOMPARE(countWhere([](const PatchStructure& s) { return s.hasVelocitySwitching(); }), 12);
        QCOMPARE(countWhere([](const PatchStructure& s) { return s.usesExpansion(); }), 66);
        QCOMPARE(countWhere([](const PatchStructure& s) { return s.isLayered(); }), 74);
        QCOMPARE(countWhere([](const PatchStructure& s) { return s.portamento; }), 28);

        // Layered is defined as the complement: every multi-Tone Patch is
        // either divided (split or velocity) or stacked, never neither.
        const int multiTone = countWhere([](const PatchStructure& s) { return s.enabledToneCount > 1; });
        const int divided = countWhere([](const PatchStructure& s) {
            return s.enabledToneCount > 1 && (s.hasKeyboardSplit() || s.hasVelocitySwitching());
        });
        QCOMPARE(multiTone, 98);
        QCOMPARE(divided + 74, multiTone);
    }

    void aDisabledTonePutsNoBoardInTheRequirement()
    {
        // A Tone that is switched off still carries a stored wave, and that
        // data is read and kept. What it must not do is make the Patch claim to
        // need a board it will never play.
        const auto image = fixtureImage();
        auto patch = *Xp60PatchCodec::decode(image, *Xp60PatchLayout::userPatchAddress(1)).patch;

        // Point Tone 4 at an expansion wave, then switch it off.
        QVERIFY(patch.setRaw(ToneIndex::tone4(), ToneParameter::WaveGroupType, 2));
        QVERIFY(patch.setRaw(ToneIndex::tone4(), ToneParameter::WaveGroupId, 1));
        const auto withTone = PatchStructure::of(patch);
        QVERIFY(withTone.expansionGroups.contains(1));

        QVERIFY(patch.setRaw(ToneIndex::tone4(), ToneParameter::ToneSwitch, 0));
        const auto withoutTone = PatchStructure::of(patch);
        QVERIFY(!withoutTone.expansionGroups.contains(1));
        // The data itself is still there.
        QCOMPARE(withoutTone.tones[3].waveGroupTypeRaw, 2);
        QCOMPARE(withoutTone.tones[3].waveGroupId, 1);
        QVERIFY(!withoutTone.tones[3].enabled);
    }

    void aSplitWrittenIntoADisabledToneIsNotASplit()
    {
        const auto image = fixtureImage();
        auto patch = *Xp60PatchCodec::decode(image, *Xp60PatchLayout::userPatchAddress(2)).patch;
        QVERIFY(!PatchStructure::of(patch).hasKeyboardSplit());

        QVERIFY(patch.setRaw(ToneIndex::tone3(), ToneParameter::KeyboardRangeLower, 60));
        QVERIFY(PatchStructure::of(patch).hasKeyboardSplit());
        QVERIFY(patch.setRaw(ToneIndex::tone3(), ToneParameter::ToneSwitch, 0));
        QVERIFY(!PatchStructure::of(patch).hasKeyboardSplit());
    }

    void summaryNamesWhatIsThere()
    {
        const auto summary = m_structures[0].summary();
        QCOMPARE(summary, std::string("4 Tones, layered, internal waves only."));
    }

    // --- the search -------------------------------------------------------

    void anEmptyQueryMatchesEverythingAndSaysSo()
    {
        StructuralQuery query;
        QVERIFY(query.isEmpty());
        QCOMPARE(query.describe(), std::string("Every Patch."));
        for (const auto& structure : m_structures) {
            QVERIFY(query.matches(structure));
        }
    }

    void narrowsByToneCount()
    {
        StructuralQuery query;
        query.minimumEnabledTones = 4;
        QCOMPARE(matchCount(query), 42);

        query.maximumEnabledTones = 4;
        QCOMPARE(matchCount(query), 42);
        QCOMPARE(query.describe(), std::string("Patches with exactly 4 Tones."));

        StructuralQuery atMostTwo;
        atMostTwo.maximumEnabledTones = 2;
        QCOMPARE(matchCount(atMostTwo), 30 + 37);
    }

    void combinesFiltersAsNarrowing()
    {
        // Four-Tone velocity stacks that need no expansion board — a question
        // the name field cannot answer.
        StructuralQuery query;
        query.minimumEnabledTones = 4;
        query.velocitySwitching = true;
        query.usesExpansion = false;

        const int matched = matchCount(query);
        // Each added filter can only narrow.
        StructuralQuery looser;
        looser.minimumEnabledTones = 4;
        looser.velocitySwitching = true;
        QVERIFY(matched <= matchCount(looser));
        QVERIFY(matchCount(looser) <= 42);

        for (const auto& structure : m_structures) {
            if (!query.matches(structure)) {
                continue;
            }
            QCOMPARE(structure.enabledToneCount, 4);
            QVERIFY(structure.hasVelocitySwitching());
            QVERIFY(!structure.usesExpansion());
        }
        QVERIFY(query.describe().find("at least 4 Tones") != std::string::npos);
        QVERIFY(query.describe().find("velocity switching") != std::string::npos);
        QVERIFY(query.describe().find("internal waves only") != std::string::npos);
    }

    void aFalseFlagIsAFilterNotAnAbsentOne()
    {
        StructuralQuery uses;
        uses.usesExpansion = true;
        StructuralQuery doesNot;
        doesNot.usesExpansion = false;

        QCOMPARE(matchCount(uses), 66);
        QCOMPARE(matchCount(doesNot), 128 - 66);
        // Together they are the whole bank and they never overlap.
        for (const auto& structure : m_structures) {
            QVERIFY(uses.matches(structure) != doesNot.matches(structure));
        }
    }

    void aSetMeansAnyOfRatherThanAllOf()
    {
        // Filter type 1 is on 240 of the bank's enabled Tones and type 3 on 14.
        StructuralQuery one;
        one.filterTypes = {1};
        StructuralQuery three;
        three.filterTypes = {3};
        StructuralQuery either;
        either.filterTypes = {1, 3};

        QVERIFY(matchCount(either) >= matchCount(one));
        QVERIFY(matchCount(either) >= matchCount(three));
        for (const auto& structure : m_structures) {
            QCOMPARE(either.matches(structure), one.matches(structure) || three.matches(structure));
        }
    }

    void findsAPatchByTheExactWaveItPlays()
    {
        // Take a wave a real Patch actually uses, so the search is over data
        // rather than over an invented reference.
        const auto& reference = m_structures[0].tones[0];
        StructuralQuery query;
        query.wave = StructuralQuery::WaveMatch{reference.waveGroupTypeRaw, reference.waveGroupId,
                                                reference.waveNumberDisplay};

        QVERIFY(matchCount(query) >= 1);
        for (const auto& structure : m_structures) {
            if (!query.matches(structure)) {
                continue;
            }
            const bool found = std::any_of(structure.tones.begin(), structure.tones.end(),
                                           [&reference](const PatchStructure::Tone& tone) {
                                               return tone.enabled
                                                   && tone.waveGroupTypeRaw == reference.waveGroupTypeRaw
                                                   && tone.waveGroupId == reference.waveGroupId
                                                   && tone.waveNumberDisplay == reference.waveNumberDisplay;
                                           });
            QVERIFY(found);
        }

        // The group is part of the identity: wave 12 of one board is not wave
        // 12 of another.
        StructuralQuery otherGroup = query;
        otherGroup.wave->groupId = reference.waveGroupId + 77;
        QCOMPARE(matchCount(otherGroup), 0);
    }

    void everyToneMatchingIsAStricterQuestionThanAnyToneMatching()
    {
        const auto& reference = m_structures[0].tones[0];
        StructuralQuery any;
        any.wave = StructuralQuery::WaveMatch{reference.waveGroupTypeRaw, reference.waveGroupId,
                                              reference.waveNumberDisplay};
        StructuralQuery all = any;
        all.allTonesMatchWave = true;

        QVERIFY(matchCount(all) <= matchCount(any));
        for (const auto& structure : m_structures) {
            if (all.matches(structure)) {
                QVERIFY(any.matches(structure));
            }
        }
        QVERIFY(all.describe().find("every Tone playing") != std::string::npos);
        QVERIFY(any.describe().find("a Tone playing") != std::string::npos);
    }

    void aSilentPatchNeverMatchesEveryTonePlayingAWave()
    {
        const auto image = fixtureImage();
        auto patch = *Xp60PatchCodec::decode(image, *Xp60PatchLayout::userPatchAddress(1)).patch;
        for (const auto tone : ToneIndex::all()) {
            QVERIFY(patch.setRaw(tone, ToneParameter::ToneSwitch, 0));
        }
        const auto structure = PatchStructure::of(patch);
        QCOMPARE(structure.enabledToneCount, 0);

        StructuralQuery query;
        query.wave = StructuralQuery::WaveMatch{structure.tones[0].waveGroupTypeRaw,
                                                structure.tones[0].waveGroupId,
                                                structure.tones[0].waveNumberDisplay};
        query.allTonesMatchWave = true;
        // "All of nothing" is vacuously true and would be a lie about a Patch
        // that plays no wave at all.
        QVERIFY(!query.matches(structure));
    }

    void narrowsByExpansionGroup()
    {
        std::set<int> groupsInBank;
        for (const auto& structure : m_structures) {
            groupsInBank.insert(structure.expansionGroups.begin(), structure.expansionGroups.end());
        }
        QVERIFY(!groupsInBank.empty());

        int summed = 0;
        for (int group : groupsInBank) {
            StructuralQuery query;
            query.expansionGroups = {group};
            const int matched = matchCount(query);
            QVERIFY(matched > 0);
            summed += matched;
            for (const auto& structure : m_structures) {
                if (query.matches(structure)) {
                    QVERIFY(structure.expansionGroups.contains(group));
                }
            }
        }
        // A Patch may refer to more than one board, so the per-group counts sum
        // to at least the number of Patches using any expansion wave.
        QVERIFY(summed >= 66);

        StructuralQuery anyGroup;
        anyGroup.expansionGroups = groupsInBank;
        QCOMPARE(matchCount(anyGroup), 66);
    }

    void aQueryForSomethingAbsentFindsNothingRatherThanEverything()
    {
        StructuralQuery query;
        query.expansionGroups = {123};  // no such board in this bank
        QCOMPARE(matchCount(query), 0);

        StructuralQuery impossible;
        impossible.minimumEnabledTones = 4;
        impossible.maximumEnabledTones = 1;
        QCOMPARE(matchCount(impossible), 0);
    }

    void matchingAPatchAndMatchingItsStructureAgree()
    {
        StructuralQuery query;
        query.minimumEnabledTones = 2;
        query.usesExpansion = true;
        for (std::size_t i = 0; i < m_patches.size(); ++i) {
            QCOMPARE(query.matches(m_patches[i]), query.matches(m_structures[i]));
        }
    }

private:
    int matchCount(const StructuralQuery& query) const
    {
        return static_cast<int>(std::count_if(
            m_structures.begin(), m_structures.end(),
            [&query](const PatchStructure& structure) { return query.matches(structure); }));
    }

    std::vector<Xp60Patch> m_patches;
    std::vector<PatchStructure> m_structures;
};

QTEST_MAIN(PatchStructureTest)
#include "tst_patch_structure.moc"
