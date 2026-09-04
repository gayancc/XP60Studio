#include "roland/HexFormat.h"
#include "roland/RolandAddress.h"
#include "roland/RolandDeviceId.h"
#include "roland/RolandModelId.h"
#include "roland/RolandSize.h"

#include <QtTest>

using namespace xp60studio::roland;

class RolandAddressSizeTest : public QObject
{
    Q_OBJECT

private slots:
    // Address ----------------------------------------------------------------
    void addressFromBytesAndBack()
    {
        const ByteVector bytes{0x11, 0x00, 0x00, 0x00};
        const auto address = RolandAddress::fromBytes(bytes);
        QVERIFY(address.has_value());
        const auto out = address->bytes();
        QCOMPARE(ByteVector(out.begin(), out.end()), bytes);
        QCOMPARE(QString::fromStdString(address->toHexString()), QStringLiteral("11 00 00 00"));
        QCOMPARE(address->value(), 0x11u << 21);
    }

    void addressRejectsInvalidBytes()
    {
        QVERIFY(!RolandAddress::fromBytes(ByteVector{0x80, 0x00, 0x00, 0x00}).has_value());
        QVERIFY(!RolandAddress::fromBytes(ByteVector{0x00, 0x00, 0x00, 0xFF}).has_value());
        QVERIFY(!RolandAddress::fromBytes(ByteVector{0x00, 0x00, 0x00}).has_value());
        QVERIFY(!RolandAddress::fromBytes(ByteVector{0x00, 0x00, 0x00, 0x00, 0x00}).has_value());
        QVERIFY(!RolandAddress::fromValue(1ull << 28).has_value());
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, RolandAddress(0x80, 0, 0, 0));
    }

    void addressArithmeticCarriesAt128()
    {
        const RolandAddress base(0x03, 0x00, 0x00, 0x7F);
        const auto next = base.plus(1);
        QVERIFY(next.has_value());
        QCOMPARE(QString::fromStdString(next->toHexString()), QStringLiteral("03 00 01 00"));

        const RolandAddress deep(0x03, 0x7F, 0x7F, 0x7F);
        QCOMPARE(QString::fromStdString(deep.plus(1)->toHexString()), QStringLiteral("04 00 00 00"));

        // 128 bytes past 03 00 00 00 is 03 00 01 00 ; 256 bytes is 03 00 02 00.
        const RolandAddress patch(0x03, 0x00, 0x00, 0x00);
        QCOMPARE(QString::fromStdString(patch.plus(128)->toHexString()), QStringLiteral("03 00 01 00"));
        QCOMPARE(QString::fromStdString(patch.plus(256)->toHexString()), QStringLiteral("03 00 02 00"));
        // Tone offsets in the JV/XP family are 10 00, 12 00, ... (7-bit): 0x10 * 128 = 2048 bytes.
        QCOMPARE(QString::fromStdString(patch.plus(0x10 * 128)->toHexString()), QStringLiteral("03 00 10 00"));
        // User patch stride 00 01 00 00 = 16384 bytes.
        const RolandAddress user(0x11, 0x00, 0x00, 0x00);
        QCOMPARE(QString::fromStdString(user.plus(16384)->toHexString()), QStringLiteral("11 01 00 00"));
        QCOMPARE(QString::fromStdString(user.plus(127 * 16384)->toHexString()), QStringLiteral("11 7F 00 00"));
    }

    void addressArithmeticDetectsOverflowAndUnderflow()
    {
        const RolandAddress top(0x7F, 0x7F, 0x7F, 0x7F);
        QVERIFY(!top.plus(1).has_value());
        QVERIFY(top.plus(0).has_value());
        const RolandAddress zero;
        QVERIFY(!zero.minus(1).has_value());
        QCOMPARE(zero.minus(0)->value(), 0u);
    }

    void addressDistanceAndComparison()
    {
        const RolandAddress a(0x03, 0x00, 0x00, 0x00);
        const RolandAddress b(0x03, 0x00, 0x01, 0x00);
        QVERIFY(a < b);
        QVERIFY(a != b);
        QCOMPARE(a.distanceTo(b).value(), 128u);
        QVERIFY(!b.distanceTo(a).has_value());
        QCOMPARE(a.distanceTo(a).value(), 0u);
        QCOMPARE(a.plus(*a.distanceTo(b)).value(), b);
    }

    void addressParseHexAcceptsCommonForms()
    {
        const RolandAddress expected(0x03, 0x00, 0x00, 0x0C);
        QCOMPARE(RolandAddress::parseHex("03 00 00 0C").value(), expected);
        QCOMPARE(RolandAddress::parseHex("0300000c").value(), expected);
        QCOMPARE(RolandAddress::parseHex("03-00-00-0C").value(), expected);
        QCOMPARE(RolandAddress::parseHex("0x03 0x00 0x00 0x0C").value(), expected);
        QCOMPARE(RolandAddress::parseHex("  03   00 00 0C  ").value(), expected);
        QVERIFY(!RolandAddress::parseHex("03 00 00").has_value());
        QVERIFY(!RolandAddress::parseHex("03 00 00 0C 00").has_value());
        QVERIFY(!RolandAddress::parseHex("03 00 00 8C").has_value());
        QVERIFY(!RolandAddress::parseHex("03 00 00 0G").has_value());
        QVERIFY(!RolandAddress::parseHex("0300000").has_value());
        QVERIFY(!RolandAddress::parseHex("").has_value());
    }

    // Size -------------------------------------------------------------------
    void sizeValueMatchesSevenBitEncoding()
    {
        QCOMPARE(RolandSize(0x00, 0x00, 0x00, 0x0C).value(), 12u);
        QCOMPARE(RolandSize(0x00, 0x00, 0x01, 0x00).value(), 128u);
        QCOMPARE(RolandSize(0x00, 0x00, 0x02, 0x00).value(), 256u);
        QCOMPARE(RolandSize(0x00, 0x00, 0x01, 0x01).value(), 129u);
        QCOMPARE(RolandSize(0x00, 0x00, 0x0C, 0x00).value(), 1536u);
        QCOMPARE(RolandSize(0x00, 0x01, 0x00, 0x00).value(), 16384u);
        QCOMPARE(QString::fromStdString(RolandSize::fromValue(256)->toHexString()), QStringLiteral("00 00 02 00"));
        QCOMPARE(QString::fromStdString(RolandSize::fromValue(129)->toHexString()), QStringLiteral("00 00 01 01"));
        QVERIFY(RolandSize().isZero());
        QVERIFY(!RolandSize::fromValue(1ull << 28).has_value());
        QVERIFY(!RolandSize::fromBytes(ByteVector{0x00, 0x00, 0x80, 0x00}).has_value());
        QVERIFY(RolandSize(0, 0, 0, 1) < RolandSize(0, 0, 1, 0));
    }

    // Device / model IDs -----------------------------------------------------
    void deviceIdMapsBytesToDisplayNumbers()
    {
        QCOMPARE(RolandDeviceId::factoryDefault().byte(), Byte(0x10));
        QCOMPARE(RolandDeviceId::factoryDefault().displayNumber(), 17);
        QCOMPARE(RolandDeviceId::fromDisplayNumber(32)->byte(), Byte(0x1F));
        QCOMPARE(RolandDeviceId::fromByte(0x1F)->displayNumber(), 32);
        QVERIFY(!RolandDeviceId::fromByte(0x0F).has_value());
        QVERIFY(!RolandDeviceId::fromByte(0x20).has_value());
        QVERIFY(!RolandDeviceId::fromDisplayNumber(16).has_value());
        QVERIFY(!RolandDeviceId::fromDisplayNumber(33).has_value());
    }

    void modelIdKeepsLengthAsPartOfIdentity()
    {
        const RolandModelId xp{0x6A};
        const RolandModelId twoByte{0x00, 0x6A};
        QCOMPARE(xp.size(), std::size_t(1));
        QCOMPARE(twoByte.size(), std::size_t(2));
        QVERIFY(xp != twoByte);
        QCOMPARE(QString::fromStdString(xp.toHexString()), QStringLiteral("6A"));
        QCOMPARE(QString::fromStdString(twoByte.toHexString()), QStringLiteral("00 6A"));
        QVERIFY(!RolandModelId::fromBytes(ByteVector{}).has_value());
        QVERIFY(!RolandModelId::fromBytes(ByteVector{0x80}).has_value());
        QVERIFY(!RolandModelId::fromBytes(ByteVector{0, 0, 0, 0, 0}).has_value());
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, RolandModelId({0x80}));
    }

    // Hex utilities ----------------------------------------------------------
    void hexRoundTrip()
    {
        const ByteVector bytes{0xF0, 0x41, 0x10, 0x6A, 0x12, 0x00, 0xF7};
        QCOMPARE(QString::fromStdString(toHex(bytes)), QStringLiteral("F0 41 10 6A 12 00 F7"));
        QCOMPARE(QString::fromStdString(toHex(bytes, "")), QStringLiteral("F041106A1200F7"));
        QCOMPARE(parseHexBytes("F0 41 10 6A 12 00 F7").value(), bytes);
        QCOMPARE(parseHexBytes("F041106A1200F7").value(), bytes);
        QVERIFY(!parseHexBytes("F0 4").has_value());
        QVERIFY(!parseHexBytes("zz").has_value());
    }
};

QTEST_APPLESS_MAIN(RolandAddressSizeTest)
#include "tst_roland_address_size.moc"
