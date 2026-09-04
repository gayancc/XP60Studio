#include "roland/HexFormat.h"
#include "roland/RolandChecksum.h"
#include "roland/RolandCodec.h"
#include "roland/RolandSysExMessage.h"
#include "xp60/Xp60Device.h"

#include <QtTest>

using namespace xp60studio::roland;

namespace {

ByteVector bytesOf(const char* hex)
{
    return parseHexBytes(hex).value();
}

QString hexOf(const ByteVector& bytes)
{
    return QString::fromStdString(toHex(bytes));
}

const RolandModelId kXp60{0x6A};
const RolandDeviceId kDevice17 = RolandDeviceId::factoryDefault();

} // namespace

class RolandCodecTest : public QObject
{
    Q_OBJECT

private slots:
    void encodeDataRequest()
    {
        const auto rq1 = RolandSysExMessage::dataRequest(kDevice17, kXp60, RolandAddress(0x03, 0, 0, 0),
                                                         RolandSize(0, 0, 0, 0x0C));
        // F0 41 10 6A 11 03 00 00 00 00 00 00 0C sum F7 ; sum = 128-15 = 0x71
        QCOMPARE(hexOf(rq1.encode()), QStringLiteral("F0 41 10 6A 11 03 00 00 00 00 00 00 0C 71 F7"));
        QVERIFY(rq1.isDataRequest());
        QCOMPARE(rq1.size().value(), 12u);
        QCOMPARE(int(rq1.checksum()), 0x71);
        QCOMPARE(QString::fromStdString(rq1.endAddress()->toHexString()), QStringLiteral("03 00 00 0C"));
    }

    void encodeDataRequestMatchesLogExampleShape()
    {
        // The phase brief example: device=17 address=11 00 00 00 size=00 00 0C 00
        const auto rq1 = RolandSysExMessage::dataRequest(kDevice17, kXp60, RolandAddress(0x11, 0, 0, 0),
                                                         RolandSize(0, 0, 0x0C, 0));
        QCOMPARE(hexOf(rq1.encode()), QStringLiteral("F0 41 10 6A 11 11 00 00 00 00 00 0C 00 63 F7"));
        QCOMPARE(QString::fromStdString(rq1.summary()),
                 QStringLiteral("Roland RQ1 device=17 model=6A address=11 00 00 00 size=00 00 0C 00 (1536 bytes)"));
    }

    void encodeDataSet()
    {
        const auto dt1 = RolandSysExMessage::dataSet(kDevice17, kXp60, RolandAddress(0x03, 0, 0, 0),
                                                     ByteVector{'W', 'a', 'r', 'm'});
        QVERIFY(dt1.has_value());
        // sum = 0x03 + 0x57 + 0x61 + 0x72 + 0x6D = 410 ; 410 mod 128 = 26 ; 128 - 26 = 102 = 0x66
        QCOMPARE(hexOf(dt1->encode()), QStringLiteral("F0 41 10 6A 12 03 00 00 00 57 61 72 6D 66 F7"));
        QVERIFY(dt1->isDataSet());
        QCOMPARE(dt1->size().value(), 4u);
        QCOMPARE(dt1->data().size(), std::size_t(4));
    }

    void dataSetRejectsInvalidPayload()
    {
        QVERIFY(!RolandSysExMessage::dataSet(kDevice17, kXp60, RolandAddress(), ByteVector{}).has_value());
        QVERIFY(!RolandSysExMessage::dataSet(kDevice17, kXp60, RolandAddress(), ByteVector{0x00, 0x80}).has_value());
    }

    void twoByteModelIdEncodesWithPrefix()
    {
        // Later Roland models (e.g. XV series) use two-byte IDs; the codec must
        // not hard-code the XP-60's single byte.
        const RolandModelId twoByte{0x00, 0x10};
        const auto rq1 = RolandSysExMessage::dataRequest(kDevice17, twoByte, RolandAddress(0x03, 0, 0, 0),
                                                         RolandSize(0, 0, 0, 0x0C));
        QCOMPARE(hexOf(rq1.encode()), QStringLiteral("F0 41 10 00 10 11 03 00 00 00 00 00 00 0C 71 F7"));
    }

    // Golden fixtures published in the Roland XP-60/XP-80 MIDI Implementation.
    void rolandPublishedRq1Example()
    {
        const ByteVector published = bytesOf("F0 41 10 6A 11 01 00 00 00 00 00 1F 19 47 F7");
        const auto decoded = decodeRolandSysEx(published, kXp60);
        QVERIFY2(decoded.ok(), decoded.failure ? describe(*decoded.failure).c_str() : "");
        QVERIFY(decoded.message->isDataRequest());
        QCOMPARE(decoded.message->deviceId().displayNumber(), 17);
        QCOMPARE(decoded.message->modelId(), kXp60);
        QCOMPARE(decoded.message->address(), RolandAddress(0x01, 0, 0, 0));
        QCOMPARE(decoded.message->size(), RolandSize(0, 0, 0x1F, 0x19));
        QCOMPARE(decoded.message->size().value(), 3993u);

        const auto rebuilt = RolandSysExMessage::dataRequest(kDevice17, kXp60, RolandAddress(0x01, 0, 0, 0),
                                                             RolandSize(0, 0, 0x1F, 0x19));
        QCOMPARE(hexOf(rebuilt.encode()), hexOf(published));
        QCOMPARE(int(rebuilt.checksum()), 0x47);
    }

    void rolandPublishedDt1Example()
    {
        const ByteVector published = bytesOf("F0 41 10 6A 12 01 00 00 28 06 51 F7");
        const auto decoded = decodeRolandSysEx(published, kXp60);
        QVERIFY2(decoded.ok(), decoded.failure ? describe(*decoded.failure).c_str() : "");
        QVERIFY(decoded.message->isDataSet());
        QCOMPARE(decoded.message->address(), RolandAddress(0x01, 0, 0, 0x28));
        QCOMPARE(decoded.message->data().size(), std::size_t(1));
        QCOMPARE(decoded.message->data()[0], Byte(0x06));

        const auto rebuilt = RolandSysExMessage::dataSet(kDevice17, kXp60, RolandAddress(0x01, 0, 0, 0x28), ByteVector{0x06}).value();
        QCOMPARE(hexOf(rebuilt.encode()), hexOf(published));
        QCOMPARE(int(rebuilt.checksum()), 0x51);
    }

    void decodeDataRequest()
    {
        const auto result = decodeRolandSysEx(bytesOf("F0 41 10 6A 11 03 00 00 00 00 00 00 0C 71 F7"), kXp60);
        QVERIFY2(result.ok(), result.failure ? describe(*result.failure).c_str() : "");
        QVERIFY(result.message->isDataRequest());
        QCOMPARE(result.message->deviceId().displayNumber(), 17);
        QCOMPARE(result.message->modelId(), kXp60);
        QCOMPARE(result.message->address(), RolandAddress(0x03, 0, 0, 0));
        QCOMPARE(result.message->size(), RolandSize(0, 0, 0, 0x0C));
    }

    void decodeDataSet()
    {
        const auto result = decodeRolandSysEx(bytesOf("F0 41 10 6A 12 03 00 00 00 57 61 72 6D 66 F7"), kXp60);
        QVERIFY2(result.ok(), result.failure ? describe(*result.failure).c_str() : "");
        QVERIFY(result.message->isDataSet());
        QCOMPARE(result.message->address(), RolandAddress(0x03, 0, 0, 0));
        QCOMPARE(result.message->data().size(), std::size_t(4));
        QCOMPARE(result.message->data()[0], Byte('W'));
        QCOMPARE(result.message->size().value(), 4u);
    }

    void decodeLongDataSet()
    {
        // 256 data bytes: twice the XP-60's 128-byte packet limit. Packet size is
        // a transfer-layer rule; the codec itself is length-agnostic.
        ByteVector data(256);
        for (std::size_t i = 0; i < data.size(); ++i) {
            data[i] = static_cast<Byte>(i & 0x7F);
        }
        const auto dt1 = RolandSysExMessage::dataSet(kDevice17, kXp60, RolandAddress(0x11, 0, 0, 0), data).value();
        const auto encoded = dt1.encode();
        QCOMPARE(encoded.size(), std::size_t(5 + 4 + 256 + 2));
        const auto decoded = decodeRolandSysEx(encoded, kXp60);
        QVERIFY(decoded.ok());
        QCOMPARE(*decoded.message, dt1);
        QCOMPARE(ByteVector(decoded.message->data().begin(), decoded.message->data().end()), data);
    }

    void roundTripModelToBytesToModel()
    {
        const auto rq1 = RolandSysExMessage::dataRequest(RolandDeviceId::fromDisplayNumber(20).value(), kXp60,
                                                         RolandAddress(0x11, 0x05, 0x00, 0x00), RolandSize(0, 0, 0x0C, 0));
        const auto decoded = decodeRolandSysEx(rq1.encode(), kXp60);
        QVERIFY(decoded.ok());
        QCOMPARE(*decoded.message, rq1);

        const auto dt1 = RolandSysExMessage::dataSet(kDevice17, kXp60, RolandAddress(0x02, 0x09, 0, 0),
                                                     ByteVector{0x00, 0x7F, 0x40, 0x01}).value();
        const auto decodedDt1 = decodeRolandSysEx(dt1.encode(), kXp60);
        QVERIFY(decodedDt1.ok());
        QCOMPARE(*decodedDt1.message, dt1);
    }

    void roundTripBytesToModelToBytes()
    {
        const ByteVector original = bytesOf("F0 41 1F 6A 12 11 7F 00 00 41 42 43 44 45 46 47 48 49 4A 4B 4C 0A F7");
        // Fix the checksum so the fixture is valid: address 11 7F 00 00 + 'A'..'L'
        const auto check = decodeRolandSysEx(original, kXp60);
        QVERIFY2(check.ok() || check.failure->error == RolandParseError::InvalidChecksum, "fixture shape");
        ByteVector fixed = original;
        {
            const ByteSpan body(fixed.data() + 5, fixed.size() - 7);
            fixed[fixed.size() - 2] = RolandChecksum::compute(body);
        }
        const auto decoded = decodeRolandSysEx(fixed, kXp60);
        QVERIFY2(decoded.ok(), describe(*decoded.failure).c_str());
        QCOMPARE(hexOf(decoded.message->encode()), hexOf(fixed));
    }

    void decodeFailures_data()
    {
        QTest::addColumn<QString>("hex");
        QTest::addColumn<int>("error");

        QTest::newRow("not sysex") << "90 40 7F" << int(RolandParseError::NotSysEx);
        QTest::newRow("truncated") << "F0 41 10 6A 11 03 00 F7" << int(RolandParseError::Truncated);
        QTest::newRow("missing end") << "F0 41 10 6A 11 03 00 00 00 00 00 00 0C 71 00" << int(RolandParseError::MissingEnd);
        QTest::newRow("not roland") << "F0 43 10 6A 11 03 00 00 00 00 00 00 0C 71 F7" << int(RolandParseError::NotRoland);
        QTest::newRow("device id low") << "F0 41 0F 6A 11 03 00 00 00 00 00 00 0C 71 F7" << int(RolandParseError::InvalidDeviceId);
        QTest::newRow("device id high") << "F0 41 20 6A 11 03 00 00 00 00 00 00 0C 71 F7" << int(RolandParseError::InvalidDeviceId);
        QTest::newRow("wrong model") << "F0 41 10 10 11 03 00 00 00 00 00 00 0C 71 F7" << int(RolandParseError::UnsupportedModel);
        QTest::newRow("two-byte 00 6A model (not the XP-60)") << "F0 41 10 00 6A 11 03 00 00 00 00 00 00 0C 71 F7" << int(RolandParseError::UnsupportedModel);
        QTest::newRow("unsupported command") << "F0 41 10 6A 13 03 00 00 00 00 00 00 0C 71 F7" << int(RolandParseError::UnsupportedCommand);
        QTest::newRow("address byte bit7") << "F0 41 10 6A 11 83 00 00 00 00 00 00 0C 71 F7" << int(RolandParseError::InvalidAddressByte);
        QTest::newRow("size byte bit7") << "F0 41 10 6A 11 03 00 00 00 00 00 00 8C 71 F7" << int(RolandParseError::InvalidSizeByte);
        QTest::newRow("rq1 body too long") << "F0 41 10 6A 11 03 00 00 00 00 00 00 0C 00 65 F7" << int(RolandParseError::InvalidSize);
        QTest::newRow("rq1 body too short") << "F0 41 10 6A 11 03 00 00 00 00 00 0C 7D F7" << int(RolandParseError::InvalidSize);
        QTest::newRow("rq1 zero size") << "F0 41 10 6A 11 03 00 00 00 00 00 00 00 7D F7" << int(RolandParseError::InvalidSize);
        QTest::newRow("dt1 data bit7") << "F0 41 10 6A 12 03 00 00 00 57 E1 72 6D 66 F7" << int(RolandParseError::InvalidDataByte);
        QTest::newRow("dt1 empty data") << "F0 41 10 6A 12 03 00 00 00 7D F7" << int(RolandParseError::EmptyData);
        QTest::newRow("rq1 bad checksum") << "F0 41 10 6A 11 03 00 00 00 00 00 00 0C 70 F7" << int(RolandParseError::InvalidChecksum);
        QTest::newRow("dt1 bad checksum") << "F0 41 10 6A 12 03 00 00 00 57 61 72 6D 67 F7" << int(RolandParseError::InvalidChecksum);
        QTest::newRow("dt1 corrupted data byte") << "F0 41 10 6A 12 03 00 00 00 57 61 72 6E 66 F7" << int(RolandParseError::InvalidChecksum);
    }

    void decodeFailures()
    {
        QFETCH(QString, hex);
        QFETCH(int, error);
        const auto result = decodeRolandSysEx(bytesOf(hex.toStdString().c_str()), kXp60);
        QVERIFY(!result.ok());
        QVERIFY(result.failure.has_value());
        QCOMPARE(int(result.failure->error), error);
        QVERIFY(!describe(*result.failure).empty());
    }

    void decodeAcceptsAnyKnownModel()
    {
        const std::vector<RolandModelId> known{RolandModelId{0x6A}, RolandModelId{0x00, 0x10}};
        const auto xp = decodeRolandSysEx(bytesOf("F0 41 10 6A 11 03 00 00 00 00 00 00 0C 71 F7"), known);
        QVERIFY(xp.ok());
        QCOMPARE(xp.message->modelId(), RolandModelId({0x6A}));
        const auto xv = decodeRolandSysEx(bytesOf("F0 41 10 00 10 11 03 00 00 00 00 00 00 0C 71 F7"), known);
        QVERIFY(xv.ok());
        QCOMPARE(xv.message->modelId(), RolandModelId({0x00, 0x10}));
    }

    void structuralHelpers()
    {
        QVERIFY(isSysEx(bytesOf("F0 41")));
        QVERIFY(isRolandSysEx(bytesOf("F0 41")));
        QVERIFY(!isRolandSysEx(bytesOf("F0 43")));
        QVERIFY(!isSysEx(bytesOf("90")));
        QVERIFY(!isSysEx(ByteVector{}));
    }

    void xp60ModelIdIsUsedByCodec()
    {
        const auto rq1 = RolandSysExMessage::dataRequest(xp60studio::xp60::factoryDefaultDeviceId(), xp60studio::xp60::modelId(),
                                                         RolandAddress(0x03, 0, 0, 0), RolandSize(0, 0, 0, 0x0C));
        QCOMPARE(hexOf(rq1.encode()), QStringLiteral("F0 41 10 6A 11 03 00 00 00 00 00 00 0C 71 F7"));
    }
};

QTEST_APPLESS_MAIN(RolandCodecTest)
#include "tst_roland_codec.moc"
