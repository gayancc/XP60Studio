// Phase 5 — `.syx` export and the observable import service.
//
// Both run against tests/fixtures/xp60/user-bank-amal.syx, a real XP-60 user
// bank, so an exported file is checked by re-importing it and comparing real
// Patches rather than invented bytes.

#include "library/LibraryDatabase.h"
#include "library/SyxExport.h"
#include "library/SyxImport.h"
#include "services/LibraryImportService.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>

using namespace xp60studio;
using library::LibraryDatabase;
using library::LibraryEntry;
using library::SyxExportOptions;
using library::SyxExportSource;
using library::SyxExportTarget;
using services::LibraryImportService;

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

bool writeFile(const QString& path, roland::ByteSpan bytes)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    const auto written = file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<qint64>(bytes.size()));
    return written == static_cast<qint64>(bytes.size());
}

} // namespace

class TestLibraryExport : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // Export
    void writesTheOriginalBytesBackVerbatim();
    void exportedOriginalBytesReimportIdentically();
    void refusesToCallReencodedBytesOriginal();
    void reencodesIntoConsecutiveUserSlotsAndSaysSo();
    void reencodedExportPreservesEveryParameter();
    void exportsOnePatchToTheTemporaryArea();
    void refusesExportsThatWouldLoseData();
    void honoursThePayloadLimit();

    // Import service
    void importsSeveralFilesReportingProgress();
    void reportsAnUnreadableFileWithoutStoppingTheBatch();
    void reportsDuplicatesAlreadyInTheLibrary();
    void cancelsBetweenFilesKeepingCommittedWork();
    void reportsAFileWithNoPatchesWithoutCallingItAnError();

private:
    [[nodiscard]] std::vector<LibraryEntry> fixtureEntries() const;

    roland::ByteVector m_fixture;
};

void TestLibraryExport::initTestCase()
{
    m_fixture = readFixture();
    QVERIFY2(!m_fixture.empty(), "golden fixture missing");
}

std::vector<LibraryEntry> TestLibraryExport::fixtureEntries() const
{
    library::SyxImportOptions options;
    options.sourceName = "user-bank-amal.syx";
    options.importedAt = std::chrono::system_clock::time_point{std::chrono::seconds{1'700'000'000}};
    return library::importSyxStream(m_fixture, options).entries;
}

// ---------------------------------------------------------------------------
// Export
// ---------------------------------------------------------------------------

void TestLibraryExport::writesTheOriginalBytesBackVerbatim()
{
    const auto entries = fixtureEntries();
    QCOMPARE(entries.size(), std::size_t{128});

    const auto result = library::exportEntries(entries);
    QVERIFY2(result.ok, qPrintable(QString::fromStdString(result.error)));
    QCOMPARE(result.patchCount, std::size_t{128});
    QCOMPARE(result.messageCount, std::size_t{128 * 5});
    QVERIFY(result.notes.empty()); // nothing was changed, so nothing to report

    // The bytes are the concatenation of what each entry preserved, in order.
    roland::ByteVector expected;
    for (const auto& entry : entries) {
        expected.insert(expected.end(), entry.originalSysEx().begin(), entry.originalSysEx().end());
    }
    QCOMPARE(result.bytes, expected);
}

void TestLibraryExport::exportedOriginalBytesReimportIdentically()
{
    const auto entries = fixtureEntries();
    const auto exported = library::exportEntries(entries);
    QVERIFY(exported.ok);

    const auto reimported = library::importSyxStream(exported.bytes);
    QCOMPARE(reimported.entries.size(), entries.size());
    QVERIFY(reimported.stream.isClean());
    QVERIFY(reimported.partial.empty());
    QVERIFY(reimported.rejected.empty());
    // Nothing outside the Patches survives an original-bytes export, because
    // only the Patches' own messages were kept at import.
    QCOMPARE(reimported.unattributedDataSets, std::size_t{0});

    for (std::size_t i = 0; i < entries.size(); ++i) {
        QVERIFY(reimported.entries[i].hasSameParameters(entries[i]));
        QCOMPARE(reimported.entries[i].originalSysEx(), entries[i].originalSysEx());
        QCOMPARE(reimported.entries[i].provenance().userNumber, entries[i].provenance().userNumber);
    }
}

void TestLibraryExport::refusesToCallReencodedBytesOriginal()
{
    const auto entries = fixtureEntries();

    // Moving Patches means re-encoding. Saying "original bytes" and getting
    // re-encoded ones would misdescribe what the file contains.
    SyxExportOptions moving;
    moving.source = SyxExportSource::OriginalBytes;
    moving.target.kind = SyxExportTarget::Kind::UserBankFrom;
    const auto moved = library::exportEntries(entries, moving);
    QVERIFY(!moved.ok);
    QVERIFY(QString::fromStdString(moved.error).contains(QStringLiteral("Re-addressing")));

    // So does changing the device ID.
    SyxExportOptions rebadged;
    rebadged.source = SyxExportSource::OriginalBytes;
    rebadged.deviceId = roland::RolandDeviceId::fromDisplayNumber(20);
    const auto badged = library::exportEntries(entries, rebadged);
    QVERIFY(!badged.ok);
    QVERIFY(QString::fromStdString(badged.error).contains(QStringLiteral("device ID")));
}

void TestLibraryExport::reencodesIntoConsecutiveUserSlotsAndSaysSo()
{
    auto entries = fixtureEntries();
    entries.erase(entries.begin() + 4, entries.end()); // USER:001..004

    SyxExportOptions options;
    options.source = SyxExportSource::ReencodedFromModel;
    options.target.kind = SyxExportTarget::Kind::UserBankFrom;
    options.target.firstUserNumber = 10;

    const auto result = library::exportEntries(entries, options);
    QVERIFY2(result.ok, qPrintable(QString::fromStdString(result.error)));
    QCOMPARE(result.patchCount, std::size_t{4});
    // Every one moved, and every move is reported.
    QCOMPARE(result.notes.size(), std::size_t{4});
    QVERIFY(QString::fromStdString(result.notes.front()).contains(QStringLiteral("USER:010")));
    QVERIFY(QString::fromStdString(result.notes.front()).contains(QStringLiteral("from USER:001")));

    const auto reimported = library::importSyxStream(result.bytes);
    QCOMPARE(reimported.entries.size(), std::size_t{4});
    for (std::size_t i = 0; i < reimported.entries.size(); ++i) {
        QCOMPARE(reimported.entries[i].provenance().userNumber, std::optional<int>{10 + static_cast<int>(i)});
        QVERIFY(reimported.entries[i].hasSameParameters(entries[i]));
    }
}

void TestLibraryExport::reencodedExportPreservesEveryParameter()
{
    const auto entries = fixtureEntries();

    SyxExportOptions options;
    options.source = SyxExportSource::ReencodedFromModel;
    const auto result = library::exportEntries(entries, options);
    QVERIFY2(result.ok, qPrintable(QString::fromStdString(result.error)));
    // Kept where they were, so nothing to report.
    QVERIFY(result.notes.empty());

    const auto reimported = library::importSyxStream(result.bytes);
    QCOMPARE(reimported.entries.size(), std::size_t{128});
    for (std::size_t i = 0; i < entries.size(); ++i) {
        QVERIFY(reimported.entries[i].hasSameParameters(entries[i]));
        QCOMPARE(reimported.entries[i].fingerprint(), entries[i].fingerprint());
    }
    // Re-encoding does NOT reproduce the instrument's own message shape, and
    // this is the reason `OriginalBytes` exists. The fixture delivers each
    // 129-byte Tone block in one DT1; the encoder splits at the documented
    // 128-byte limit, so a Patch becomes 1 Common + 4 x 2 Tone messages.
    // See ROLAND_XP60_PROTOCOL_FACTS.md §2.1: which behaviour the XP-60
    // actually requires is still open, so sending stays conservative.
    QCOMPARE(result.messageCount, std::size_t{128 * 9});
    QVERIFY(result.bytes != m_fixture);
}

void TestLibraryExport::exportsOnePatchToTheTemporaryArea()
{
    const auto entries = fixtureEntries();

    SyxExportOptions options;
    options.source = SyxExportSource::ReencodedFromModel;
    options.target.kind = SyxExportTarget::Kind::TemporaryPatch;

    const auto result = library::exportEntry(entries.front(), options);
    QVERIFY2(result.ok, qPrintable(QString::fromStdString(result.error)));
    QCOMPARE(result.patchCount, std::size_t{1});
    QCOMPARE(result.notes.size(), std::size_t{1});

    const auto reimported = library::importSyxStream(result.bytes);
    QCOMPARE(reimported.entries.size(), std::size_t{1});
    QCOMPARE(reimported.entries.front().provenance().address,
             xpmodel::Xp60PatchLayout::temporaryPatchAddress());
    QVERIFY(!reimported.entries.front().provenance().userNumber.has_value());
    QVERIFY(reimported.entries.front().hasSameParameters(entries.front()));
}

void TestLibraryExport::refusesExportsThatWouldLoseData()
{
    const auto entries = fixtureEntries();

    // Several Patches cannot share one edit buffer.
    SyxExportOptions crowded;
    crowded.source = SyxExportSource::ReencodedFromModel;
    crowded.target.kind = SyxExportTarget::Kind::TemporaryPatch;
    const auto crowdedResult = library::exportEntries(entries, crowded);
    QVERIFY(!crowdedResult.ok);
    QVERIFY(QString::fromStdString(crowdedResult.error).contains(QStringLiteral("one Patch")));

    // 128 patches starting at slot 10 would run off the end of the bank.
    SyxExportOptions overflowing;
    overflowing.source = SyxExportSource::ReencodedFromModel;
    overflowing.target.kind = SyxExportTarget::Kind::UserBankFrom;
    overflowing.target.firstUserNumber = 10;
    const auto overflowed = library::exportEntries(entries, overflowing);
    QVERIFY(!overflowed.ok);
    QVERIFY(QString::fromStdString(overflowed.error).contains(QStringLiteral("128-slot")));

    // Slot 0 does not exist.
    overflowing.target.firstUserNumber = 0;
    QVERIFY(!library::exportEntry(entries.front(), overflowing).ok);

    QVERIFY(!library::exportEntries({}).ok);
}

void TestLibraryExport::honoursThePayloadLimit()
{
    const auto entries = fixtureEntries();

    SyxExportOptions small;
    small.source = SyxExportSource::ReencodedFromModel;
    small.maxPayloadBytes = 32;
    const auto result = library::exportEntry(entries.front(), small);
    QVERIFY2(result.ok, qPrintable(QString::fromStdString(result.error)));
    // More, smaller messages; the same Patch on the other side.
    QVERIFY(result.messageCount > 5);
    const auto reimported = library::importSyxStream(result.bytes);
    QCOMPARE(reimported.entries.size(), std::size_t{1});
    QVERIFY(reimported.entries.front().hasSameParameters(entries.front()));

    // The documented XP-60 limit is 128 bytes; the export will not exceed it.
    SyxExportOptions oversized;
    oversized.source = SyxExportSource::ReencodedFromModel;
    oversized.maxPayloadBytes = 256;
    const auto refused = library::exportEntry(entries.front(), oversized);
    QVERIFY(!refused.ok);
    QVERIFY(QString::fromStdString(refused.error).contains(QStringLiteral("documented limit")));
}

// ---------------------------------------------------------------------------
// Import service
// ---------------------------------------------------------------------------

void TestLibraryExport::importsSeveralFilesReportingProgress()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto entries = fixtureEntries();

    // Two real files: the whole bank, and its first four patches.
    const QString bankPath = directory.filePath(QStringLiteral("bank.syx"));
    QVERIFY(writeFile(bankPath, m_fixture));

    std::vector<LibraryEntry> few(entries.begin(), entries.begin() + 4);
    const auto fewBytes = library::exportEntries(few);
    QVERIFY(fewBytes.ok);
    const QString fewPath = directory.filePath(QStringLiteral("four.syx"));
    QVERIFY(writeFile(fewPath, fewBytes.bytes));

    LibraryDatabase database;
    QVERIFY(database.open(QString::fromLatin1(LibraryDatabase::kInMemoryPath)));
    LibraryImportService service(database);

    QSignalSpy startedSpy(&service, &LibraryImportService::started);
    QSignalSpy progressSpy(&service, &LibraryImportService::progressChanged);
    QSignalSpy fileFinishedSpy(&service, &LibraryImportService::fileFinished);
    QSignalSpy finishedSpy(&service, &LibraryImportService::finished);

    QVERIFY(service.importFiles({bankPath, fewPath}));
    QVERIFY(finishedSpy.wait(30'000));

    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(startedSpy.front().front().toInt(), 2);
    QCOMPARE(fileFinishedSpy.count(), 2);
    // One "0 of 2" before any work, then one per completed file.
    QCOMPARE(progressSpy.count(), 3);
    QCOMPARE(progressSpy.back().front().toInt(), 2);

    const auto& summary = service.lastSummary();
    QVERIFY(!summary.cancelled);
    QCOMPARE(summary.filesFailed, std::size_t{0});
    QCOMPARE(summary.patchesStored, std::size_t{132});
    QCOMPARE(database.totalCount(), std::optional<int>{132});

    // The bank's Performance blocks are reported, not imported as Patches.
    QCOMPARE(summary.files.front().patchesStored, std::size_t{128});
    QCOMPARE(summary.files.front().unattributedDataSets, std::size_t{1314 - 128 * 5});
    QVERIFY(summary.files.front().describe().contains(QStringLiteral("128 patches stored")));
    QVERIFY(service.lastSummary().summary().contains(QStringLiteral("132 patches stored")));
}

void TestLibraryExport::reportsAnUnreadableFileWithoutStoppingTheBatch()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString goodPath = directory.filePath(QStringLiteral("good.syx"));
    const auto entries = fixtureEntries();
    const auto exported = library::exportEntries({entries.begin(), entries.begin() + 2});
    QVERIFY(exported.ok);
    QVERIFY(writeFile(goodPath, exported.bytes));

    const QString emptyPath = directory.filePath(QStringLiteral("empty.syx"));
    QVERIFY(writeFile(emptyPath, roland::ByteVector{}));
    const QString missingPath = directory.filePath(QStringLiteral("missing.syx"));

    LibraryDatabase database;
    QVERIFY(database.open(QString::fromLatin1(LibraryDatabase::kInMemoryPath)));
    LibraryImportService service(database);
    QSignalSpy finishedSpy(&service, &LibraryImportService::finished);

    QVERIFY(service.importFiles({missingPath, goodPath, emptyPath}));
    QVERIFY(finishedSpy.wait(30'000));

    const auto& summary = service.lastSummary();
    QCOMPARE(summary.files.size(), std::size_t{3});
    QCOMPARE(summary.filesFailed, std::size_t{2});
    // The readable file still imported: one bad file does not lose the batch.
    QCOMPARE(summary.patchesStored, std::size_t{2});
    QCOMPARE(database.totalCount(), std::optional<int>{2});

    QVERIFY(!summary.files[0].ok());
    QVERIFY(summary.files[0].error.contains(QStringLiteral("No such file")));
    QVERIFY(summary.files[1].ok());
    QVERIFY(!summary.files[2].ok());
    QVERIFY(summary.files[2].error.contains(QStringLiteral("empty")));
}

void TestLibraryExport::reportsDuplicatesAlreadyInTheLibrary()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto entries = fixtureEntries();
    const auto exported = library::exportEntries({entries.begin(), entries.begin() + 3});
    QVERIFY(exported.ok);
    const QString path = directory.filePath(QStringLiteral("three.syx"));
    QVERIFY(writeFile(path, exported.bytes));

    LibraryDatabase database;
    QVERIFY(database.open(QString::fromLatin1(LibraryDatabase::kInMemoryPath)));
    LibraryImportService service(database);
    QSignalSpy finishedSpy(&service, &LibraryImportService::finished);

    QVERIFY(service.importFiles({path, path})); // the same file twice
    QVERIFY(finishedSpy.wait(30'000));

    const auto& summary = service.lastSummary();
    QCOMPARE(summary.files.size(), std::size_t{2});
    // Nothing is discarded: six entries, each with its own provenance.
    QCOMPARE(summary.patchesStored, std::size_t{6});
    QCOMPARE(database.totalCount(), std::optional<int>{6});
    // The first pass found nothing already there; the second found all three.
    QCOMPARE(summary.files[0].duplicatesInLibrary, std::size_t{0});
    QCOMPARE(summary.files[1].duplicatesInLibrary, std::size_t{3});
    QVERIFY(summary.files[1].describe().contains(QStringLiteral("already in the library")));
}

void TestLibraryExport::cancelsBetweenFilesKeepingCommittedWork()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString bankPath = directory.filePath(QStringLiteral("bank.syx"));
    QVERIFY(writeFile(bankPath, m_fixture));

    LibraryDatabase database;
    QVERIFY(database.open(QString::fromLatin1(LibraryDatabase::kInMemoryPath)));
    LibraryImportService service(database);
    QSignalSpy cancelledSpy(&service, &LibraryImportService::cancelled);
    QSignalSpy finishedSpy(&service, &LibraryImportService::finished);

    // Cancel as soon as the first file is done, before the rest are read.
    connect(&service, &LibraryImportService::fileFinished, &service,
            [&service] { service.cancel(); }, Qt::DirectConnection);

    QVERIFY(service.importFiles({bankPath, bankPath, bankPath, bankPath}));
    QVERIFY(finishedSpy.wait(60'000));

    const auto& summary = service.lastSummary();
    QVERIFY(summary.cancelled);
    QCOMPARE(cancelledSpy.count(), 1);
    // The file that had already been committed stays committed and reported.
    QVERIFY(!summary.files.empty());
    QVERIFY(summary.files.size() < 4);
    QCOMPARE(summary.patchesStored, static_cast<std::size_t>(128 * summary.files.size()));
    QCOMPARE(database.totalCount(), std::optional<int>{static_cast<int>(summary.patchesStored)});

    // A cancel with nothing running is refused rather than pretending.
    QVERIFY(!service.cancel());
    QVERIFY(!service.isRunning());
}

void TestLibraryExport::reportsAFileWithNoPatchesWithoutCallingItAnError()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    // A single Patch Common block: real bytes, but not a whole Patch.
    const auto entries = fixtureEntries();
    const auto whole = library::exportEntry(entries.front());
    QVERIFY(whole.ok);
    const auto firstMessageEnd =
        std::find(whole.bytes.begin(), whole.bytes.end(), roland::kSysExEnd) + 1;
    const roland::ByteVector partial(whole.bytes.begin(), firstMessageEnd);

    const QString path = directory.filePath(QStringLiteral("partial.syx"));
    QVERIFY(writeFile(path, partial));

    LibraryDatabase database;
    QVERIFY(database.open(QString::fromLatin1(LibraryDatabase::kInMemoryPath)));
    LibraryImportService service(database);
    QSignalSpy finishedSpy(&service, &LibraryImportService::finished);

    QVERIFY(service.importFiles({path}));
    QVERIFY(finishedSpy.wait(30'000));

    const auto& summary = service.lastSummary();
    QCOMPARE(summary.files.size(), std::size_t{1});
    // Readable and understood; it simply held no complete Patch. That is
    // information, not a failure — and the incomplete Patch is reported.
    QVERIFY(summary.files.front().ok());
    QCOMPARE(summary.files.front().patchesStored, std::size_t{0});
    QCOMPARE(summary.files.front().partial, std::size_t{1});
    QVERIFY(summary.files.front().describe().contains(QStringLiteral("incomplete patch")));
    QCOMPARE(database.totalCount(), std::optional<int>{0});
}

QTEST_MAIN(TestLibraryExport)
#include "tst_library_export.moc"
