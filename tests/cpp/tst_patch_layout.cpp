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
        // 7FH is inside Roland's documented 32..127 range: accepted, shown as '?'.
        const auto del = PatchName::fromText(std::string_view("\x7F", 1));
        QVERIFY(del.has_value());
        QCOMPARE(del->displayText(), std::string("?"));
        QCOMPARE(del->text().size(), std::size_t(1));
        QVERIFY(!PatchName::fromBytes(ByteVector(11, 'a')).has_value());
        QVERIFY(!PatchName::fromBytes(ByteVector(13, 'a')).has_value());
        ByteVector withHighBit(12, 'a');
        withHighBit[3] = 0xC4;
        QVERIFY(!PatchName::fromBytes(withHighBit).has_value());
        ByteVector withControl(12, 'a');
        withControl[0] = 0x0A;
        QVERIFY(!PatchName::fromBytes(withControl).has_value());
    }

    void commonTableDecodesNamesThroughTheGenericCodec()
    {
        const auto& table = Xp60PatchLayout::patchCommonTable();
        QVERIFY(table.validate().empty());
        QVERIFY(table.isComplete());
        QCOMPARE(table.blockSize(), 73u);
        for (std::size_t i = 0; i < PatchName::kLength; ++i) {
            const auto& parameter = table.parameters()[i];
            QVERIFY(parameter.isText());
            QCOMPARE(parameter.offset, static_cast<std::uint32_t>(i));
            QCOMPARE(parameter.rawMin, int(PatchName::kMinChar));
            QCOMPARE(parameter.rawMax, int(PatchName::kMaxChar));
        }

        ByteVector bytes(73, 0);
        const ByteVector name{'W', 'a', 'r', 'm', ' ', 'O', 'r', 'c', 'h', 'e', 's', 't'};
        std::copy(name.begin(), name.end(), bytes.begin());
        const auto decoded = BlockCodec::decode(table, bytes);
        QVERIFY(decoded.ok()); // zeros elsewhere may be out of range -> warnings only
        QCOMPARE(decoded.values->text("common.name."), std::string("Warm Orchest"));
        QCOMPARE(decoded.values->raw("common.name.1").value(), int('W'));
        QCOMPARE(BlockCodec::encode(*decoded.values), bytes);
    }

    void layoutIsComplete()
    {
        QVERIFY(Xp60PatchLayout::isComplete());
        QCOMPARE(Xp60PatchLayout::patchCommonSize(), 73u);
        QCOMPARE(Xp60PatchLayout::toneSize(), 129u);
        QCOMPARE(Xp60PatchLayout::toneOffset(ToneIndex::tone1()), 2048u);
        QCOMPARE(Xp60PatchLayout::toneOffset(ToneIndex::tone2()), 2304u);
        QCOMPARE(Xp60PatchLayout::toneOffset(ToneIndex::tone3()), 2560u);
        QCOMPARE(Xp60PatchLayout::toneOffset(ToneIndex::tone4()), 2816u);
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
