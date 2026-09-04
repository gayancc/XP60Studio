#include "roland/RolandChecksum.h"

#include <QtTest>

#include <array>
#include <random>

using namespace xp60studio::roland;

class RolandChecksumTest : public QObject
{
    Q_OBJECT

private slots:
    void knownValues_data()
    {
        QTest::addColumn<QByteArray>("body");
        QTest::addColumn<int>("expected");

        // address 11 00 00 00 + size 00 00 0C 00 : sum = 0x1D (29) -> 128 - 29 = 99 = 0x63
        QTest::newRow("RQ1 user patch 1, 0C 00") << QByteArray::fromHex("110000000000" "0C00") << 0x63;
        // address 03 00 00 00 + size 00 00 00 0C : sum = 0x0F (15) -> 113 = 0x71
        QTest::newRow("RQ1 temp patch name") << QByteArray::fromHex("03000000" "0000000C") << 0x71;
        // all zero : sum = 0 -> (128 - 0) mod 128 = 0
        QTest::newRow("all zero") << QByteArray::fromHex("00000000" "00000000") << 0x00;
        // sum exactly 128 : 40 40 00 00 + 00 00 00 00 -> 0
        QTest::newRow("sum is 128") << QByteArray::fromHex("40400000" "00000000") << 0x00;
        // sum 127 : 7F 00 00 00 -> 1
        QTest::newRow("sum is 127") << QByteArray::fromHex("7F000000" "00000000") << 0x01;
        // DT1 with data: address 03 00 00 00, data "A" (0x41): sum = 0x44 (68) -> 60 = 0x3C
        QTest::newRow("DT1 single byte") << QByteArray::fromHex("03000000" "41") << 0x3C;
        // sum wraps past 256: 7F 7F 7F 7F = 508 -> 508 mod 128 = 124 -> 4
        QTest::newRow("wraps past 256") << QByteArray::fromHex("7F7F7F7F") << 0x04;
    }

    void knownValues()
    {
        QFETCH(QByteArray, body);
        QFETCH(int, expected);
        const ByteSpan span(reinterpret_cast<const Byte*>(body.constData()), static_cast<std::size_t>(body.size()));
        QCOMPARE(int(RolandChecksum::compute(span)), expected);
        QVERIFY(RolandChecksum::verify(span, static_cast<Byte>(expected)));
    }

    void splitAddressAndBodyMatchesConcatenation()
    {
        const std::array<Byte, 4> address{0x11, 0x00, 0x00, 0x00};
        const std::array<Byte, 4> size{0x00, 0x00, 0x0C, 0x00};
        std::array<Byte, 8> all{0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x00};
        QCOMPARE(RolandChecksum::compute(address, size), RolandChecksum::compute(all));
    }

    void generatedCombinationsSumToZeroModulo128()
    {
        std::mt19937 rng(20260904);
        std::uniform_int_distribution<int> byteDist(0, 127);
        std::uniform_int_distribution<int> lenDist(4, 300);
        for (int iteration = 0; iteration < 500; ++iteration) {
            ByteVector body(static_cast<std::size_t>(lenDist(rng)));
            for (auto& b : body) {
                b = static_cast<Byte>(byteDist(rng));
            }
            const Byte checksum = RolandChecksum::compute(body);
            QVERIFY(checksum < 0x80);
            unsigned sum = 0;
            for (const Byte b : body) {
                sum += b;
            }
            QCOMPARE((sum + checksum) % 128u, 0u);
            QVERIFY(RolandChecksum::verify(body, checksum));
        }
    }

    void corruptionIsRejected()
    {
        ByteVector body{0x03, 0x00, 0x00, 0x00, 0x57, 0x61, 0x72, 0x6D};
        const Byte checksum = RolandChecksum::compute(body);
        QVERIFY(RolandChecksum::verify(body, checksum));

        // Every other checksum value fails.
        for (int candidate = 0; candidate < 128; ++candidate) {
            if (candidate == checksum) {
                continue;
            }
            QVERIFY2(!RolandChecksum::verify(body, static_cast<Byte>(candidate)),
                     qPrintable(QString("checksum %1 should be rejected").arg(candidate)));
        }

        // Flipping a data bit invalidates the original checksum (unless the
        // flip is a multiple of 128, which cannot happen within 7 bits).
        for (std::size_t i = 0; i < body.size(); ++i) {
            ByteVector corrupted = body;
            corrupted[i] ^= 0x01;
            QVERIFY(!RolandChecksum::verify(corrupted, checksum));
        }
    }

    void emptyBodyHasZeroChecksum()
    {
        QCOMPARE(int(RolandChecksum::compute(ByteSpan{})), 0);
    }
};

QTEST_APPLESS_MAIN(RolandChecksumTest)
#include "tst_roland_checksum.moc"
