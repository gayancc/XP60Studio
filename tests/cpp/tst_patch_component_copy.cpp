// Phase 10 — copying one part of a Patch into another.
//
// Written around what the copy leaves behind rather than what it moves: taking
// a filter across is easy, and knowing that the Structure pairing stayed with
// the destination, or that the wave needs a board, is what makes it usable.

#include "library/PatchComponentCopy.h"

#include "xp60/Xp60Device.h"
#include "xpmodel/PatchName.h"
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

std::set<std::string> categoriesThatDiffer(const Xp60Patch& before, const Xp60Patch& after)
{
    std::set<std::string> out;
    const auto diff = Xp60PatchDiff::compare(before, after);
    for (const auto& difference : diff.differences()) {
        out.insert(difference.category);
    }
    return out;
}

} // namespace

class PatchComponentCopyTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        const auto image = fixtureImage();
        for (int n = 1; n <= 128; ++n) {
            m_patches.push_back(userPatch(image, n));
        }
        QCOMPARE(m_patches.size(), std::size_t(128));
    }

    // --- the components partition the Patch --------------------------------

    void everyToneParameterBelongsToWholeTone()
    {
        std::set<std::string> inTable;
        for (const auto& parameter : Xp60PatchLayout::patchToneTable().parameters()) {
            inTable.insert(std::string(parameter.category));
        }
        std::set<std::string> inComponent;
        for (const auto category : PatchComponentCopy::categoriesOf(PatchComponent::WholeTone)) {
            inComponent.insert(std::string(category));
        }
        // Both ways: nothing in the instrument's own table is left out, and
        // nothing is listed that the table does not have. A UI showing what a
        // copy will touch shows the same thing the copy does.
        QCOMPARE(inComponent, inTable);
    }

    void thePatchComponentsCoverPatchCommonExactlyOnce()
    {
        std::set<std::string> inTable;
        for (const auto& parameter : Xp60PatchLayout::patchCommonTable().parameters()) {
            inTable.insert(std::string(parameter.category));
        }
        std::set<std::string> covered{"Name"};  // never copied, by design
        for (const auto component : {PatchComponent::PatchEffects,
                                     PatchComponent::PatchCommonSettings,
                                     PatchComponent::Structure}) {
            for (const auto category : PatchComponentCopy::categoriesOf(component)) {
                covered.insert(std::string(category));
            }
        }
        QCOMPARE(covered, inTable);
    }

    void toneScopeIsDeclaredForEveryComponent()
    {
        for (const auto component : allPatchComponents()) {
            QVERIFY(!patchComponentName(component).empty());
            QVERIFY(!PatchComponentCopy::categoriesOf(component).empty());
        }
        QVERIFY(patchComponentIsToneScoped(PatchComponent::Filter));
        QVERIFY(!patchComponentIsToneScoped(PatchComponent::Structure));
        QVERIFY(!patchComponentIsToneScoped(PatchComponent::PatchEffects));
    }

    // --- copying ------------------------------------------------------------

    void copyingAWholeToneReproducesItExactly()
    {
        PatchCopyRequest request;
        request.component = PatchComponent::WholeTone;
        request.fromTone = ToneIndex::tone1();
        request.toTone = ToneIndex::tone3();

        const auto result = PatchComponentCopy::apply(m_patches[1], m_patches[0], request);
        QVERIFY2(result.ok, result.reason.c_str());
        QVERIFY(result.patch.has_value());
        QCOMPARE(result.parametersCopied, 128);

        // Every one of the Tone's parameters, byte for byte.
        const auto& table = Xp60PatchLayout::patchToneTable();
        for (std::size_t i = 0; i < table.parameters().size(); ++i) {
            QCOMPARE(result.patch->tone(ToneIndex::tone3()).rawAt(i),
                     m_patches[0].tone(ToneIndex::tone1()).rawAt(i));
        }
        // ...and nothing else moved. The diff is bound to a name: a range-for
        // over a temporary's member container does not extend the temporary's
        // lifetime, and the differences hold views into it.
        const auto diff = Xp60PatchDiff::compare(m_patches[1], *result.patch);
        for (const auto& difference : diff.differences()) {
            QCOMPARE(difference.block, std::string("Tone 3"));
        }
    }

    void copyingAFilterTouchesOnlyTheFilter()
    {
        PatchCopyRequest request;
        request.component = PatchComponent::Filter;
        request.fromTone = ToneIndex::tone1();
        request.toTone = ToneIndex::tone1();

        const auto result = PatchComponentCopy::apply(m_patches[9], m_patches[0], request);
        QVERIFY2(result.ok, result.reason.c_str());
        QCOMPARE(result.parametersCopied, 5);

        // Exactly the five parameters Roland files under TVF, and nothing that
        // merely sounds related — the TVF Envelope is its own component.
        const auto moved = categoriesThatDiffer(m_patches[9], *result.patch);
        for (const auto& category : moved) {
            QCOMPARE(category, std::string("TVF"));
        }
        QVERIFY(result.parametersChanged > 0);
    }

    void copyingOntoItselfChangesNothing()
    {
        PatchCopyRequest request;
        request.component = PatchComponent::WholeTone;
        request.fromTone = ToneIndex::tone2();
        request.toTone = ToneIndex::tone2();

        const auto result = PatchComponentCopy::apply(m_patches[4], m_patches[4], request);
        QVERIFY(result.ok);
        QCOMPARE(result.parametersChanged, 0);
        QVERIFY(Xp60PatchDiff::compare(m_patches[4], *result.patch).identical());
    }

    void thePatchNameIsNeverCopied()
    {
        const auto source = m_patches[0];
        auto destination = m_patches[1];
        QVERIFY(destination.setName(*PatchName::fromText("Keep Me")));
        QVERIFY(source.name() != destination.name());

        for (const auto component : allPatchComponents()) {
            PatchCopyRequest request;
            request.component = component;
            request.fromTone = ToneIndex::tone1();
            request.toTone = ToneIndex::tone1();
            const auto result = PatchComponentCopy::apply(destination, source, request);
            QVERIFY2(result.ok, result.reason.c_str());
            // A Patch that took on the name of the one a filter came from is a
            // Patch nobody could find again.
            QCOMPARE(result.patch->name(), destination.name());
        }
    }

    void patchScopedComponentsMoveOnlyPatchCommon()
    {
        PatchCopyRequest request;
        request.component = PatchComponent::PatchEffects;

        const auto result = PatchComponentCopy::apply(m_patches[20], m_patches[0], request);
        QVERIFY2(result.ok, result.reason.c_str());
        const auto diff = Xp60PatchDiff::compare(m_patches[20], *result.patch);
        for (const auto& difference : diff.differences()) {
            QCOMPARE(difference.block, std::string("Patch Common"));
            QVERIFY(difference.category == "EFX" || difference.category == "Chorus"
                    || difference.category == "Reverb");
        }
    }

    // --- refusals -----------------------------------------------------------

    void aToneComponentWithoutTonesIsRefused()
    {
        PatchCopyRequest request;
        request.component = PatchComponent::Filter;  // no tones given

        const auto result = PatchComponentCopy::apply(m_patches[1], m_patches[0], request);
        QVERIFY(!result.ok);
        QVERIFY(!result.patch.has_value());
        QVERIFY(result.reason.find("belongs to a Tone") != std::string::npos);
        QCOMPARE(result.summary(), result.reason);
    }

    void aRefusedCopyLeavesNoPartialPatch()
    {
        PatchCopyRequest request;
        request.component = PatchComponent::Lfo1;  // tone-scoped, no tones
        const auto result = PatchComponentCopy::apply(m_patches[1], m_patches[0], request);
        QVERIFY(!result.ok);
        // Nothing to apply, so nothing was applied. The destination the caller
        // holds is untouched because the copy works on its own copy.
        QVERIFY(!result.patch.has_value());
    }

    // --- what it says stayed behind ----------------------------------------

    void aToneCopyAlwaysSaysTheStructureStayedBehind()
    {
        PatchCopyRequest request;
        request.component = PatchComponent::WholeTone;
        request.fromTone = ToneIndex::tone1();
        request.toTone = ToneIndex::tone2();

        const auto result = PatchComponentCopy::apply(m_patches[1], m_patches[0], request);
        QVERIFY(result.ok);
        // Structure Type pairs Tones 1&2 and 3&4 in Patch Common, so it is not
        // part of any Tone and cannot travel with one.
        QVERIFY(std::any_of(result.notes.begin(), result.notes.end(), [](const std::string& note) {
            return note.find("Structure Type") != std::string::npos;
        }));
        QVERIFY(result.summary().find("Structure Type") != std::string::npos);
    }

    void anExpansionWaveIsNamedWhenItTravels()
    {
        // Find a real Tone in the bank that plays an expansion wave.
        const Xp60Patch* source = nullptr;
        ToneIndex tone = ToneIndex::tone1();
        for (const auto& patch : m_patches) {
            for (const auto index : ToneIndex::all()) {
                if (patch.wave(index).groupTypeRaw == 2) {
                    source = &patch;
                    tone = index;
                    break;
                }
            }
            if (source != nullptr) {
                break;
            }
        }
        QVERIFY2(source != nullptr, "the fixture has expansion references; see Phase 7");

        PatchCopyRequest request;
        request.component = PatchComponent::Wave;
        request.fromTone = tone;
        request.toTone = ToneIndex::tone1();
        const auto result = PatchComponentCopy::apply(m_patches[0], *source, request);
        QVERIFY(result.ok);
        QVERIFY2(std::any_of(result.notes.begin(), result.notes.end(),
                             [](const std::string& note) {
                                 return note.find("expansion group") != std::string::npos;
                             }),
                 "a wave that needs a board must say so when it moves to another Patch");
    }

    void aToneSwitchThatChangesStateIsCalledOut()
    {
        // Turn the destination's Tone 4 off and the source's on, so copying the
        // whole Tone switches it back on.
        auto destination = m_patches[0];
        auto source = m_patches[0];
        QVERIFY(destination.setRaw(ToneIndex::tone4(), ToneParameter::ToneSwitch, 0));
        QVERIFY(source.setRaw(ToneIndex::tone4(), ToneParameter::ToneSwitch, 1));

        PatchCopyRequest request;
        request.component = PatchComponent::WholeTone;
        request.fromTone = ToneIndex::tone4();
        request.toTone = ToneIndex::tone4();
        const auto result = PatchComponentCopy::apply(destination, source, request);

        QVERIFY(result.ok);
        QVERIFY(result.patch->toneEnabled(ToneIndex::tone4()));
        QVERIFY(std::any_of(result.notes.begin(), result.notes.end(), [](const std::string& note) {
            return note.find("now on") != std::string::npos;
        }));
    }

    void copyingStructureSaysItBroughtNoTones()
    {
        PatchCopyRequest request;
        request.component = PatchComponent::Structure;
        const auto result = PatchComponentCopy::apply(m_patches[1], m_patches[0], request);

        QVERIFY(result.ok);
        QVERIFY(std::any_of(result.notes.begin(), result.notes.end(), [](const std::string& note) {
            return note.find("did not bring") != std::string::npos;
        }));
    }

    // --- across the whole bank ---------------------------------------------

    void everyComponentCopiesBetweenEveryPairWithoutRefusing()
    {
        // Real Patches, real values: if any component of any of these can be
        // rejected by the destination, that is a fact worth failing on.
        for (const auto component : allPatchComponents()) {
            for (std::size_t i = 0; i + 1 < 24; ++i) {
                PatchCopyRequest request;
                request.component = component;
                request.fromTone = ToneIndex::tone1();
                request.toTone = ToneIndex::tone2();
                const auto result
                    = PatchComponentCopy::apply(m_patches[i + 1], m_patches[i], request);
                QVERIFY2(result.ok, result.reason.c_str());
                QVERIFY(result.parametersCopied > 0);
                QVERIFY(result.parametersChanged <= result.parametersCopied);
            }
        }
    }

private:
    std::vector<Xp60Patch> m_patches;
};

QTEST_MAIN(PatchComponentCopyTest)
#include "tst_patch_component_copy.moc"
