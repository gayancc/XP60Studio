#include "support/FakeXp60.h"
#include "xpmodel/Xp60PatchRouting.h"

#include <QtTest>
#include <algorithm>

using namespace xp60studio::xpmodel;
using namespace xp60studio::testsupport;
using N = RoutingNode;

namespace {
Xp60Patch patch()
{
    auto p = patchFrom(temporaryAreaWith(4), temporaryPatchAddress());
    p.setRaw(CommonParameter::StructureType12, 0);
    p.setRaw(CommonParameter::StructureType34, 0);
    p.setRaw(CommonParameter::EfxOutputAssign, 0);
    p.setRaw(CommonParameter::EfxMixOutSendLevel, 90);
    p.setRaw(CommonParameter::EfxChorusSendLevel, 40);
    p.setRaw(CommonParameter::EfxReverbSendLevel, 50);
    p.setRaw(CommonParameter::ChorusOutput, 0);
    p.setRaw(CommonParameter::ChorusLevel, 80);
    p.setRaw(CommonParameter::ReverbLevel, 70);
    for (auto tone : ToneIndex::all()) {
        p.setRaw(tone, ToneParameter::OutputAssign, 0);
        p.setRaw(tone, ToneParameter::MixEfxSendLevel, 100);
        p.setRaw(tone, ToneParameter::ChorusSendLevel, 20);
        p.setRaw(tone, ToneParameter::ReverbSendLevel, 30);
    }
    return p;
}
const RoutingEdge* edge(const PatchRouting& r, N from, N to)
{
    const auto it = std::find_if(r.edges.begin(), r.edges.end(), [=](const auto& e) { return e.from == from && e.to == to; });
    return it == r.edges.end() ? nullptr : &*it;
}
}

class PatchRoutingTest : public QObject {
    Q_OBJECT
private slots:
    void mixUsesParallelSendsWithoutEfx()
    {
        const auto r = patchRouting(patch(), ToneIndex::tone1());
        QCOMPARE(r.edges.size(), 5u);
        QVERIFY(edge(r, N::Source, N::Mix)->open);
        QCOMPARE(edge(r, N::Source, N::Chorus)->level, 20);
        QCOMPARE(edge(r, N::Source, N::Reverb)->level, 30);
        QVERIFY(!edge(r, N::Source, N::Efx));
        QVERIFY(!edge(r, N::Chorus, N::Reverb));
    }
    void directIgnoresToneEffectSends()
    {
        auto p = patch();
        p.setRaw(ToneIndex::tone1(), ToneParameter::OutputAssign, 2);
        const auto r = patchRouting(p, ToneIndex::tone1());
        QCOMPARE(r.edges.size(), 1u);
        QCOMPARE(edge(r, N::Source, N::Direct)->level, 100);
    }
    void efxDirectKeepsIndependentToneSends()
    {
        auto p = patch();
        p.setRaw(ToneIndex::tone1(), ToneParameter::OutputAssign, 1);
        p.setRaw(CommonParameter::EfxOutputAssign, 1);
        const auto r = patchRouting(p, ToneIndex::tone1());
        QVERIFY(edge(r, N::Source, N::Efx)->open);
        QCOMPARE(edge(r, N::Efx, N::Direct)->level, 90);
        QVERIFY(edge(r, N::Source, N::Chorus)->open);
        QVERIFY(edge(r, N::Source, N::Reverb)->open);
        QVERIFY(!edge(r, N::Efx, N::Chorus));
        QVERIFY(!edge(r, N::Efx, N::Reverb));
    }
    void serialAndParallelExamplesFollowManualPage62()
    {
        auto p = patch();
        p.setRaw(ToneIndex::tone1(), ToneParameter::OutputAssign, 1);
        p.setRaw(ToneIndex::tone1(), ToneParameter::ChorusSendLevel, 0);
        p.setRaw(ToneIndex::tone1(), ToneParameter::ReverbSendLevel, 0);
        p.setRaw(CommonParameter::EfxReverbSendLevel, 0);
        p.setRaw(CommonParameter::ChorusOutput, 1);
        auto r = patchRouting(p, ToneIndex::tone1());
        QVERIFY(edge(r, N::Efx, N::Chorus)->open);
        QVERIFY(edge(r, N::Chorus, N::Reverb)->open);
        QVERIFY(edge(r, N::Reverb, N::Mix)->open);
        QVERIFY(!edge(r, N::Chorus, N::Mix));
        QVERIFY(!edge(r, N::Efx, N::Reverb)->open);
        p.setRaw(CommonParameter::ChorusOutput, 2);
        r = patchRouting(p, ToneIndex::tone1());
        QVERIFY(edge(r, N::Chorus, N::Mix)->open);
        QVERIFY(edge(r, N::Chorus, N::Reverb)->open);
        // A zero upstream send must dim downstream paths despite stored levels.
        p.setRaw(ToneIndex::tone1(), ToneParameter::MixEfxSendLevel, 0);
        r = patchRouting(p, ToneIndex::tone1());
        QVERIFY(std::none_of(r.edges.begin(), r.edges.end(), [](const auto& e) { return e.open; }));
        QCOMPARE(edge(r, N::Reverb, N::Mix)->level, 70);
    }
    void everyStructureResolvesBothPairOwners()
    {
        auto p = patch();
        p.setRaw(ToneIndex::tone2(), ToneParameter::OutputAssign, 2);
        p.setRaw(ToneIndex::tone4(), ToneParameter::OutputAssign, 2);
        for (int type = 0; type < 10; ++type) {
            p.setRaw(CommonParameter::StructureType12, type);
            p.setRaw(CommonParameter::StructureType34, type);
            for (auto selected : ToneIndex::all()) {
                const auto r = patchRouting(p, selected);
                const int owner = type == 0 ? selected.number() : selected.number() <= 2 ? 2 : 4;
                QCOMPARE(r.outputTone, owner);
                QCOMPARE(r.combined, type != 0);
                QCOMPARE(r.structureType, type + 1);
                if (owner % 2 == 0) {
                    QCOMPARE(r.edges.size(), 1u);
                    QVERIFY(edge(r, N::Source, N::Direct));
                } else QVERIFY(edge(r, N::Source, N::Mix));
            }
        }
    }
    void undocumentedDestinationsAreNeverGuessed()
    {
        auto p = patch();
        QVERIFY(p.setRaw(ToneIndex::tone1(), ToneParameter::OutputAssign, 3));
        auto r = patchRouting(p, ToneIndex::tone1());
        QVERIFY(r.unknown);
        QCOMPARE(r.edges.size(), 1u);
        QVERIFY(edge(r, N::Source, N::Unknown));
        QVERIFY(!edge(r, N::Source, N::Direct));
        p.setRaw(ToneIndex::tone1(), ToneParameter::OutputAssign, 1);
        QVERIFY(p.setRaw(CommonParameter::EfxOutputAssign, 2));
        r = patchRouting(p, ToneIndex::tone1());
        QVERIFY(r.unknown);
        QVERIFY(edge(r, N::Efx, N::Unknown));
        QVERIFY(edge(r, N::Source, N::Chorus));
        QVERIFY(!edge(r, N::Efx, N::Mix));
    }
};
QTEST_GUILESS_MAIN(PatchRoutingTest)
#include "tst_patch_routing.moc"
