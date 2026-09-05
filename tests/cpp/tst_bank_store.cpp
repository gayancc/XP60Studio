// Saved User banks in the library database.
//
// A bank is an arrangement of *references*: saving one must never copy, move
// or rewrite a Patch, and deleting a Patch must never delete the destination
// it occupied. The destination keeps the name it was given and reports itself
// as missing instead, which is what the Bank Builder shows the user.
//
// Populated from tests/fixtures/xp60/user-bank-amal.syx so the references are
// to real library rows rather than invented ids.

#include "library/BankDraft.h"
#include "library/LibraryDatabase.h"
#include "library/SyxImport.h"
#include "xpmodel/Xp60BankLocation.h"

#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

using namespace xp60studio;
using library::BankDraft;
using library::BankSlotContent;
using library::LibraryDatabase;
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

} // namespace

class TestBankStore : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    void savesAndReloadsAnArrangement();
    void savingLeavesEveryPatchWhereItWas();
    void updatingAnExistingBankReplacesItsArrangement();
    void refusesMoreDestinationsThanABankHas();
    void aDeletedPatchLeavesAMissingDestination();
    void deletingABankLeavesTheLibraryAlone();
    void listsBanksWithTheirOccupancy();
    void groupsTheLibraryBySource();
    void migratesAVersionOneLibraryForward();

private:
    [[nodiscard]] std::vector<std::int64_t> importFixture();

    roland::ByteVector m_fixture;
    QString m_sourceDigest;
    LibraryDatabase m_db;
};

void TestBankStore::initTestCase()
{
    m_fixture = readFixture();
    QVERIFY2(!m_fixture.empty(), "golden fixture missing");
    QVERIFY2(QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE")),
             "this Qt build has no QSQLITE driver");
}

void TestBankStore::init()
{
    m_db.close();
    QVERIFY2(m_db.open(QString::fromLatin1(LibraryDatabase::kInMemoryPath)), qPrintable(m_db.lastError()));
}

std::vector<std::int64_t> TestBankStore::importFixture()
{
    library::SyxImportOptions options;
    options.sourceName = "user-bank-amal.syx";
    options.importedAt = std::chrono::system_clock::time_point{std::chrono::seconds{1'700'000'000}};
    const auto result = library::importSyxStream(m_fixture, options);
    // The digest is computed from the bytes by the import, not supplied: it is
    // how one file is recognised again after being renamed.
    m_sourceDigest = QString::fromStdString(result.sourceDigest);
    const auto ids = m_db.insertAll(result.entries);
    return ids ? *ids : std::vector<std::int64_t>{};
}

void TestBankStore::savesAndReloadsAnArrangement()
{
    const auto ids = importFixture();
    QVERIFY(ids.size() >= 3);

    std::vector<BankSlotContent> arrangement(BankDraft::kSlotCount);
    BankSlotContent first;
    first.patchId = ids[0];
    first.patchName = "GrandPiano";
    first.sourceName = "user-bank-amal.syx";
    first.sourceSlotLabel = "USER:001";
    arrangement[static_cast<std::size_t>(slotOf("A11"))] = first;

    BankSlotContent last;
    last.patchId = ids[2];
    last.patchName = "WarmStrings";
    last.sourceName = "user-bank-amal.syx";
    arrangement[static_cast<std::size_t>(slotOf("B88"))] = last;

    const auto bankId = m_db.saveBank("Live Band Bank", arrangement);
    QVERIFY2(bankId.has_value(), qPrintable(m_db.lastError()));

    const auto loaded = m_db.loadBank(*bankId);
    QVERIFY2(loaded.has_value(), qPrintable(m_db.lastError()));
    QCOMPARE(QString::fromStdString(loaded->record.name), QStringLiteral("Live Band Bank"));
    QCOMPARE(static_cast<int>(loaded->destinations.size()), BankDraft::kSlotCount);
    QCOMPARE(loaded->record.occupiedCount, 2);
    QCOMPARE(loaded->record.missingCount, 0);

    // The two placed Patches came back at exactly the panel destinations they
    // were put at, and nothing else did.
    QCOMPARE(loaded->destinations[static_cast<std::size_t>(slotOf("A11"))].patchId, ids[0]);
    QCOMPARE(QString::fromStdString(loaded->destinations[static_cast<std::size_t>(slotOf("A11"))].patchName),
             QStringLiteral("GrandPiano"));
    QCOMPARE(QString::fromStdString(loaded->destinations[static_cast<std::size_t>(slotOf("A11"))].sourceSlotLabel),
             QStringLiteral("USER:001"));
    QCOMPARE(loaded->destinations[static_cast<std::size_t>(slotOf("B88"))].patchId, ids[2]);
    QVERIFY(loaded->destinations[static_cast<std::size_t>(slotOf("A12"))].empty());

    // And the linear identity of those destinations is still 001 and 128.
    QCOMPARE(slotOf("A11") + 1, 1);
    QCOMPARE(slotOf("B88") + 1, 128);
}

void TestBankStore::savingLeavesEveryPatchWhereItWas()
{
    const auto ids = importFixture();
    const auto before = m_db.totalCount();
    const auto originalBytes = m_db.originalSysEx(ids[0]);
    QVERIFY(originalBytes.has_value());

    std::vector<BankSlotContent> arrangement(BankDraft::kSlotCount);
    // The same Patch in four destinations: still one Patch in the library.
    for (int i = 0; i < 4; ++i) {
        BankSlotContent content;
        content.patchId = ids[0];
        content.patchName = "GrandPiano";
        arrangement[static_cast<std::size_t>(i)] = content;
    }
    QVERIFY(m_db.saveBank("Four Pianos", arrangement).has_value());

    QCOMPARE(m_db.totalCount(), before);
    QCOMPARE(m_db.originalSysEx(ids[0]), originalBytes);
}

void TestBankStore::updatingAnExistingBankReplacesItsArrangement()
{
    const auto ids = importFixture();

    std::vector<BankSlotContent> arrangement(BankDraft::kSlotCount);
    BankSlotContent content;
    content.patchId = ids[0];
    content.patchName = "First";
    arrangement[0] = content;
    const auto bankId = m_db.saveBank("Working Bank", arrangement);
    QVERIFY(bankId.has_value());

    // A different arrangement, saved over the same bank.
    std::vector<BankSlotContent> revised(BankDraft::kSlotCount);
    BankSlotContent moved;
    moved.patchId = ids[1];
    moved.patchName = "Second";
    revised[static_cast<std::size_t>(slotOf("B35"))] = moved;
    const auto same = m_db.saveBank("Working Bank v2", revised, bankId);
    QVERIFY2(same.has_value(), qPrintable(m_db.lastError()));
    QCOMPARE(*same, *bankId);

    const auto loaded = m_db.loadBank(*bankId);
    QVERIFY(loaded.has_value());
    QCOMPARE(QString::fromStdString(loaded->record.name), QStringLiteral("Working Bank v2"));
    QCOMPARE(loaded->record.occupiedCount, 1);
    QVERIFY(loaded->destinations[0].empty());
    QCOMPARE(loaded->destinations[static_cast<std::size_t>(slotOf("B35"))].patchId, ids[1]);

    // Still one saved bank, not two.
    QCOMPARE(static_cast<int>(m_db.banks().size()), 1);

    // Saving over a bank that does not exist is refused rather than creating one.
    QVERIFY(!m_db.saveBank("Ghost", revised, std::optional<std::int64_t>{9999}).has_value());
    QCOMPARE(static_cast<int>(m_db.banks().size()), 1);
}

void TestBankStore::refusesMoreDestinationsThanABankHas()
{
    QVERIFY(!m_db.saveBank("Too big", std::vector<BankSlotContent>(BankDraft::kSlotCount + 1)).has_value());
    QVERIFY(m_db.lastError().contains(QStringLiteral("128")));
    QVERIFY(m_db.banks().empty());

    // An unnamed bank is refused too: a saved bank the user cannot identify
    // later is not a saved bank.
    QVERIFY(!m_db.saveBank("", std::vector<BankSlotContent>(BankDraft::kSlotCount)).has_value());
}

void TestBankStore::aDeletedPatchLeavesAMissingDestination()
{
    const auto ids = importFixture();

    std::vector<BankSlotContent> arrangement(BankDraft::kSlotCount);
    BankSlotContent doomed;
    doomed.patchId = ids[0];
    doomed.patchName = "DoomedPad";
    doomed.sourceName = "user-bank-amal.syx";
    arrangement[static_cast<std::size_t>(slotOf("A35"))] = doomed;

    BankSlotContent survivor;
    survivor.patchId = ids[1];
    survivor.patchName = "Survivor";
    arrangement[static_cast<std::size_t>(slotOf("A36"))] = survivor;

    const auto bankId = m_db.saveBank("Fragile", arrangement);
    QVERIFY(bankId.has_value());

    QVERIFY2(m_db.remove(ids[0]), qPrintable(m_db.lastError()));

    const auto loaded = m_db.loadBank(*bankId);
    QVERIFY(loaded.has_value());
    const auto& hole = loaded->destinations[static_cast<std::size_t>(slotOf("A35"))];
    // The destination is still occupied, still named, and explicitly missing —
    // never quietly presented as free space.
    QVERIFY(!hole.empty());
    QVERIFY(hole.missing);
    QCOMPARE(hole.patchId, static_cast<std::int64_t>(0));
    QCOMPARE(QString::fromStdString(hole.patchName), QStringLiteral("DoomedPad"));
    QCOMPARE(loaded->record.missingCount, 1);
    QCOMPARE(loaded->record.occupiedCount, 2);

    // The other destination is untouched.
    QCOMPARE(loaded->destinations[static_cast<std::size_t>(slotOf("A36"))].patchId, ids[1]);
    QVERIFY(!loaded->destinations[static_cast<std::size_t>(slotOf("A36"))].missing);

    // Re-saving keeps the hole a hole rather than inventing a reference.
    QVERIFY(m_db.saveBank("Fragile", loaded->destinations, bankId).has_value());
    const auto again = m_db.loadBank(*bankId);
    QVERIFY(again.has_value());
    QVERIFY(again->destinations[static_cast<std::size_t>(slotOf("A35"))].missing);
    QCOMPARE(QString::fromStdString(again->destinations[static_cast<std::size_t>(slotOf("A35"))].patchName),
             QStringLiteral("DoomedPad"));
}

void TestBankStore::deletingABankLeavesTheLibraryAlone()
{
    const auto ids = importFixture();
    const auto before = m_db.totalCount();

    std::vector<BankSlotContent> arrangement(BankDraft::kSlotCount);
    BankSlotContent content;
    content.patchId = ids[0];
    content.patchName = "Piano";
    arrangement[0] = content;
    const auto bankId = m_db.saveBank("Temporary", arrangement);
    QVERIFY(bankId.has_value());

    QVERIFY2(m_db.removeBank(*bankId), qPrintable(m_db.lastError()));
    QVERIFY(m_db.banks().empty());
    QVERIFY(!m_db.loadBank(*bankId).has_value());
    // Every Patch is still in the library.
    QCOMPARE(m_db.totalCount(), before);
    QVERIFY(m_db.record(ids[0]).has_value());

    QVERIFY(!m_db.removeBank(*bankId));
}

void TestBankStore::listsBanksWithTheirOccupancy()
{
    const auto ids = importFixture();

    std::vector<BankSlotContent> five(BankDraft::kSlotCount);
    for (int i = 0; i < 5; ++i) {
        BankSlotContent content;
        content.patchId = ids[static_cast<std::size_t>(i)];
        content.patchName = "P" + std::to_string(i);
        five[static_cast<std::size_t>(i)] = content;
    }
    QVERIFY(m_db.saveBank("Five", five).has_value());
    QVERIFY(m_db.saveBank("Empty", std::vector<BankSlotContent>(BankDraft::kSlotCount)).has_value());

    const auto banks = m_db.banks();
    QCOMPARE(static_cast<int>(banks.size()), 2);
    int five_count = -1;
    int empty_count = -1;
    for (const auto& bank : banks) {
        if (bank.name == "Five") {
            five_count = bank.occupiedCount;
        } else if (bank.name == "Empty") {
            empty_count = bank.occupiedCount;
        }
    }
    QCOMPARE(five_count, 5);
    QCOMPARE(empty_count, 0);
}

void TestBankStore::groupsTheLibraryBySource()
{
    QVERIFY(m_db.sourcesInUse().empty());
    const auto ids = importFixture();
    QVERIFY(!ids.empty());

    const auto sources = m_db.sourcesInUse();
    QCOMPARE(static_cast<int>(sources.size()), 1);
    QCOMPARE(QString::fromStdString(sources[0].name), QStringLiteral("user-bank-amal.syx"));
    QCOMPARE(QString::fromStdString(sources[0].digest), m_sourceDigest);
    QVERIFY(!m_sourceDigest.isEmpty());
    QCOMPARE(sources[0].patchCount, static_cast<int>(ids.size()));
}

void TestBankStore::migratesAVersionOneLibraryForward()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("old.xp60lib"));

    {
        // A library written before banks existed: the two new tables are
        // absent and the recorded schema is 1.
        auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("legacy"));
        db.setDatabaseName(path);
        QVERIFY(db.open());
        QSqlQuery sql(db);
        QVERIFY(sql.exec(QStringLiteral("CREATE TABLE schema_info (version INTEGER NOT NULL)")));
        QVERIFY(sql.exec(QStringLiteral("INSERT INTO schema_info (version) VALUES (1)")));
        db.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("legacy"));

    LibraryDatabase migrated;
    QVERIFY2(migrated.open(path), qPrintable(migrated.lastError()));
    QCOMPARE(migrated.schemaVersion(), std::optional<int>{LibraryDatabase::kSchemaVersion});
    // The new tables work, and nothing that was there before was disturbed.
    QVERIFY(migrated.saveBank("After migration", std::vector<BankSlotContent>(BankDraft::kSlotCount)).has_value());
    QCOMPARE(static_cast<int>(migrated.banks().size()), 1);
}

QTEST_MAIN(TestBankStore)
#include "tst_bank_store.moc"
