// Phase 8 — the XP-60 System Common and Scale Tune tables.
//
// Unlike the Patch, Performance and Rhythm transcriptions, this one has no
// fixture to check against: the golden fixture is a User bank dump and carries
// no System data. So this proves what can be proved locally — that the tables
// tile the blocks Roland declares, that the layout is the seventeen Scale Tune
// blocks Roland lists rather than the one it would be easy to assume, and that
// every descriptor is self-consistent. Confirming the values themselves is
// DEVICE_ACCEPTANCE.md area 19.

#include "xpmodel/generated/Xp60SystemTables.h"

#include <QTest>

#include <set>

using namespace xp60studio;
namespace sys = xp60studio::xpmodel::xp60system;

class TestXp60System : public QObject
{
    Q_OBJECT

private slots:
    void tablesCoverTheBlocksRolandDeclares()
    {
        QCOMPARE(sys::kSystemCommonSize, 96u);
        QCOMPARE(sys::kScaleTuneSize, 12u);
        QCOMPARE(sys::systemCommonTable().blockSize(), sys::kSystemCommonSize);
        QCOMPARE(sys::scaleTuneTable().blockSize(), sys::kScaleTuneSize);
        QVERIFY(sys::systemCommonTable().validate().empty());
        QVERIFY(sys::scaleTuneTable().validate().empty());
        QCOMPARE(sys::scaleTuneTable().size(), std::size_t{12});
    }

    // Seventeen Scale Tune blocks: one per Performance Part, plus one for Patch
    // mode. Easy to assume there is a single global one, and there is not.
    void thereAreSeventeenScaleTuneBlocks()
    {
        QCOMPARE(sys::kScaleTuneBlockCount, 17);
        QCOMPARE(sys::kScaleTuneOffsets.size(), std::size_t{17});
        std::set<std::uint32_t> offsets;
        for (std::size_t i = 0; i < sys::kScaleTuneOffsets.size(); ++i) {
            QCOMPARE(sys::kScaleTuneOffsets[i], static_cast<std::uint32_t>((0x10 + i) * 128));
            QVERIFY(offsets.insert(sys::kScaleTuneOffsets[i]).second);
        }
        // The Patch-mode block is the seventeenth, at Roland 20 00.
        QCOMPARE(sys::kPatchModeScaleTuneOffset, 0x20u * 128u);
        QCOMPARE(sys::kScaleTuneOffsets.back(), sys::kPatchModeScaleTuneOffset);
        QCOMPARE(sys::kSystemSpan, sys::kPatchModeScaleTuneOffset + sys::kScaleTuneSize);
    }

    // One offset per pitch class, and the sharps must be distinguishable from
    // the naturals — "C#" pascal-cased naively collides with "C".
    void everyPitchClassHasItsOwnEnumerator()
    {
        QCOMPARE(sys::descriptor(sys::ScaleTuneParameter::ScaleTuneC).offset, 0u);
        QCOMPARE(sys::descriptor(sys::ScaleTuneParameter::ScaleTuneCSharp).offset, 1u);
        QCOMPARE(sys::descriptor(sys::ScaleTuneParameter::ScaleTuneB).offset, 11u);
        std::set<std::uint32_t> offsets;
        std::set<std::string_view> ids;
        for (const auto& parameter : sys::scaleTuneTable().parameters()) {
            QVERIFY(offsets.insert(parameter.offset).second);
            QVERIFY(ids.insert(parameter.id).second);
            // Roland's -64..+63 over raw 0..127.
            QCOMPARE(parameter.rawMin, 0);
            QCOMPARE(parameter.rawMax, 127);
            QCOMPARE(parameter.toDisplay(64), 0);
        }
    }

    // Rows whose printed enumeration is not one label per raw value must carry
    // no labels at all: naming them would mis-name every value.
    void enumerationsThatDoNotIndexCarryNoLabels()
    {
        for (const auto& parameter : sys::systemCommonTable().parameters()) {
            if (parameter.enumLabels.empty()) {
                continue;
            }
            const auto count = static_cast<std::size_t>(parameter.rawMax - parameter.rawMin + 1);
            QVERIFY2(parameter.enumLabels.size() == count,
                     qPrintable(QStringLiteral("%1 has %2 labels for %3 raw values")
                                    .arg(QString::fromUtf8(parameter.name.data(),
                                                           static_cast<qsizetype>(parameter.name.size())))
                                    .arg(parameter.enumLabels.size())
                                    .arg(count)));
        }
        // The 97- and 104-value controller assignments are the rows this rule
        // exists for: Roland prints ranges of CC numbers, not an indexed list.
        QVERIFY(sys::descriptor(sys::SystemCommonParameter::SystemControlSource1).enumLabels.empty());
        QVERIFY(sys::descriptor(sys::SystemCommonParameter::Pedal1Assign).enumLabels.empty());
        // Where the list does index, it is used.
        QCOMPARE(sys::descriptor(sys::SystemCommonParameter::SoundMode).enumLabels.size(), std::size_t{3});
        QCOMPARE(sys::descriptor(sys::SystemCommonParameter::KeyboardSens).enumLabels.size(), std::size_t{3});
    }
};

QTEST_MAIN(TestXp60System)
#include "tst_xp60_system.moc"
