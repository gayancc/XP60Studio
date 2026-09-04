#include "xpmodel/BlockCodec.h"
#include "xpmodel/PatchName.h"
#include "xpmodel/Xp60PatchLayout.h"

#include <QtTest>

using namespace xp60studio;
using namespace xp60studio::roland;
using namespace xp60studio::xpmodel;

class PatchLayoutTest : public QObject
{
    Q_OBJECT

private slots:
    void patchNameFromBytesAndText()
    {
        const ByteVector bytes{'W', 'a', 'r', 'm', ' ', 'O', 'r', 'c', 'h', 'e', 's', 't'};
        const auto name = PatchName::fromBytes(bytes);
        QVERIFY(name.has_value());
        QCOMPARE(name->text(), std::string("Warm Orchest"));
        QCOMPARE(name->paddedText().size(), std::size_t(12));
        QVERIFY(!name->isBlank());

        const auto padded = PatchName::fromText("Piano");
        QVERIFY(padded.has_value());
        QCOMPARE(padded->text(), std::string("Piano"));
        QCOMPARE(padded->paddedText(), std::string("Piano       "));
        QCOMPARE(padded->bytes()[11], Byte(0x20));

        QVERIFY(PatchName().isBlank());
        QCOMPARE(PatchName().text(), std::string());
        QVERIFY(PatchName::fromText("") == PatchName());
        QCOMPARE(PatchName::fromText("XP60STUDIO  ")->text(), std::string("XP60STUDIO"));
    }

    void patchNameRejectsBadInput()
    {
        QVERIFY(!PatchName::fromText("thirteen chars").has_value());
        QVERIFY(!PatchName::fromText("tab\tname").has_value());
        QVERIFY(!PatchName::fromText(std::string_view("\x7F", 1)).has_value());
        QVERIFY(!PatchName::fromBytes(ByteVector(11, 'a')).has_value());
        QVERIFY(!PatchName::fromBytes(ByteVector(13, 'a')).has_value());
        ByteVector withHighBit(12, 'a');
        withHighBit[3] = 0xC4;
        QVERIFY(!PatchName::fromBytes(withHighBit).has_value());
        ByteVector withControl(12, 'a');
        withControl[0] = 0x0A;
        QVERIFY(!PatchName::fromBytes(withControl).has_value());
    }

    void knownPrefixTableDecodesNamesThroughTheGenericCodec()
    {
        const auto& table = Xp60PatchLayout::patchCommonKnownPrefix();
        QVERIFY(table.validate().empty());
        QVERIFY(!table.isComplete());
        QCOMPARE(table.completeness(), TableCompleteness::Partial);
        QCOMPARE(table.size(), std::size_t(12));
        QCOMPARE(table.blockSize(), 12u);
        QVERIFY(table.reservedRanges().empty());
        QVERIFY(!table.sourceNote().empty());
        for (const auto& parameter : table.parameters()) {
            QCOMPARE(parameter.status, xp60::VerificationStatus::DocumentationDerived);
            QVERIFY(parameter.isText());
        }

        const ByteVector bytes{'W', 'a', 'r', 'm', ' ', 'O', 'r', 'c', 'h', 'e', 's', 't'};
        const auto decoded = BlockCodec::decode(table, bytes);
        QVERIFY(decoded.ok());
        QCOMPARE(decoded.values->text("common.name."), std::string("Warm Orchest"));
        QCOMPARE(decoded.values->raw("common.name.1").value(), int('W'));
        QCOMPARE(BlockCodec::encode(*decoded.values), bytes);
    }

    void layoutDeclaresItsGapsExplicitly()
    {
        QVERIFY(!Xp60PatchLayout::isComplete());
        QVERIFY(!Xp60PatchLayout::patchCommonSize().has_value());
        QVERIFY(!Xp60PatchLayout::toneSize().has_value());
        QVERIFY(!Xp60PatchLayout::patchSize().has_value());
        for (int tone = 1; tone <= Xp60PatchLayout::kToneCount; ++tone) {
            QVERIFY(!Xp60PatchLayout::toneOffset(tone).has_value());
        }
        QVERIFY(Xp60PatchLayout::missingInputs().find("Parameter Address Map") != std::string_view::npos);
    }

    void documentedAddresses()
    {
        QCOMPARE(QString::fromStdString(Xp60PatchLayout::temporaryPatchAddress().toHexString()), QStringLiteral("03 00 00 00"));
        QCOMPARE(QString::fromStdString(Xp60PatchLayout::userPatchAddress(1)->toHexString()), QStringLiteral("11 00 00 00"));
        QCOMPARE(QString::fromStdString(Xp60PatchLayout::userPatchAddress(2)->toHexString()), QStringLiteral("11 01 00 00"));
        QCOMPARE(QString::fromStdString(Xp60PatchLayout::userPatchAddress(128)->toHexString()), QStringLiteral("11 7F 00 00"));
        QVERIFY(!Xp60PatchLayout::userPatchAddress(0).has_value());
        QVERIFY(!Xp60PatchLayout::userPatchAddress(129).has_value());
        QCOMPARE(Xp60PatchLayout::kUserPatchStride, 16384u);
    }

    void namesFromImage()
    {
        MemoryImage image;
        const ByteVector good{'P', 'i', 'a', 'n', 'o', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
        image.write(Xp60PatchLayout::temporaryPatchAddress(), good);
        QCOMPARE(Xp60PatchLayout::readTemporaryPatchName(image)->text(), std::string("Piano"));

        // Partial coverage yields no name rather than a guess.
        image.write(*Xp60PatchLayout::userPatchAddress(5), ByteVector(6, 'x'));
        QVERIFY(!Xp60PatchLayout::readPatchName(image, *Xp60PatchLayout::userPatchAddress(5)).has_value());

        // Non-printable bytes in a name slot are rejected rather than shown.
        ByteVector bad(12, 0x01);
        image.write(*Xp60PatchLayout::userPatchAddress(7), bad);
        QVERIFY(!Xp60PatchLayout::readPatchName(image, *Xp60PatchLayout::userPatchAddress(7)).has_value());

        const auto names = Xp60PatchLayout::readUserPatchNames(image);
        QCOMPARE(names.size(), std::size_t(128));
        QCOMPARE(std::count_if(names.begin(), names.end(), [](const auto& e) { return e.name.has_value(); }), 0);
    }
};

QTEST_APPLESS_MAIN(PatchLayoutTest)
#include "tst_patch_layout.moc"
