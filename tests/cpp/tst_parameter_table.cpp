#include "xpmodel/ParameterTable.h"

#include <QtTest>

#include <array>

using namespace xp60studio;
using namespace xp60studio::xpmodel;

namespace {

constexpr std::array<std::string_view, 3> kWaveGroupLabels{"INT", "PCM", "EXP"};

const std::array<ParameterDescriptor, 4> kGood{{
    {"name.1", "Name 1", 0, ParameterEncoding::Ascii, 1, 0x20, 0x7E, 0, {}, {}, "Name", xp60::VerificationStatus::DocumentationDerived, ""},
    {"wave.group", "Wave Group", 1, ParameterEncoding::SevenBit, 1, 0, 2, 0, {}, kWaveGroupLabels, "Wave", xp60::VerificationStatus::DocumentationDerived, ""},
    {"wave.number", "Wave Number", 2, ParameterEncoding::Nibble, 2, 0, 254, 1, {}, {}, "Wave", xp60::VerificationStatus::DocumentationDerived, ""},
    {"pitch.coarse", "Coarse Tune", 6, ParameterEncoding::SevenBit, 1, 16, 112, -64, "semitone", {}, "Pitch", xp60::VerificationStatus::DocumentationDerived, ""},
}};

} // namespace

class ParameterTableTest : public QObject
{
    Q_OBJECT

private slots:
    void validTableHasNoIssues()
    {
        const ParameterTable table("Test", 8, kGood, TableCompleteness::Complete);
        QVERIFY(table.validate().empty());
        QCOMPARE(table.size(), std::size_t(4));
        QCOMPARE(table.describedByteCount(), 5u);
        QVERIFY(table.isComplete());
        QVERIFY(table.find("wave.number") != nullptr);
        QVERIFY(table.find("nope") == nullptr);
        QCOMPARE(table.indexOf("pitch.coarse").value(), std::size_t(3));
        QCOMPARE(table.atOffset(3)->id, std::string_view("wave.number")); // second nibble byte
        QVERIFY(table.atOffset(4) == nullptr);                              // reserved
        QVERIFY(table.atOffset(99) == nullptr);
    }

    void reservedRangesAreTheGaps()
    {
        const ParameterTable table("Test", 8, kGood, TableCompleteness::Complete);
        const auto gaps = table.reservedRanges();
        QCOMPARE(gaps.size(), std::size_t(2));
        QCOMPARE(gaps[0], ParameterTable::ByteRange(4, 6));
        QCOMPARE(gaps[1], ParameterTable::ByteRange(7, 8));
    }

    void descriptorHelpers()
    {
        const auto& coarse = kGood[3];
        QCOMPARE(coarse.toDisplay(64), 0);
        QCOMPARE(coarse.toDisplay(16), -48);
        QCOMPARE(coarse.fromDisplay(48), 112);
        QVERIFY(coarse.isRawInRange(16));
        QVERIFY(!coarse.isRawInRange(15));
        QCOMPARE(kGood[2].encodingMaximum(), 255);
        QCOMPARE(kGood[1].encodingMaximum(), 127);
        QCOMPARE(kGood[1].label(2).value(), std::string_view("EXP"));
        QVERIFY(!kGood[1].label(3).has_value());
        QVERIFY(!kGood[2].label(0).has_value());
        QVERIFY(kGood[0].isText());
        QVERIFY(kGood[1].isEnumeration());
        QCOMPARE(QString::fromUtf8(parameterEncodingName(ParameterEncoding::Nibble).data()), QStringLiteral("Nibble"));
    }

    void descriptorSelfCheckCatchesStructuralMistakes()
    {
        ParameterDescriptor d = kGood[1];
        QVERIFY(!d.selfCheck().has_value());
        d.byteCount = 2; // SevenBit must be one byte
        QVERIFY(d.selfCheck().has_value());

        d = kGood[2];
        d.byteCount = 3; // nibbles are 2 or 4 bytes
        QVERIFY(d.selfCheck().has_value());
        d.byteCount = 2;
        d.rawMax = 300; // exceeds 8 bits
        QVERIFY(d.selfCheck().has_value());

        d = kGood[3];
        d.rawMin = 100;
        d.rawMax = 50;
        QVERIFY(d.selfCheck().has_value());

        d = kGood[1];
        d.rawMax = 5; // 3 labels for 6 values
        QVERIFY(d.selfCheck().has_value());

        d = kGood[0];
        d.id = "";
        QVERIFY(d.selfCheck().has_value());
    }

    void tableValidationReportsEveryProblemKind()
    {
        static const std::array<ParameterDescriptor, 4> bad{{
            {"a", "A", 0, ParameterEncoding::SevenBit, 1, 0, 127, 0, {}, {}, "", xp60::VerificationStatus::Unknown, ""},
            {"b", "B", 0, ParameterEncoding::SevenBit, 1, 0, 127, 0, {}, {}, "", xp60::VerificationStatus::Unknown, ""}, // overlaps a
            {"a", "A2", 3, ParameterEncoding::Nibble, 2, 0, 255, 0, {}, {}, "", xp60::VerificationStatus::Unknown, ""},   // duplicate id, out of block
            {"c", "C", 2, ParameterEncoding::SevenBit, 3, 0, 127, 0, {}, {}, "", xp60::VerificationStatus::Unknown, ""},  // unsorted + descriptor
        }};
        const ParameterTable table("Bad", 4, bad, TableCompleteness::Partial);
        const auto issues = table.validate();
        QVERIFY(!issues.empty());
        auto has = [&](ParameterTable::Issue::Kind kind) {
            return std::any_of(issues.begin(), issues.end(), [&](const auto& i) { return i.kind == kind; });
        };
        QVERIFY(has(ParameterTable::Issue::Kind::Overlap));
        QVERIFY(has(ParameterTable::Issue::Kind::DuplicateId));
        QVERIFY(has(ParameterTable::Issue::Kind::OutOfBlock));
        QVERIFY(has(ParameterTable::Issue::Kind::Unsorted));
        QVERIFY(has(ParameterTable::Issue::Kind::Descriptor));
        for (const auto& issue : issues) {
            QVERIFY(!issue.detail.empty());
            QVERIFY(!tableIssueKindName(issue.kind).empty());
        }
        QCOMPARE(QString::fromUtf8(tableCompletenessName(table.completeness()).data()), QStringLiteral("Partial"));
    }
};

QTEST_APPLESS_MAIN(ParameterTableTest)
#include "tst_parameter_table.moc"
