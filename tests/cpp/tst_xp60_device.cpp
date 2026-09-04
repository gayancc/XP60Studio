#include "xp60/Xp60Device.h"

#include <QtTest>

using namespace xp60studio;

class Xp60DeviceTest : public QObject
{
    Q_OBJECT

private slots:
    void modelIdAndDefaults()
    {
        QCOMPARE(QString::fromStdString(xp60::modelId().toHexString()), QStringLiteral("00 6A"));
        QCOMPARE(xp60::factoryDefaultDeviceId().displayNumber(), 17);
        // Nothing may claim hardware verification before a capture exists.
        QVERIFY(xp60::modelIdStatus() != xp60::VerificationStatus::HardwareVerified);
    }

    void memoryRegionsAreDistinctAndOrdered()
    {
        const auto regions = xp60::knownMemoryRegions();
        QVERIFY(regions.size() >= 4);
        for (std::size_t i = 1; i < regions.size(); ++i) {
            QVERIFY2(regions[i - 1].baseAddress < regions[i].baseAddress, "regions must be listed in address order");
            QVERIFY(regions[i - 1].id != regions[i].id);
        }
        for (const auto& region : regions) {
            QVERIFY(!region.name.empty());
            QVERIFY(!region.sourceNote.empty());
            QVERIFY(region.status != xp60::VerificationStatus::HardwareVerified);
        }
        QVERIFY(xp60::findMemoryRegion("temporary-patch") != nullptr);
        QVERIFY(xp60::findMemoryRegion("temporary-patch")->temporaryMemory);
        QVERIFY(!xp60::findMemoryRegion("user-patch")->temporaryMemory);
        QVERIFY(xp60::findMemoryRegion("does-not-exist") == nullptr);
    }

    void safeReadPresetsAreSmallAndReadOnly()
    {
        const auto presets = xp60::safeReadPresets();
        QVERIFY(!presets.empty());
        for (const auto& preset : presets) {
            QVERIFY(!preset.size.isZero());
            QVERIFY2(preset.size.value() <= 256, "diagnostic presets stay within one documented DT1 chunk");
            QVERIFY(!preset.description.empty());
            QVERIFY(preset.status != xp60::VerificationStatus::HardwareVerified);
        }
        QCOMPARE(QString::fromStdString(presets[0].address.toHexString()), QStringLiteral("03 00 00 00"));
        QCOMPARE(presets[0].size.value(), 12u);
    }

    void transferDefaultsAreSane()
    {
        const auto defaults = xp60::transferDefaults();
        QVERIFY(defaults.interMessageDelay.count() >= 0);
        QCOMPARE(defaults.maxDataSetPayloadBytes, std::size_t(256));
        QVERIFY(defaults.firstResponseTimeout > defaults.interMessageDelay);
        QVERIFY(defaults.betweenChunkTimeout.count() > 0);
    }

    void statusLabels()
    {
        QCOMPARE(QString::fromUtf8(xp60::verificationStatusName(xp60::VerificationStatus::DocumentationDerived).data()),
                 QStringLiteral("DocumentationDerived"));
        QVERIFY(!xp60::verificationStatusLabel(xp60::VerificationStatus::Unknown).empty());
    }
};

QTEST_APPLESS_MAIN(Xp60DeviceTest)
#include "tst_xp60_device.moc"
