#include "xp60/Xp60Device.h"
#include "xpmodel/SysExStream.h"
#include "xpmodel/Xp60PatchCodec.h"
#include "xpmodel/Xp60PatchDiff.h"

#include <QtTest>

#include <fstream>

using namespace xp60studio;
using namespace xp60studio::roland;
using namespace xp60studio::xpmodel;

namespace {

const RolandAddress kBase = Xp60PatchLayout::temporaryPatchAddress();

// Two real patches from the golden fixture make better diff subjects than
// synthetic ones: their differences are the differences of actual sounds.
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

class PatchDiffTest : public QObject
{
    Q_OBJECT

private slots:
    void identicalPatchesHaveNoDifferences()
    {
        const auto image = fixtureImage();
        const auto patch = userPatch(image, 1);
        const auto diff = Xp60PatchDiff::compare(patch, patch);
        QVERIFY(diff.identical());
        QCOMPARE(diff.count(), std::size_t(0));
        QCOMPARE(diff.summary(), std::string("identical"));
        QCOMPARE(diff.describe(), std::string("identical"));
        QVERIFY(diff.forBlock("Patch Common").empty());
    }

    void singleParameterChangeIsPinpointed()
    {
        const auto image = fixtureImage();
        auto patch = userPatch(image, 1);
        const auto original = patch;
        const int before = patch.raw(ToneIndex::tone2(), ToneParameter::CutoffFrequency);
        QVERIFY(patch.setRaw(ToneIndex::tone2(), ToneParameter::CutoffFrequency, before == 127 ? 126 : before + 1));

        const auto diff = Xp60PatchDiff::compare(original, patch);
        QVERIFY(!diff.identical());
        QCOMPARE(diff.count(), std::size_t(1));
        const auto& d = diff.differences()[0];
        QCOMPARE(d.block, std::string("Tone 2"));
        QCOMPARE(d.tone->number(), 2);
        QCOMPARE(d.parameterName, std::string("Cutoff Frequency"));
        QCOMPARE(d.category, std::string("TVF"));
        QCOMPARE(d.blockOffset, 0x51u);
        // Tone 2 starts at 12 00 = 2304 bytes into the Patch.
        QCOMPARE(d.patchOffset, 2304u + 0x51u);
        QCOMPARE(d.leftRaw, before);
        QVERIFY(d.leftRaw != d.rightRaw);
        QVERIFY(diff.summary().find("1 parameter differs") != std::string::npos);
        QVERIFY(diff.summary().find("Tone 2 1") != std::string::npos);
        QVERIFY(diff.describe().find("Cutoff Frequency") != std::string::npos);
        QCOMPARE(diff.forBlock("Tone 2").size(), std::size_t(1));
        QVERIFY(diff.forBlock("Patch Common").empty());
    }

    void differencesAreReportedInBlockOrderWithDisplayText()
    {
        const auto image = fixtureImage();
        auto patch = userPatch(image, 1);
        const auto original = patch;
        // Pick values that certainly differ from what this patch already holds,
        // so the test does not depend on the fixture's contents.
        QVERIFY(patch.setRaw(CommonParameter::ReverbType,
                             original.raw(CommonParameter::ReverbType) == 4 ? 5 : 4));
        QVERIFY(patch.setRaw(ToneIndex::tone1(), ToneParameter::TonePan,
                             original.raw(ToneIndex::tone1(), ToneParameter::TonePan) == 0 ? 127 : 0));
        QVERIFY(patch.setRaw(ToneIndex::tone4(), ToneParameter::FilterType,
                             original.raw(ToneIndex::tone4(), ToneParameter::FilterType) == 1 ? 2 : 1));

        const auto diff = Xp60PatchDiff::compare(original, patch);
        QCOMPARE(diff.count(), std::size_t(3));
        // Reported in layout order: Common, then Tones.
        QCOMPARE(diff.differences()[0].block, std::string("Patch Common"));
        QCOMPARE(diff.differences()[1].block, std::string("Tone 1"));
        QCOMPARE(diff.differences()[2].block, std::string("Tone 4"));
        // Display text, not bare numbers: enum labels and pan notation.
        QVERIFY(diff.differences()[0].rightText == std::string("HALL1")
                || diff.differences()[0].rightText == std::string("HALL2"));
        QVERIFY(diff.differences()[1].rightText == std::string("L64")
                || diff.differences()[1].rightText == std::string("63R"));
        QVERIFY(diff.differences()[2].rightText == std::string("LPF")
                || diff.differences()[2].rightText == std::string("BPF"));
        QVERIFY(diff.summary().find("3 parameters differ") != std::string::npos);
    }

    void twoRealPatchesDifferInManyParameters()
    {
        const auto image = fixtureImage();
        const auto diff = Xp60PatchDiff::compare(userPatch(image, 1), userPatch(image, 4));
        QVERIFY(!diff.identical());
        QVERIFY(diff.count() > 10);
        // The name alone accounts for several Common differences.
        QVERIFY(!diff.forBlock("Patch Common").empty());
        // describe() truncates rather than dumping hundreds of lines.
        const auto text = diff.describe(5);
        QCOMPARE(text.find("... and ") != std::string::npos, diff.count() > 5);
        QVERIFY(std::count(text.begin(), text.end(), '\n') <= 6);
    }
};

QTEST_APPLESS_MAIN(PatchDiffTest)
#include "tst_patch_diff.moc"
