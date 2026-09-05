#include "xp60/Xp60Device.h"

#include <QtTest>

#include <algorithm>
#include <array>
#include <string_view>

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
        // Hardware-verified 2026-09-04: every reply from a physical XP-60 carried
        // "41 <dev> 6A 12" after F0. See docs/HARDWARE_VALIDATION_XP60.md.
        QCOMPARE(xp60::modelIdStatus(), xp60::VerificationStatus::HardwareVerified);
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
        }
        // Only regions actually read on hardware may claim verification. The
        // four below were read on 2026-09-04; the rest have never been
        // addressed, and promoting one without a capture is the mistake this
        // guards against.
        const std::array<std::string_view, 4> verified{
            {"system", "temporary-performance", "temporary-patch", "user-patch"}};
        for (const auto& region : regions) {
            const bool expectVerified = std::find(verified.begin(), verified.end(), region.id) != verified.end();
            QCOMPARE(region.status == xp60::VerificationStatus::HardwareVerified, expectVerified);
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
        // All three documented presets were answered by a physical XP-60 on
        // 2026-09-04. The reply to presets[2] carried 466 payload bytes, not
        // 3993: the size is an address span over padded blocks.
        QCOMPARE(presets[0].status, xp60::VerificationStatus::HardwareVerified);
        QCOMPARE(presets[1].status, xp60::VerificationStatus::HardwareVerified);
        QCOMPARE(presets[2].status, xp60::VerificationStatus::HardwareVerified);
        // The partial System read is a project choice, never labelled as
        // Roland-documented. The instrument answering it does not make the
        // 16-byte size a Roland fact, so this stays ProjectDefined.
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
