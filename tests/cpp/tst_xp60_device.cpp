#include "xp60/Xp60Device.h"

#include <QtTest>

using namespace xp60studio;

class Xp60DeviceTest : public QObject
{
    Q_OBJECT

private slots:
    void modelIdAndDefaults()
    {
        QCOMPARE(QString::fromStdString(xp60::modelId().toHexString()), QStringLiteral("6A"));
        QCOMPARE(xp60::modelId().size(), std::size_t(1));
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
        // Rhythm Setup entries from the MIDI Implementation.
        const auto* tempRhythm = xp60::findMemoryRegion("temporary-rhythm-setup");
        QVERIFY(tempRhythm != nullptr);
        QCOMPARE(QString::fromStdString(tempRhythm->baseAddress.toHexString()), QStringLiteral("02 09 00 00"));
        QVERIFY(tempRhythm->temporaryMemory);
        const auto* userRhythm = xp60::findMemoryRegion("user-rhythm-setup");
        QVERIFY(userRhythm != nullptr);
        QCOMPARE(QString::fromStdString(userRhythm->baseAddress.toHexString()), QStringLiteral("10 40 00 00"));
        QVERIFY(!userRhythm->temporaryMemory);
    }

    void safeReadPresetsAreSmallAndReadOnly()
    {
        const auto presets = xp60::safeReadPresets();
        QVERIFY(!presets.empty());
        for (const auto& preset : presets) {
            QVERIFY(!preset.size.isZero());
            QVERIFY(!preset.description.empty());
            QVERIFY(preset.status != xp60::VerificationStatus::HardwareVerified);
        }
        // Patch-name reads: 12 characters at Patch Common offset 00 00.
        QCOMPARE(QString::fromStdString(presets[0].address.toHexString()), QStringLiteral("03 00 00 00"));
        QCOMPARE(presets[0].size.value(), 12u);
        QCOMPARE(QString::fromStdString(presets[1].address.toHexString()), QStringLiteral("11 00 00 00"));
        QCOMPARE(presets[1].size.value(), 12u);
        // Roland-published RQ1 example: Temporary Performance, 00 00 1F 19 bytes.
        QCOMPARE(QString::fromStdString(presets[2].address.toHexString()), QStringLiteral("01 00 00 00"));
        QCOMPARE(QString::fromStdString(presets[2].size.toHexString()), QStringLiteral("00 00 1F 19"));
        QCOMPARE(presets[2].size.value(), 3993u);
        QCOMPARE(presets[2].status, xp60::VerificationStatus::DocumentationDerived);
        // The partial System read is a project choice, never labelled as Roland-documented.
        QCOMPARE(presets[3].status, xp60::VerificationStatus::ProjectDefined);
    }

    void transferDefaultsAreSane()
    {
        const auto defaults = xp60::transferDefaults();
        QVERIFY(defaults.interMessageDelay.count() >= 0);
        QCOMPARE(defaults.maxDataSetPayloadBytes, std::size_t(128));
        QCOMPARE(defaults.interMessageDelay.count(), 20LL);
        QVERIFY(defaults.firstResponseTimeout > defaults.interMessageDelay);
        QVERIFY(defaults.betweenChunkTimeout.count() > 0);
    }

    void statusLabels()
    {
        QCOMPARE(QString::fromUtf8(xp60::verificationStatusName(xp60::VerificationStatus::DocumentationDerived).data()),
                 QStringLiteral("DocumentationDerived"));
        QVERIFY(!xp60::verificationStatusLabel(xp60::VerificationStatus::Unknown).empty());
        QCOMPARE(QString::fromUtf8(xp60::verificationStatusName(xp60::VerificationStatus::ProjectDefined).data()),
                 QStringLiteral("ProjectDefined"));
    }
};

QTEST_APPLESS_MAIN(Xp60DeviceTest)
#include "tst_xp60_device.moc"
