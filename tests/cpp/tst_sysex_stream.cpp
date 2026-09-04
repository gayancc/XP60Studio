#include "roland/HexFormat.h"
#include "roland/RolandCodec.h"
#include "xp60/Xp60Device.h"
#include "xpmodel/SysExStream.h"
#include "xpmodel/Xp60PatchLayout.h"

#include <QtTest>

using namespace xp60studio;
using namespace xp60studio::roland;
using namespace xp60studio::xpmodel;

namespace {

ByteVector dt1Bytes(RolandAddress address, ByteVector data)
{
    return RolandSysExMessage::dataSet(RolandDeviceId::factoryDefault(), xp60::modelId(), address, std::move(data)).value().encode();
}

ByteVector nameBytes(const char* text)
{
    const auto arr = PatchName::fromText(text).value().bytes();
    return ByteVector(arr.begin(), arr.end());
}

void append(ByteVector& out, const ByteVector& more)
{
    out.insert(out.end(), more.begin(), more.end());
}

const std::vector<RolandModelId> kModels{xp60::modelId()};

} // namespace

class SysExStreamTest : public QObject
{
    Q_OBJECT

private slots:
    void cleanStreamOfDataSets()
    {
        ByteVector stream;
        append(stream, dt1Bytes(RolandAddress(0x03, 0, 0, 0), nameBytes("Warm Orchest")));
        append(stream, dt1Bytes(RolandAddress(0x11, 0, 0, 0), nameBytes("Piano 1")));
        append(stream, dt1Bytes(RolandAddress(0x11, 0x01, 0, 0), nameBytes("Piano 2")));

        const auto result = parseSysExStream(stream, kModels);
        QVERIFY(result.isClean());
        QCOMPARE(result.items.size(), std::size_t(3));
        QCOMPARE(result.dataSetCount, std::size_t(3));
        QCOMPARE(result.requestCount, std::size_t(0));
        QCOMPARE(result.items[0].offset, std::size_t(0));
        QCOMPARE(result.items[1].offset, result.items[0].raw.size());
        QVERIFY(result.items[0].isSysEx);
        QVERIFY(result.items[0].roland.has_value());
        QVERIFY(!result.items[0].failure.has_value());

        const auto image = imageFromStream(result);
        QCOMPARE(image.byteCount(), std::size_t(36));
        QCOMPARE(Xp60PatchLayout::readTemporaryPatchName(image)->text(), std::string("Warm Orchest"));
        const auto names = Xp60PatchLayout::readUserPatchNames(image);
        QCOMPARE(names.size(), std::size_t(128));
        QCOMPARE(names[0].userNumber, 1);
        QCOMPARE(names[0].name->text(), std::string("Piano 1"));
        QCOMPARE(names[1].name->text(), std::string("Piano 2"));
        QVERIFY(!names[2].name.has_value());
        QCOMPARE(QString::fromStdString(names[127].address.toHexString()), QStringLiteral("11 7F 00 00"));
    }

    void mixedStreamReportsEverything()
    {
        ByteVector stream;
        append(stream, ByteVector{0x90, 0x3C, 0x64});                                    // note on
        append(stream, dt1Bytes(RolandAddress(0x03, 0, 0, 0), nameBytes("Good")));       // valid DT1
        append(stream, ByteVector{0xF0, 0x43, 0x10, 0x4C, 0x00, 0xF7});                  // Yamaha
        auto broken = dt1Bytes(RolandAddress(0x11, 0, 0, 0), nameBytes("Broken"));
        broken[broken.size() - 2] ^= 0x01;                                                // bad checksum
        append(stream, broken);
        append(stream, RolandSysExMessage::dataRequest(RolandDeviceId::factoryDefault(), xp60::modelId(),
                                                       RolandAddress(0x01, 0, 0, 0), RolandSize(0, 0, 0x1F, 0x19)).encode());
        append(stream, ByteVector{0xF8});                                                 // clock
        append(stream, ByteVector{0x40, 0x41});                                           // stray data bytes

        const auto result = parseSysExStream(stream, kModels);
        QVERIFY(!result.isClean());
        QCOMPARE(result.dataSetCount, std::size_t(1));
        QCOMPARE(result.requestCount, std::size_t(1));
        QCOMPARE(result.nonRolandSysExCount, std::size_t(1));
        QCOMPARE(result.rejectedRolandCount, std::size_t(1));
        QCOMPARE(result.otherMidiCount, std::size_t(2)); // note on + clock
        QCOMPARE(result.strayBytes, std::size_t(2));
        QCOMPARE(result.abortedSysEx, std::size_t(0));
        QCOMPARE(result.items.size(), std::size_t(6));

        // The rejected DT1 keeps its raw bytes and a structured failure.
        const auto rejected = std::find_if(result.items.begin(), result.items.end(),
                                           [](const auto& i) { return i.failure && isRolandSysEx(i.raw); });
        QVERIFY(rejected != result.items.end());
        QCOMPARE(rejected->failure->error, RolandParseError::InvalidChecksum);
        QCOMPARE(rejected->raw, broken);

        // Only the valid DT1 reaches the image.
        const auto image = imageFromStream(result);
        QCOMPARE(image.byteCount(), std::size_t(12));
        QCOMPARE(Xp60PatchLayout::readTemporaryPatchName(image)->text(), std::string("Good"));
        QVERIFY(!Xp60PatchLayout::readPatchName(image, RolandAddress(0x11, 0, 0, 0)).has_value());
    }

    void truncatedTrailingSysExIsCounted()
    {
        auto stream = dt1Bytes(RolandAddress(0x03, 0, 0, 0), nameBytes("Ok"));
        auto partial = dt1Bytes(RolandAddress(0x11, 0, 0, 0), nameBytes("Cut"));
        partial.resize(partial.size() - 3);
        append(stream, partial);
        const auto result = parseSysExStream(stream, kModels);
        QCOMPARE(result.dataSetCount, std::size_t(1));
        QCOMPARE(result.abortedSysEx, std::size_t(1));
        QVERIFY(!result.isClean());
    }

    void emptyStream()
    {
        const auto result = parseSysExStream(ByteVector{}, kModels);
        QVERIFY(result.isClean());
        QVERIFY(result.items.empty());
        QVERIFY(imageFromStream(result).isEmpty());
    }
};

QTEST_APPLESS_MAIN(SysExStreamTest)
#include "tst_sysex_stream.moc"
