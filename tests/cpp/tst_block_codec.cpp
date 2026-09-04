#include "roland/HexFormat.h"
#include "xpmodel/BlockCodec.h"

#include <QtTest>

#include <array>
#include <random>

using namespace xp60studio;
using namespace xp60studio::roland;
using namespace xp60studio::xpmodel;

namespace {

constexpr std::array<std::string_view, 2> kSwitchLabels{"OFF", "ON"};

// Synthetic 16-byte block exercising every encoding plus reserved gaps:
//   0..3  name (Ascii)
//   4     switch (SevenBit, enum)
//   5..6  wave number (Nibble x2, 0..254)
//   7     reserved
//   8..11 tempo (Nibble x4, 20..250)
//   12    coarse tune (SevenBit 16..112, display -48..+48)
//   13..15 reserved
const std::array<ParameterDescriptor, 7> kParams{{
    {"name.1", "Name 1", 0, ParameterEncoding::Ascii, 1, 0x20, 0x7E, 0, {}, {}, "Name", xp60::VerificationStatus::DocumentationDerived, ""},
    {"name.2", "Name 2", 1, ParameterEncoding::Ascii, 1, 0x20, 0x7E, 0, {}, {}, "Name", xp60::VerificationStatus::DocumentationDerived, ""},
    {"name.3", "Name 3", 2, ParameterEncoding::Ascii, 1, 0x20, 0x7E, 0, {}, {}, "Name", xp60::VerificationStatus::DocumentationDerived, ""},
    {"name.4", "Name 4", 3, ParameterEncoding::Ascii, 1, 0x20, 0x7E, 0, {}, {}, "Name", xp60::VerificationStatus::DocumentationDerived, ""},
    {"tone.switch", "Tone Switch", 4, ParameterEncoding::SevenBit, 1, 0, 1, 0, {}, kSwitchLabels, "Tone", xp60::VerificationStatus::DocumentationDerived, ""},
    {"wave.number", "Wave Number", 5, ParameterEncoding::Nibble, 2, 0, 254, 1, {}, {}, "Wave", xp60::VerificationStatus::DocumentationDerived, ""},
    {"tempo", "Tempo", 8, ParameterEncoding::Nibble, 4, 20, 250, 0, "bpm", {}, "Common", xp60::VerificationStatus::DocumentationDerived, ""},
}};
const std::array<ParameterDescriptor, 8> kParamsAll{{
    kParams[0], kParams[1], kParams[2], kParams[3], kParams[4], kParams[5], kParams[6],
    {"pitch.coarse", "Coarse Tune", 12, ParameterEncoding::SevenBit, 1, 16, 112, -64, "semitone", {}, "Pitch", xp60::VerificationStatus::DocumentationDerived, ""},
}};
const ParameterTable kTable("Synthetic", 16, kParamsAll, TableCompleteness::Complete);

ByteVector sampleBlock()
{
    // "Warm" | ON | wave 0x8B (139) -> 08 0B | reserved 0x55 | tempo 120 = 0x0078 -> 00 00 07 08 | coarse 64 | reserved 7F 01 02
    return ByteVector{'W', 'a', 'r', 'm', 0x01, 0x08, 0x0B, 0x55, 0x00, 0x00, 0x07, 0x08, 0x40, 0x7F, 0x01, 0x02};
}

} // namespace

class BlockCodecTest : public QObject
{
    Q_OBJECT

private slots:
    void decodeReadsEveryEncoding()
    {
        const auto result = BlockCodec::decode(kTable, sampleBlock());
        QVERIFY2(result.ok(), result.issues.empty() ? "" : result.issues[0].detail.c_str());
        QVERIFY(result.issues.empty());
        const auto& values = *result.values;
        QCOMPARE(values.text("name."), std::string("Warm"));
        QCOMPARE(values.raw("tone.switch").value(), 1);
        QCOMPARE(values.label("tone.switch").value(), std::string_view("ON"));
        QCOMPARE(values.raw("wave.number").value(), 139);
        QCOMPARE(values.display("wave.number").value(), 140); // 1-based display
        QCOMPARE(values.raw("tempo").value(), 120);
        QCOMPARE(values.raw("pitch.coarse").value(), 64);
        QCOMPARE(values.display("pitch.coarse").value(), 0);
        QVERIFY(!values.raw("missing").has_value());
        QVERIFY(!values.display("missing").has_value());
        QVERIFY(!values.label("tempo").has_value());
    }

    void encodeDecodeRoundTripPreservesReservedBytes()
    {
        const auto block = sampleBlock();
        const auto decoded = BlockCodec::decode(kTable, block);
        QVERIFY(decoded.ok());
        QCOMPARE(BlockCodec::encode(*decoded.values), block);

        // Modify parameters; reserved bytes 7, 13, 14, 15 must survive.
        auto values = *decoded.values;
        QVERIFY(values.setRaw("wave.number", 254));
        QVERIFY(values.setDisplay("pitch.coarse", 12));
        QVERIFY(values.setRaw("tempo", 250));
        QVERIFY(values.setRaw("name.1", 'C'));
        const auto encoded = BlockCodec::encode(values);
        QCOMPARE(encoded[7], Byte(0x55));
        QCOMPARE(encoded[13], Byte(0x7F));
        QCOMPARE(encoded[14], Byte(0x01));
        QCOMPARE(encoded[15], Byte(0x02));
        QCOMPARE(encoded[5], Byte(0x0F));
        QCOMPARE(encoded[6], Byte(0x0E));
        QCOMPARE(encoded[12], Byte(76));
        QCOMPARE(encoded[0], Byte('C'));
        // 250 = 0x00FA -> 00 00 0F 0A
        QCOMPARE(QString::fromStdString(toHex(ByteSpan(encoded.data() + 8, 4))), QStringLiteral("00 00 0F 0A"));

        const auto again = BlockCodec::decode(kTable, encoded);
        QVERIFY(again.ok());
        QCOMPARE(*again.values, values);
    }

    void setRejectsOutOfRangeWithoutClamping()
    {
        auto values = *BlockCodec::decode(kTable, sampleBlock()).values;
        QVERIFY(!values.setRaw("wave.number", 255));
        QCOMPARE(values.raw("wave.number").value(), 139);
        QVERIFY(!values.setRaw("pitch.coarse", 15));
        QVERIFY(!values.setDisplay("pitch.coarse", 49));
        QVERIFY(values.setDisplay("pitch.coarse", 48));
        QVERIFY(!values.setRaw("missing", 1));
        QVERIFY(!values.setRawAt(99, 1));
    }

    void outOfRangeValuesAreWarningsAndRoundTrip()
    {
        auto block = sampleBlock();
        block[12] = 0x05; // coarse below documented minimum 16
        const auto result = BlockCodec::decode(kTable, block);
        QVERIFY(result.ok());
        QVERIFY(result.hasWarnings());
        QCOMPARE(result.errorCount(), std::size_t(0));
        QCOMPARE(result.issues.size(), std::size_t(1));
        QCOMPARE(result.issues[0].kind, BlockIssueKind::OutOfRange);
        QCOMPARE(result.issues[0].parameterId, std::string("pitch.coarse"));
        QCOMPARE(result.issues[0].value, 5);
        QCOMPARE(result.values->raw("pitch.coarse").value(), 5); // kept verbatim, never clamped
        QCOMPARE(BlockCodec::encode(*result.values), block);
    }

    void sizeMismatchIsAnError()
    {
        auto block = sampleBlock();
        block.pop_back();
        const auto result = BlockCodec::decode(kTable, block);
        QVERIFY(!result.ok());
        QCOMPARE(result.errorCount(), std::size_t(1));
        QCOMPARE(result.issues[0].kind, BlockIssueKind::SizeMismatch);
        QVERIFY(result.issues[0].detail.find("15") != std::string::npos);
    }

    void bit7AndNibbleViolationsAreErrors()
    {
        auto block = sampleBlock();
        block[4] = 0x81;
        auto result = BlockCodec::decode(kTable, block);
        QVERIFY(!result.ok());
        QCOMPARE(result.issues[0].kind, BlockIssueKind::DataByteBit7);
        QCOMPARE(result.issues[0].parameterId, std::string("tone.switch"));
        QCOMPARE(result.issues[0].offset, 4u);

        block = sampleBlock();
        block[6] = 0x1B; // bit 4 set inside a nibble byte: cannot round-trip
        result = BlockCodec::decode(kTable, block);
        QVERIFY(!result.ok());
        QCOMPARE(result.issues[0].kind, BlockIssueKind::InvalidNibbleByte);
        QCOMPARE(result.issues[0].parameterId, std::string("wave.number"));
        QCOMPARE(result.issues[0].offset, 6u);
        QVERIFY(isBlockIssueError(BlockIssueKind::InvalidNibbleByte));
        QVERIFY(!isBlockIssueError(BlockIssueKind::OutOfRange));
    }

    void invalidTableIsReportedNotUsed()
    {
        static const std::array<ParameterDescriptor, 2> overlapping{{
            {"a", "A", 0, ParameterEncoding::SevenBit, 1, 0, 127, 0, {}, {}, "", xp60::VerificationStatus::Unknown, ""},
            {"b", "B", 0, ParameterEncoding::SevenBit, 1, 0, 127, 0, {}, {}, "", xp60::VerificationStatus::Unknown, ""},
        }};
        const ParameterTable bad("Bad", 2, overlapping, TableCompleteness::Complete);
        const auto result = BlockCodec::decode(bad, ByteVector{0, 0});
        QVERIFY(!result.ok());
        QCOMPARE(result.issues[0].kind, BlockIssueKind::TableInvalid);
        QCOMPARE(QString::fromUtf8(blockIssueKindName(result.issues[0].kind).data()), QStringLiteral("TableInvalid"));
    }

    void randomisedRoundTrips()
    {
        std::mt19937 rng(0x6A);
        std::uniform_int_distribution<int> dataByte(0, 127);
        std::uniform_int_distribution<int> nibbleByte(0, 15);
        std::uniform_int_distribution<int> ascii(0x20, 0x7E);
        for (int iteration = 0; iteration < 300; ++iteration) {
            ByteVector block(16);
            for (std::size_t i = 0; i < block.size(); ++i) {
                const auto* p = kTable.atOffset(static_cast<std::uint32_t>(i));
                if (p && p->encoding == ParameterEncoding::Nibble) {
                    block[i] = static_cast<Byte>(nibbleByte(rng));
                } else if (p && p->isText()) {
                    block[i] = static_cast<Byte>(ascii(rng));
                } else {
                    block[i] = static_cast<Byte>(dataByte(rng));
                }
            }
            const auto decoded = BlockCodec::decode(kTable, block);
            QVERIFY(decoded.ok()); // out-of-range values are warnings, never errors
            QCOMPARE(BlockCodec::encode(*decoded.values), block);
        }
    }

    void rawHelpers()
    {
        ByteVector block(16, 0);
        BlockCodec::writeRaw(kParamsAll[6], 0xABCD, block); // tempo: 4 nibbles
        QCOMPARE(QString::fromStdString(toHex(ByteSpan(block.data() + 8, 4))), QStringLiteral("0A 0B 0C 0D"));
        QCOMPARE(BlockCodec::readRaw(kParamsAll[6], block), 0xABCD);
        BlockCodec::writeRaw(kParamsAll[7], 0x7F, block);
        QCOMPARE(BlockCodec::readRaw(kParamsAll[7], block), 0x7F);
    }
};

QTEST_APPLESS_MAIN(BlockCodecTest)
#include "tst_block_codec.moc"
