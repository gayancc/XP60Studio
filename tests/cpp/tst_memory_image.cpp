#include "xp60/Xp60Device.h"
#include "xpmodel/MemoryImage.h"

#include <QtTest>

using namespace xp60studio;
using namespace xp60studio::roland;
using namespace xp60studio::xpmodel;

namespace {

RolandSysExMessage dt1(RolandAddress address, ByteVector data)
{
    return RolandSysExMessage::dataSet(RolandDeviceId::factoryDefault(), xp60::modelId(), address, std::move(data)).value();
}

ByteVector filled(std::size_t count, Byte value)
{
    return ByteVector(count, value);
}

} // namespace

class MemoryImageTest : public QObject
{
    Q_OBJECT

private slots:
    void emptyImage()
    {
        MemoryImage image;
        QVERIFY(image.isEmpty());
        QCOMPARE(image.byteCount(), std::size_t(0));
        QVERIFY(!image.read(RolandAddress(0x03, 0, 0, 0), 1).has_value());
        QVERIFY(image.read(RolandAddress(0x03, 0, 0, 0), 0).has_value()); // zero bytes is trivially readable
        QVERIFY(image.ranges().empty());
        const auto cov = image.coverage(RolandAddress(0x03, 0, 0, 0), 12);
        QCOMPARE(cov.covered, 0u);
        QCOMPARE(cov.requested, 12u);
        QVERIFY(!cov.complete());
        QCOMPARE(cov.firstMissing.value(), RolandAddress(0x03, 0, 0, 0));
    }

    void writeAndReadBack()
    {
        MemoryImage image;
        const RolandAddress base(0x03, 0, 0, 0);
        QVERIFY(image.write(base, filled(12, 'A')));
        QCOMPARE(image.byteCount(), std::size_t(12));
        QCOMPARE(image.read(base, 12).value(), filled(12, 'A'));
        QCOMPARE(image.read(*base.plus(4), 4).value(), filled(4, 'A'));
        QVERIFY(!image.read(base, 13).has_value());
        QVERIFY(!image.read(*base.minus(1), 2).has_value());
        QCOMPARE(image.byteAt(*base.plus(11)).value(), Byte('A'));
        QVERIFY(!image.byteAt(*base.plus(12)).has_value());
        QVERIFY(image.contains(base, 12));
        QVERIFY(!image.contains(base, 13));
        QCOMPARE(image.writeCount(), std::size_t(1));
        QCOMPARE(image.overlappingWriteCount(), std::size_t(0));
    }

    void chunksInAnyOrderMergeIntoOneRange()
    {
        MemoryImage image;
        const RolandAddress base(0x11, 0, 0, 0);
        // 300 bytes as 128 + 128 + 44, delivered out of order.
        QVERIFY(image.addDataSet(dt1(*base.plus(256), filled(44, 0x03))));
        QVERIFY(image.addDataSet(dt1(base, filled(128, 0x01))));
        QVERIFY(image.addDataSet(dt1(*base.plus(128), filled(128, 0x02))));
        QCOMPARE(image.ranges().size(), std::size_t(1));
        QCOMPARE(image.ranges()[0].begin, base);
        QCOMPARE(image.ranges()[0].byteCount, 300u);
        QCOMPARE(image.byteCount(), std::size_t(300));
        const auto all = image.read(base, 300).value();
        QCOMPARE(all[0], Byte(0x01));
        QCOMPARE(all[127], Byte(0x01));
        QCOMPARE(all[128], Byte(0x02));
        QCOMPARE(all[255], Byte(0x02));
        QCOMPARE(all[256], Byte(0x03));
        QCOMPARE(all[299], Byte(0x03));
        QVERIFY(image.coverage(base, 300).complete());
        QCOMPARE(image.overlappingWriteCount(), std::size_t(0));
    }

    void gapsAreReportedNotHidden()
    {
        MemoryImage image;
        const RolandAddress base(0x11, 0, 0, 0);
        image.write(base, filled(10, 1));
        image.write(*base.plus(20), filled(10, 2));
        QCOMPARE(image.ranges().size(), std::size_t(2));
        QVERIFY(!image.read(base, 30).has_value());
        const auto cov = image.coverage(base, 30);
        QCOMPARE(cov.covered, 20u);
        QCOMPARE(cov.requested, 30u);
        QVERIFY(!cov.complete());
        QCOMPARE(cov.firstMissing.value(), *base.plus(10));
        // Filling the gap merges everything.
        image.write(*base.plus(10), filled(10, 3));
        QCOMPARE(image.ranges().size(), std::size_t(1));
        QCOMPARE(image.byteCount(), std::size_t(30));
        QVERIFY(image.coverage(base, 30).complete());
    }

    void overlappingWritesLatestWinsAndAreCounted()
    {
        MemoryImage image;
        const RolandAddress base(0x03, 0, 0, 0);
        image.write(base, filled(12, 'A'));
        image.write(*base.plus(6), filled(12, 'B')); // overlaps 6 bytes, extends 6
        QCOMPARE(image.byteCount(), std::size_t(18));
        QCOMPARE(image.overlappingWriteCount(), std::size_t(1));
        const auto bytes = image.read(base, 18).value();
        QCOMPARE(bytes[5], Byte('A'));
        QCOMPARE(bytes[6], Byte('B'));
        QCOMPARE(bytes[17], Byte('B'));
        // Exact rewrite of an existing range
        image.write(base, filled(6, 'C'));
        QCOMPARE(image.byteCount(), std::size_t(18));
        QCOMPARE(image.overlappingWriteCount(), std::size_t(2));
        QCOMPARE(image.read(base, 6).value(), filled(6, 'C'));
        QCOMPARE(image.ranges().size(), std::size_t(1));
    }

    void writeBridgingSeveralRuns()
    {
        MemoryImage image;
        const RolandAddress base(0x10, 0, 0, 0);
        image.write(base, filled(4, 1));
        image.write(*base.plus(8), filled(4, 2));
        image.write(*base.plus(16), filled(4, 3));
        QCOMPARE(image.ranges().size(), std::size_t(3));
        image.write(*base.plus(2), filled(16, 9)); // covers 2..18, bridging all three
        QCOMPARE(image.ranges().size(), std::size_t(1));
        QCOMPARE(image.ranges()[0].byteCount, 20u);
        QCOMPARE(image.byteCount(), std::size_t(20));
        const auto bytes = image.read(base, 20).value();
        QCOMPARE(bytes[1], Byte(1));
        QCOMPARE(bytes[2], Byte(9));
        QCOMPARE(bytes[17], Byte(9));
        QCOMPARE(bytes[18], Byte(3));
    }

    void rq1IsNotData()
    {
        MemoryImage image;
        const auto rq1 = RolandSysExMessage::dataRequest(RolandDeviceId::factoryDefault(), xp60::modelId(),
                                                         RolandAddress(0x03, 0, 0, 0), RolandSize(0, 0, 0, 0x0C));
        QVERIFY(!image.addDataSet(rq1));
        QVERIFY(image.isEmpty());
    }

    void addressSpaceEndIsRespected()
    {
        MemoryImage image;
        const RolandAddress top(0x7F, 0x7F, 0x7F, 0x7E);
        QVERIFY(image.write(top, filled(2, 1)));
        QVERIFY(!image.write(top, filled(3, 1)));
        QVERIFY(image.write(top, ByteVector{}));
        QCOMPARE(image.byteCount(), std::size_t(2));
        image.clear();
        QVERIFY(image.isEmpty());
        QCOMPARE(image.writeCount(), std::size_t(0));
    }

    void separatePatchesStaySeparateRanges()
    {
        MemoryImage image;
        // Two user patches 00 01 00 00 apart (16384 bytes).
        image.write(RolandAddress(0x11, 0x00, 0, 0), filled(12, 'A'));
        image.write(RolandAddress(0x11, 0x01, 0, 0), filled(12, 'B'));
        QCOMPARE(image.ranges().size(), std::size_t(2));
        QCOMPARE(image.read(RolandAddress(0x11, 0x01, 0, 0), 12).value(), filled(12, 'B'));
        QVERIFY(!image.contains(RolandAddress(0x11, 0x00, 0x00, 0x0C)));
    }
};

QTEST_APPLESS_MAIN(MemoryImageTest)
#include "tst_memory_image.moc"
