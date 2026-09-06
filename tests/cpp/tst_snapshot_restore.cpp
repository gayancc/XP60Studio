// Phase 8 — snapshots and restore workflows.
//
// The golden fixture is a real dump of an XP-60's whole user memory, which
// makes it a real snapshot. It is not, as its name suggests, only a Patch bank:
// it holds 32 User Performances (17 blocks each), 2 User Rhythm Setups (65
// blocks each) and 128 User Patches (5 blocks each) — 1314 messages. Every
// assertion about coverage here is about data an XP-60 actually sent.

#include "services/RestorePlan.h"
#include "services/SnapshotStore.h"

#include "xp60/Xp60Device.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>

using namespace xp60studio;
using library::InstrumentSnapshot;
using library::SnapshotMetadata;
using services::RestoreArea;
using services::RestorePlan;
using services::SnapshotStore;

namespace {

roland::ByteVector readFixture()
{
    QFile file(QStringLiteral(XP60STUDIO_FIXTURE_DIR "/user-bank-amal.syx"));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QByteArray bytes = file.readAll();
    return roland::ByteVector(reinterpret_cast<const roland::Byte*>(bytes.constData()),
                              reinterpret_cast<const roland::Byte*>(bytes.constData()) + bytes.size());
}

} // namespace

class TestSnapshotRestore : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        m_fixture = readFixture();
        QVERIFY2(!m_fixture.empty(), "golden fixture missing");
        m_models = {xp60::modelId()};
    }

    // --- the snapshot itself ----------------------------------------------

    void keepsTheInstrumentsOwnBytes()
    {
        const auto snapshot = InstrumentSnapshot::fromSysEx(m_fixture, m_models);
        QVERIFY(!snapshot.isEmpty());
        QVERIFY(snapshot.isClean());
        // 32 * 17 Performance blocks + 2 * 65 Rhythm blocks + 128 * 5 Patch
        // blocks. The arithmetic is spelled out because it is the check: a
        // count that stops matching means the fixture or the parse changed.
        QCOMPARE(snapshot.messageCount(), std::size_t(32 * 17 + 2 * 65 + 128 * 5));
        QCOMPARE(snapshot.messageCount(), std::size_t(1314));

        // A snapshot is restorable byte for byte, so what comes out must be
        // exactly what went in.
        QCOMPARE(snapshot.toSysEx(), m_fixture);
    }

    void reportsTheGapsRatherThanPapingOverThem()
    {
        const auto snapshot = InstrumentSnapshot::fromSysEx(m_fixture, m_models);
        QVERIFY(!snapshot.regions().empty());
        QVERIFY(std::is_sorted(snapshot.regions().begin(), snapshot.regions().end(),
                               [](const library::SnapshotRegion& a, const library::SnapshotRegion& b) {
                                   return a.begin < b.begin;
                               }));

        // The XP-60's own layout leaves unused address space between blocks, so
        // nothing coalesces: one region per block is the map reported honestly.
        // The gaps are addresses the instrument sent nothing for, and a
        // snapshot must not claim to hold them.
        QCOMPARE(snapshot.regions().size(), snapshot.messageCount());

        int counted = 0;
        for (const auto& region : snapshot.regions()) {
            counted += region.messageCount;
            QVERIFY(region.byteCount > 0);
            QVERIFY(!region.describe().empty());
        }
        QCOMPARE(counted, 1314);
    }

    void aPartlyCoveredRangeIsNotCovered()
    {
        const auto snapshot = InstrumentSnapshot::fromSysEx(m_fixture, m_models);
        const auto base = roland::RolandAddress(0x11, 0x00, 0x00, 0x00);

        // Patch Common is 73 bytes at the Patch base.
        QVERIFY(snapshot.covers(base, 73));
        // Restoring half a Patch would leave the instrument holding a Patch
        // that never existed on it, so a range running past what was captured
        // is not covered at all.
        QVERIFY(!snapshot.covers(base, 0x4000));
        // Performance Common is 66 bytes and is present; the 16 KB slot it
        // sits in is not, because the instrument sent nothing for the gaps.
        QVERIFY(snapshot.covers(roland::RolandAddress(0x10, 0x00, 0x00, 0x00), 66));
        QVERIFY(!snapshot.covers(roland::RolandAddress(0x10, 0x00, 0x00, 0x00), 67));
    }

    void countsWhatItCouldNotReadRatherThanDroppingIt()
    {
        auto damaged = m_fixture;
        // A stray data byte between messages, and a truncated tail.
        damaged.insert(damaged.begin(), roland::Byte(0x42));
        damaged.resize(damaged.size() - 20);

        const auto snapshot = InstrumentSnapshot::fromSysEx(damaged, m_models);
        QVERIFY(!snapshot.isClean());
        QVERIFY(snapshot.strayBytes() > 0 || snapshot.rejectedMessages() > 0);
        QVERIFY(snapshot.summary().find("not a complete capture") != std::string::npos);
    }

    void anEmptySnapshotSaysSo()
    {
        const InstrumentSnapshot snapshot;
        QVERIFY(snapshot.isEmpty());
        QCOMPARE(snapshot.messageCount(), std::size_t(0));
        QVERIFY(snapshot.regions().empty());
        QCOMPARE(snapshot.summary(), std::string("Empty snapshot: nothing was captured."));
    }

    // --- saving and loading ------------------------------------------------

    void savesAPlainSyxAndAManifestBesideIt()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("before-live-set.syx"));

        SnapshotMetadata metadata;
        metadata.label = "Before the live set";
        metadata.deviceName = "XP-60";
        metadata.capturedAt = std::chrono::system_clock::time_point{std::chrono::seconds{1'700'000'000}};
        const auto snapshot = InstrumentSnapshot::fromSysEx(m_fixture, m_models, metadata);

        const auto saved = SnapshotStore::save(snapshot, path);
        QVERIFY2(saved.ok, qPrintable(saved.error));
        QVERIFY(QFile::exists(saved.sysExPath));
        QVERIFY(QFile::exists(saved.manifestPath));

        // The backup is an ordinary .syx: byte-identical to the capture, so any
        // other librarian could send it back.
        QFile file(saved.sysExPath);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray written = file.readAll();
        QCOMPARE(written.size(), qsizetype(m_fixture.size()));
        QVERIFY(std::equal(m_fixture.begin(), m_fixture.end(),
                           reinterpret_cast<const roland::Byte*>(written.constData())));
    }

    void refusesToOverwriteASnapshotUnlessTold()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("backup.syx"));
        const auto snapshot = InstrumentSnapshot::fromSysEx(m_fixture, m_models);

        QVERIFY(SnapshotStore::save(snapshot, path).ok);
        const auto second = SnapshotStore::save(snapshot, path);
        QVERIFY2(!second.ok, "a snapshot is what stands between the user and lost data");
        QVERIFY(second.error.contains(QStringLiteral("already exists")));
        QVERIFY(SnapshotStore::save(snapshot, path, /*overwrite=*/true).ok);
    }

    void refusesToSaveAnEmptySnapshot()
    {
        QTemporaryDir dir;
        const auto saved = SnapshotStore::save(InstrumentSnapshot{},
                                               dir.filePath(QStringLiteral("nothing.syx")));
        QVERIFY(!saved.ok);
        QVERIFY(!QFile::exists(saved.sysExPath));
    }

    void roundTripsThroughDisk()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("backup.syx"));
        SnapshotMetadata metadata;
        metadata.label = "Bank as shipped";
        metadata.note = "safety snapshot";
        metadata.capturedAt = std::chrono::system_clock::time_point{std::chrono::seconds{1'700'000'000}};

        const auto original = InstrumentSnapshot::fromSysEx(m_fixture, m_models, metadata);
        QVERIFY(SnapshotStore::save(original, path).ok);

        const auto loaded = SnapshotStore::load(path);
        QVERIFY2(loaded.ok, qPrintable(loaded.error));
        QVERIFY(loaded.manifestFound);
        QVERIFY(loaded.digestMatched);
        QCOMPARE(loaded.expectedDigest, original.digest());
        QCOMPARE(loaded.snapshot->messageCount(), original.messageCount());
        QCOMPARE(loaded.snapshot->toSysEx(), original.toSysEx());
        QCOMPARE(loaded.snapshot->metadata().label, std::string("Bank as shipped"));
        QCOMPARE(loaded.snapshot->metadata().note, std::string("safety snapshot"));
        QCOMPARE(loaded.snapshot->metadata().capturedAt, metadata.capturedAt);
        QVERIFY(loaded.warnings.isEmpty());
    }

    void reportsATamperedFileRatherThanRefusingIt()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("backup.syx"));
        const auto snapshot = InstrumentSnapshot::fromSysEx(m_fixture, m_models);
        QVERIFY(SnapshotStore::save(snapshot, path).ok);

        // Replace the file with a shorter capture, leaving the old manifest.
        auto shorter = m_fixture;
        shorter.resize(shorter.size() / 2);
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(reinterpret_cast<const char*>(shorter.data()), qint64(shorter.size()));
        file.close();

        const auto loaded = SnapshotStore::load(path);
        // Still loadable: it may be exactly what the user needs. What must not
        // happen is restoring it without saying this.
        QVERIFY(loaded.ok);
        QVERIFY(loaded.manifestFound);
        QVERIFY(!loaded.digestMatched);
        QVERIFY(std::any_of(loaded.warnings.begin(), loaded.warnings.end(),
                            [](const QString& warning) {
                                return warning.contains(QStringLiteral("no longer matches"));
                            }));
    }

    void aSyxWithoutAManifestLoadsWithAWarning()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("someone-elses-bank.syx"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(reinterpret_cast<const char*>(m_fixture.data()), qint64(m_fixture.size()));
        file.close();

        const auto loaded = SnapshotStore::load(path);
        QVERIFY(loaded.ok);
        QVERIFY(!loaded.manifestFound);
        QVERIFY(!loaded.digestMatched);
        QVERIFY(!loaded.warnings.isEmpty());
        QCOMPARE(loaded.snapshot->messageCount(), std::size_t(1314));

        // ...and it is not listed as a snapshot, because it is not one.
        QVERIFY(SnapshotStore::list(dir.path()).empty());
    }

    void listsSnapshotsNewestFirst()
    {
        QTemporaryDir dir;
        const auto write = [&](const QString& name, qint64 seconds) {
            SnapshotMetadata metadata;
            metadata.label = name.toStdString();
            metadata.capturedAt = std::chrono::system_clock::time_point{std::chrono::seconds{seconds}};
            const auto snapshot = InstrumentSnapshot::fromSysEx(m_fixture, m_models, metadata);
            QVERIFY(SnapshotStore::save(snapshot, dir.filePath(name + QStringLiteral(".syx"))).ok);
        };
        write(QStringLiteral("older"), 1'600'000'000);
        write(QStringLiteral("newest"), 1'800'000'000);
        write(QStringLiteral("middle"), 1'700'000'000);

        const auto listed = SnapshotStore::list(dir.path());
        QCOMPARE(listed.size(), std::size_t(3));
        QVERIFY(listed[0].contains(QStringLiteral("newest")));
        QVERIFY(listed[1].contains(QStringLiteral("middle")));
        QVERIFY(listed[2].contains(QStringLiteral("older")));
    }

    // --- the restore plan --------------------------------------------------

    void planningTheUserPatchesFromAFullBank()
    {
        const auto snapshot = InstrumentSnapshot::fromSysEx(m_fixture, m_models);
        const auto plan = RestorePlan::build(snapshot, {RestoreArea::UserPatches});

        QCOMPARE(plan.steps().size(), std::size_t(1));
        const auto& step = plan.steps().front();
        QCOMPARE(step.expectedEntries, 128);
        QCOMPARE(step.entriesWithData, 128);
        QVERIFY(step.completenessChecked());
        QCOMPARE(step.completeEntries, 128);
        QVERIFY(step.covered);
        QCOMPARE(step.messages.size(), std::size_t(640));
        QCOMPARE(step.begin, roland::RolandAddress(0x11, 0x00, 0x00, 0x00));

        QVERIFY(plan.isComplete());
        QVERIFY(plan.writesAnything());
        QCOMPARE(plan.messageCount(), std::size_t(640));
        QVERIFY(plan.unavailableAreas().empty());
        QVERIFY(plan.partialAreas().empty());
    }

    void aRestoreAlwaysRequiresASafetySnapshot()
    {
        const auto snapshot = InstrumentSnapshot::fromSysEx(m_fixture, m_models);
        const auto plan = RestorePlan::build(snapshot, {RestoreArea::UserPatches});
        QVERIFY(plan.requiresSafetySnapshot());
        // The snapshot being restored is not the safety net; it is what will
        // replace what is there now.
        const auto warnings = plan.warnings();
        QVERIFY(std::any_of(warnings.begin(), warnings.end(),
                            [](const QString& warning) {
                                return warning.contains(QStringLiteral("Take a snapshot"));
                            }));
    }

    void refusesAnAreaTheSnapshotDoesNotHold()
    {
        const auto snapshot = InstrumentSnapshot::fromSysEx(m_fixture, m_models);
        const auto plan = RestorePlan::build(snapshot, {RestoreArea::UserPatches, RestoreArea::System});

        // A user-memory dump carries no System data, and the plan says so
        // instead of quietly restoring only what it happens to have.
        QCOMPARE(plan.steps().size(), std::size_t(1));
        QCOMPARE(plan.unavailableAreas().size(), std::size_t(1));
        QCOMPARE(plan.unavailableAreas().front(), RestoreArea::System);
        QVERIFY(!plan.isComplete());
        const auto warnings = plan.warnings();
        QVERIFY(std::any_of(warnings.begin(), warnings.end(),
                            [](const QString& warning) {
                                return warning.contains(QStringLiteral("no System settings"));
                            }));
    }

    void writesNothingOutsideTheAreasAskedFor()
    {
        const auto snapshot = InstrumentSnapshot::fromSysEx(m_fixture, m_models);
        // The snapshot holds Patches, Performances and Rhythm Setups. Asking
        // for the Performances gets a Performance restore and nothing else.
        const auto plan = RestorePlan::build(snapshot, {RestoreArea::UserPerformances});

        QCOMPARE(plan.steps().size(), std::size_t(1));
        QCOMPARE(plan.messageCount(), std::size_t(32 * 17));
        QCOMPARE(plan.steps().front().expectedEntries, 32);
        QCOMPARE(plan.steps().front().entriesWithData, 32);
        QCOMPARE(plan.steps().front().completeEntries, 32);
        QVERIFY(plan.steps().front().covered);

        // Not one Patch byte is in the plan, though the snapshot is full of
        // them.
        const auto base = roland::RolandAddress(0x11, 0x00, 0x00, 0x00).value();
        for (const auto& message : plan.messages()) {
            QVERIFY(message.size() > 8);
            const auto address = roland::RolandAddress(message[5], message[6], message[7], message[8]);
            QVERIFY(address.value() < base);
        }
    }

    void rhythmSetupsAreRestorableButTheirCompletenessIsUnknown()
    {
        const auto snapshot = InstrumentSnapshot::fromSysEx(m_fixture, m_models);
        const auto plan = RestorePlan::build(snapshot, {RestoreArea::UserRhythmSetups});

        QCOMPARE(plan.steps().size(), std::size_t(1));
        const auto& step = plan.steps().front();
        QCOMPARE(step.expectedEntries, 2);
        QCOMPARE(step.entriesWithData, 2);
        QCOMPARE(step.messages.size(), std::size_t(2 * 65));
        // Rhythm Setup is transcribed but has no layout class yet, so whether
        // each Setup is whole cannot be checked. Saying "unknown" is the point:
        // the alternative is assuming it and being wrong silently.
        QVERIFY(!step.completenessChecked());
        QVERIFY(step.covered);
        const auto warnings = plan.warnings();
        QVERIFY(std::any_of(warnings.begin(), warnings.end(),
                            [](const QString& warning) {
                                return warning.contains(QStringLiteral("no block layout"));
                            }));
    }

    void aPartialBankIsPlannedAsPartial()
    {
        // Keep only the messages belonging to the first sixty Patch slots,
        // selected by address rather than by position in the file, so the test
        // does not depend on the order the dump happens to be in.
        const auto whole = InstrumentSnapshot::fromSysEx(m_fixture, m_models);
        const auto base = roland::RolandAddress(0x11, 0x00, 0x00, 0x00).value();
        const std::uint32_t limit = base + 60 * (0x01u << 14);
        std::vector<roland::ByteVector> messages;
        for (const auto& message : whole.messages()) {
            const auto address
                = roland::RolandAddress(message[5], message[6], message[7], message[8]).value();
            if (address >= base && address < limit) {
                messages.push_back(message);
            }
        }
        QCOMPARE(messages.size(), std::size_t(300));
        const auto partial = InstrumentSnapshot::fromDataSets(std::move(messages), m_models);

        const auto plan = RestorePlan::build(partial, {RestoreArea::UserPatches});
        QCOMPARE(plan.steps().size(), std::size_t(1));
        const auto& step = plan.steps().front();
        QCOMPARE(step.entriesWithData, 60);
        QCOMPARE(step.completeEntries, 60);
        QVERIFY(!step.covered);
        QVERIFY(!plan.isComplete());
        QCOMPARE(plan.partialAreas().size(), std::size_t(1));

        const auto warnings = plan.warnings();
        QVERIFY(std::any_of(warnings.begin(), warnings.end(),
                            [](const QString& warning) {
                                return warning.contains(QStringLiteral("holds 60 of the 128"));
                            }));
    }

    void aPatchMissingABlockIsCountedButNotComplete()
    {
        const auto whole = InstrumentSnapshot::fromSysEx(m_fixture, m_models);
        auto messages = whole.messages();
        // Drop one block of User Patch 001, found by address. The slot still
        // has data, so it is present — but it is not whole, and the plan must
        // not confuse the two.
        const auto patchBase = roland::RolandAddress(0x11, 0x00, 0x00, 0x00).value();
        const auto victim = std::find_if(messages.begin(), messages.end(),
                                         [patchBase](const roland::ByteVector& message) {
                                             return roland::RolandAddress(message[5], message[6],
                                                                          message[7], message[8])
                                                        .value()
                                                 == patchBase;
                                         });
        QVERIFY(victim != messages.end());
        messages.erase(victim);
        const auto snapshot = InstrumentSnapshot::fromDataSets(std::move(messages), m_models);

        const auto plan = RestorePlan::build(snapshot, {RestoreArea::UserPatches});
        const auto& step = plan.steps().front();
        QCOMPARE(step.entriesWithData, 128);
        QCOMPARE(step.completeEntries, 127);
        QVERIFY(!step.covered);
        const auto warnings = plan.warnings();
        QVERIFY(std::any_of(warnings.begin(), warnings.end(),
                            [](const QString& warning) {
                                return warning.contains(QStringLiteral("missing some of their data"));
                            }));
    }

    void everythingCoveredPlansOnlyWhatIsThere()
    {
        const auto snapshot = InstrumentSnapshot::fromSysEx(m_fixture, m_models);
        const auto plan = RestorePlan::buildForEverythingCovered(snapshot);

        // The three user areas the dump holds, and not System, which it does
        // not.
        QCOMPARE(plan.steps().size(), std::size_t(3));
        QVERIFY(plan.unavailableAreas().empty());
        QVERIFY(plan.isComplete());
        QCOMPARE(plan.messageCount(), snapshot.messageCount());
    }

    void thePlansMessagesAreTheSnapshotsOwnBytes()
    {
        const auto snapshot = InstrumentSnapshot::fromSysEx(m_fixture, m_models);
        const auto plan = RestorePlan::build(snapshot, {RestoreArea::UserPatches});
        const auto messages = plan.messages();

        QCOMPARE(messages.size(), std::size_t(640));
        // Every message in the plan is one of the snapshot's own, unaltered.
        for (const auto& message : messages) {
            QVERIFY(std::find(snapshot.messages().begin(), snapshot.messages().end(), message)
                    != snapshot.messages().end());
        }
    }

private:
    roland::ByteVector m_fixture;
    std::vector<roland::RolandModelId> m_models;
};

QTEST_MAIN(TestSnapshotRestore)
#include "tst_snapshot_restore.moc"
