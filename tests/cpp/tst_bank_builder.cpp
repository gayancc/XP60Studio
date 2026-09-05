// The Bank Builder's two file-facing verbs, checked below QML.
//
// The panel's arrangement rules live in tst_bank_draft and tst_bank_location,
// and its surface behaviour in tests/qml/tst_BankBuilderScreen.qml. What is
// checked here is the pair that crosses into files and the library:
//
//   * filling a bank from an import source, which must put every Patch back at
//     the User slot it was read from and must never guess one it wasn't given;
//   * exporting the built bank, which must address each Patch to the
//     destination it occupies and leave the gaps as gaps.
//
// Both run against tests/fixtures/xp60/user-bank-amal.syx — a real XP-60 user
// bank — and the export is checked by re-importing the file it wrote.

#include "library/BankDraft.h"
#include "library/LibraryDatabase.h"
#include "library/SyxImport.h"
#include "presentation/BankBuilderViewModel.h"
#include "presentation/LibraryTransferViewModel.h"
#include "services/LibraryExportService.h"
#include "services/LibraryImportService.h"
#include "xpmodel/Xp60BankLocation.h"

#include <QFile>
#include <QSqlDatabase>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include <memory>

using namespace xp60studio;
using library::LibraryDatabase;
using library::LibraryEntry;
using presentation::BankBuilderViewModel;
using presentation::LibraryTransferViewModel;
using xpmodel::Xp60BankLocation;

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

int slotOf(const char* panelLabel)
{
    const auto location = Xp60BankLocation::fromPanelLabel(panelLabel);
    return location ? location->slotIndex() : -1;
}

// How the fixture is varied for one test: how much of it to import, and which
// entry's recorded User slot to remove or duplicate.
struct ImportOptions
{
    int limit = 0;                  // 0 = all 128
    int forgetUserNumberAt = -1;    // index into the imported entries
    int duplicateUserNumberAt = -1; // index whose User number is copied from its predecessor
    std::string sourceName = "user-bank-amal.syx";
};

} // namespace

class TestBankBuilder : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    void fillsABankFromTheSlotsASourceRecords();
    void leavesPatchesWithNoRecordedSlotUnplacedAndSaysSo();
    void keepsTheFirstPatchWhenTwoClaimOneDestination();
    void fillingIsOneUndoStepAndAddsToWhatIsThere();
    void refusesToFillFromASourceThatIsNotThere();

    void reportsDuplicateSoundsWithoutActingOnThem();
    void exportsEachPatchToTheDestinationItOccupies();
    void refusesToExportAnEmptyBank();

private:
    // Imports the fixture into the database and returns the source digest the
    // import computed from the bytes.
    QString importFixture(const ImportOptions& options = ImportOptions{});

    roland::ByteVector m_fixture;
    LibraryDatabase m_db;
};

void TestBankBuilder::initTestCase()
{
    m_fixture = readFixture();
    QVERIFY2(!m_fixture.empty(), "golden fixture missing");
    QVERIFY2(QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE")),
             "this Qt build has no QSQLITE driver");
}

void TestBankBuilder::init()
{
    m_db.close();
    QVERIFY2(m_db.open(QString::fromLatin1(LibraryDatabase::kInMemoryPath)), qPrintable(m_db.lastError()));
}

QString TestBankBuilder::importFixture(const ImportOptions& options)
{
    library::SyxImportOptions importOptions;
    importOptions.sourceName = options.sourceName;
    importOptions.importedAt = std::chrono::system_clock::time_point{std::chrono::seconds{1'700'000'000}};
    auto result = library::importSyxStream(m_fixture, importOptions);

    if (options.limit > 0 && static_cast<int>(result.entries.size()) > options.limit) {
        result.entries.erase(result.entries.begin() + options.limit, result.entries.end());
    }
    // Provenance is recorded once at import and never mutated afterwards, so
    // a variation is a new entry built from the same Patch and bytes.
    const auto rebuild = [](const LibraryEntry& entry, library::PatchProvenance provenance) {
        return LibraryEntry(entry.patch(), entry.originalSysEx(), std::move(provenance));
    };
    if (options.forgetUserNumberAt >= 0 && options.forgetUserNumberAt < static_cast<int>(result.entries.size())) {
        const auto index = static_cast<std::size_t>(options.forgetUserNumberAt);
        auto provenance = result.entries[index].provenance();
        provenance.userNumber.reset();
        result.entries[index] = rebuild(result.entries[index], std::move(provenance));
    }
    if (options.duplicateUserNumberAt > 0
        && options.duplicateUserNumberAt < static_cast<int>(result.entries.size())) {
        const auto index = static_cast<std::size_t>(options.duplicateUserNumberAt);
        auto provenance = result.entries[index].provenance();
        provenance.userNumber = result.entries[index - 1].provenance().userNumber;
        result.entries[index] = rebuild(result.entries[index], std::move(provenance));
    }

    const auto ids = m_db.insertAll(result.entries);
    if (!ids) {
        return {};
    }
    return QString::fromStdString(result.sourceDigest);
}

// ---------------------------------------------------------------------------
// Filling from a source
// ---------------------------------------------------------------------------

void TestBankBuilder::fillsABankFromTheSlotsASourceRecords()
{
    const auto digest = importFixture();
    QVERIFY(!digest.isEmpty());

    BankBuilderViewModel builder;
    builder.setDatabase(&m_db);

    const auto report = builder.fillFromSource(digest);
    QVERIFY2(report.value(QStringLiteral("ok")).toBool(),
             qPrintable(report.value(QStringLiteral("message")).toString()));
    QCOMPARE(report.value(QStringLiteral("placed")).toInt(), 128);
    QCOMPARE(report.value(QStringLiteral("unplaced")).toInt(), 0);
    QCOMPARE(report.value(QStringLiteral("conflicts")).toInt(), 0);
    QCOMPARE(builder.occupiedCount(), 128);

    // A bank read from USER:001..128 comes back arranged exactly that way: the
    // first destination holds the first Patch and the last holds the last.
    const auto first = builder.destinationAt(slotOf("A11"));
    const auto last = builder.destinationAt(slotOf("B88"));
    QCOMPARE(first.value(QStringLiteral("sourceSlot")).toString(), QStringLiteral("USER:001"));
    QCOMPARE(last.value(QStringLiteral("sourceSlot")).toString(), QStringLiteral("USER:128"));
    QVERIFY(!first.value(QStringLiteral("patchName")).toString().isEmpty());
}

void TestBankBuilder::leavesPatchesWithNoRecordedSlotUnplacedAndSaysSo()
{
    ImportOptions options;
    options.limit = 6;
    options.forgetUserNumberAt = 2;
    const auto digest = importFixture(options);
    QVERIFY(!digest.isEmpty());

    BankBuilderViewModel builder;
    builder.setDatabase(&m_db);

    const auto report = builder.fillFromSource(digest);
    QVERIFY(report.value(QStringLiteral("ok")).toBool());
    QCOMPARE(report.value(QStringLiteral("placed")).toInt(), 5);
    // Not dropped into the first free destination: a slot nobody recorded is
    // one this application must not invent.
    QCOMPARE(report.value(QStringLiteral("unplaced")).toInt(), 1);
    QCOMPARE(builder.occupiedCount(), 5);
    QCOMPARE(builder.lastActionTone(), QStringLiteral("warning"));
    QVERIFY(builder.lastAction().contains(QStringLiteral("no User slot")));
}

void TestBankBuilder::keepsTheFirstPatchWhenTwoClaimOneDestination()
{
    ImportOptions options;
    options.limit = 4;
    options.duplicateUserNumberAt = 3; // entry 4 now claims entry 3's slot
    const auto digest = importFixture(options);
    QVERIFY(!digest.isEmpty());

    BankBuilderViewModel builder;
    builder.setDatabase(&m_db);

    const auto report = builder.fillFromSource(digest);
    QVERIFY(report.value(QStringLiteral("ok")).toBool());
    QCOMPARE(report.value(QStringLiteral("placed")).toInt(), 3);
    QCOMPARE(report.value(QStringLiteral("conflicts")).toInt(), 1);
    QCOMPARE(builder.occupiedCount(), 3);
    QCOMPARE(builder.lastActionTone(), QStringLiteral("warning"));
}

void TestBankBuilder::fillingIsOneUndoStepAndAddsToWhatIsThere()
{
    ImportOptions options;
    options.limit = 8;
    const auto digest = importFixture(options);
    QVERIFY(!digest.isEmpty());

    BankBuilderViewModel builder;
    builder.setDatabase(&m_db);

    // Something the musician placed by hand, at a destination this source
    // says nothing about.
    const auto firstId = m_db.search({}).front().id;
    QVERIFY(builder.placePatch(slotOf("B88"), firstId));
    QCOMPARE(builder.occupiedCount(), 1);

    QVERIFY(builder.fillFromSource(digest).value(QStringLiteral("ok")).toBool());
    // Eight from the source plus the one placed by hand, which the fill did
    // not touch.
    QCOMPARE(builder.occupiedCount(), 9);
    QCOMPARE(builder.patchIdAt(slotOf("B88")), firstId);

    QVERIFY(builder.undo());
    QCOMPARE(builder.occupiedCount(), 1);
    QCOMPARE(builder.patchIdAt(slotOf("B88")), firstId);
    QVERIFY(builder.redo());
    QCOMPARE(builder.occupiedCount(), 9);
}

void TestBankBuilder::refusesToFillFromASourceThatIsNotThere()
{
    BankBuilderViewModel builder;
    builder.setDatabase(&m_db);

    const auto report = builder.fillFromSource(QStringLiteral("no-such-digest"));
    QVERIFY(!report.value(QStringLiteral("ok")).toBool());
    QCOMPARE(builder.occupiedCount(), 0);
    QVERIFY(!builder.modified());
}

// A bank holding the same sound twice is worth knowing about and is never an
// error: filling four destinations from one Patch is a documented thing to do,
// and keeping two copies of a sound is the musician's business. The surface
// reports it and distinguishes the two ways it can happen.
void TestBankBuilder::reportsDuplicateSoundsWithoutActingOnThem()
{
    ImportOptions options;
    options.limit = 4;
    const auto digest = importFixture(options);
    QVERIFY(!digest.isEmpty());
    // Import the same file again, so the library holds two rows per sound.
    QVERIFY(!importFixture(options).isEmpty());

    BankBuilderViewModel builder;
    builder.setDatabase(&m_db);
    const auto records = m_db.search({});
    QCOMPARE(static_cast<int>(records.size()), 8);

    // Find two rows that are different library Patches with the same parameters,
    // and one that is neither.
    std::int64_t first = 0;
    std::int64_t twin = 0;
    std::int64_t other = 0;
    for (const auto& candidate : records) {
        if (first == 0) {
            first = candidate.id;
            continue;
        }
        if (twin == 0 && candidate.fingerprint == records.front().fingerprint) {
            twin = candidate.id;
        } else if (other == 0 && !(candidate.fingerprint == records.front().fingerprint)) {
            other = candidate.id;
        }
    }
    QVERIFY(first > 0 && twin > 0 && other > 0);

    QVERIFY(builder.placePatch(slotOf("A11"), first));
    QCOMPARE(builder.duplicateCount(), 0);

    // A different library row holding the same sound.
    QVERIFY(builder.placePatch(slotOf("A12"), twin));
    QCOMPARE(builder.duplicateCount(), 2);
    const auto flagged = builder.destinationAt(slotOf("A12"));
    QVERIFY(flagged.value(QStringLiteral("duplicate")).toBool());
    QVERIFY2(flagged.value(QStringLiteral("duplicateNote")).toString().contains(QStringLiteral("another name")),
             qPrintable(flagged.value(QStringLiteral("duplicateNote")).toString()));
    // Both ends of the pair say so, not just the second one.
    QVERIFY(builder.destinationAt(slotOf("A11")).value(QStringLiteral("duplicate")).toBool());

    // A genuinely different sound is left alone.
    QVERIFY(builder.placePatch(slotOf("A13"), other));
    QCOMPARE(builder.duplicateCount(), 2);
    QVERIFY(!builder.destinationAt(slotOf("A13")).value(QStringLiteral("duplicate")).toBool());

    // The same library Patch in two destinations is the other kind of
    // duplicate, and is named differently because it means something different.
    QVERIFY(builder.placePatch(slotOf("A14"), other));
    QCOMPARE(builder.duplicateCount(), 4);
    QVERIFY(builder.destinationAt(slotOf("A14")).value(QStringLiteral("duplicateNote")).toString()
                .contains(QStringLiteral("same Patch")));

    // Nothing was acted on: every destination still holds what it was given.
    QCOMPARE(builder.occupiedCount(), 4);
    QCOMPARE(builder.patchIdAt(slotOf("A12")), twin);

    // Clearing one end resolves the pair.
    QVERIFY(builder.clearSlot(slotOf("A14")));
    QCOMPARE(builder.duplicateCount(), 2);
}

// ---------------------------------------------------------------------------
// Exporting the built bank
// ---------------------------------------------------------------------------

void TestBankBuilder::exportsEachPatchToTheDestinationItOccupies()
{
    ImportOptions options;
    options.limit = 3;
    const auto digest = importFixture(options);
    QVERIFY(!digest.isEmpty());

    BankBuilderViewModel builder;
    builder.setDatabase(&m_db);
    const auto records = m_db.search({});
    QCOMPARE(static_cast<int>(records.size()), 3);

    // A deliberately gappy arrangement: nothing consecutive about it.
    QVERIFY(builder.placePatch(slotOf("A15"), records[0].id));
    QVERIFY(builder.placePatch(slotOf("A21"), records[1].id));
    QVERIFY(builder.placePatch(slotOf("B88"), records[2].id));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto path = dir.filePath(QStringLiteral("live-band.syx"));

    services::LibraryImportService importService(m_db);
    services::LibraryExportService exportService(m_db);
    LibraryTransferViewModel transfer(importService, exportService);

    QVERIFY2(transfer.exportBankArrangement(builder.arrangementIds(), QUrl::fromLocalFile(path)),
             qPrintable(transfer.resultDetail()));
    QCOMPARE(transfer.resultTone(), QStringLiteral("success"));

    QFile written(path);
    QVERIFY(written.open(QIODevice::ReadOnly));
    const QByteArray bytes = written.readAll();
    const roland::ByteVector data(reinterpret_cast<const roland::Byte*>(bytes.constData()),
                                  reinterpret_cast<const roland::Byte*>(bytes.constData()) + bytes.size());

    const auto reimported = library::importSyxStream(data);
    QCOMPARE(reimported.entries.size(), std::size_t{3});
    // A35 is USER:021 and B88 is USER:128; the file says so, and the empty
    // destinations between them contributed nothing at all.
    QCOMPARE(reimported.entries[0].provenance().userNumber, std::optional<int>{slotOf("A15") + 1});
    QCOMPARE(reimported.entries[1].provenance().userNumber, std::optional<int>{slotOf("A21") + 1});
    QCOMPARE(reimported.entries[2].provenance().userNumber, std::optional<int>{128});

    for (std::size_t i = 0; i < reimported.entries.size(); ++i) {
        const auto original = m_db.loadEntry(records[i].id);
        QVERIFY(original.has_value());
        QVERIFY(reimported.entries[i].hasSameParameters(*original));
    }
}

void TestBankBuilder::refusesToExportAnEmptyBank()
{
    BankBuilderViewModel builder;
    builder.setDatabase(&m_db);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto path = dir.filePath(QStringLiteral("empty.syx"));

    services::LibraryImportService importService(m_db);
    services::LibraryExportService exportService(m_db);
    LibraryTransferViewModel transfer(importService, exportService);

    QVERIFY(!transfer.exportBankArrangement(builder.arrangementIds(), QUrl::fromLocalFile(path)));
    QCOMPARE(transfer.resultTone(), QStringLiteral("error"));
    // Nothing written: an empty file that looks importable is worse than none.
    QVERIFY(!QFile::exists(path));
}

QTEST_MAIN(TestBankBuilder)
#include "tst_bank_builder.moc"
