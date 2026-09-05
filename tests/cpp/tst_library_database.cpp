// Phase 5 — local library persistence (SQLite through Qt SQL).
//
// The library is populated from tests/fixtures/xp60/user-bank-amal.syx, a real
// XP-60 user bank, so the queries run against 128 real patch names and real
// provenance rather than invented rows.

#include "library/LibraryDatabase.h"
#include "library/SyxImport.h"

#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <set>

using namespace xp60studio;
using library::LibraryDatabase;
using library::LibraryEntry;
using library::LibraryQuery;
using library::LibraryRecord;
using library::PatchUserMetadata;

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

std::vector<std::string> namesOf(const std::vector<LibraryRecord>& records)
{
    std::vector<std::string> names;
    names.reserve(records.size());
    for (const auto& record : records) {
        names.push_back(record.name);
    }
    return names;
}

} // namespace

class TestLibraryDatabase : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init(); // a fresh in-memory library per test

    void createsItsSchemaAndReportsTheVersion();
    void refusesALibraryFromANewerBuild();
    void storesAWholeBankInOneTransaction();
    void rollsBackAFailedBatchCompletely();
    void rebuildsThePatchFromThePreservedBytes();
    void keepsTheOriginalBytesByteForByte();
    void survivesAReopenOnDisk();

    void searchesNamesCaseInsensitivelyAcrossTerms();
    void combinesFiltersAsNarrowing();
    void ordersResultsAsAsked();
    void pagesResultsForAVirtualizedModel();
    void countsIndependentlyOfPaging();

    void roundTripsUserMetadata();
    void rejectsAnOutOfRangeRating();
    void replacesTagsOnUpdateWithoutTouchingThePatch();
    void removesAnEntryAndItsTags();

    void reportsDuplicatesWithoutMergingThem();
    void listsCategoriesAndTagsInUse();

    void derivesWhatEachPatchNeedsFromAnExpansionBoard();
    void backfillsTheDerivedExpansionDataForAnOlderLibrary();

private:
    [[nodiscard]] std::vector<LibraryEntry> fixtureEntries() const;

    roland::ByteVector m_fixture;
    LibraryDatabase m_db;
};

void TestLibraryDatabase::initTestCase()
{
    m_fixture = readFixture();
    QVERIFY2(!m_fixture.empty(), "golden fixture missing");
    QVERIFY2(QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE")),
             "this Qt build has no QSQLITE driver");
}

void TestLibraryDatabase::init()
{
    m_db.close();
    QVERIFY2(m_db.open(QString::fromLatin1(LibraryDatabase::kInMemoryPath)), qPrintable(m_db.lastError()));
}

std::vector<LibraryEntry> TestLibraryDatabase::fixtureEntries() const
{
    library::SyxImportOptions options;
    options.sourceName = "user-bank-amal.syx";
    options.importedAt = std::chrono::system_clock::time_point{std::chrono::seconds{1'700'000'000}};
    return library::importSyxStream(m_fixture, options).entries;
}

// ---------------------------------------------------------------------------
// Schema and lifetime
// ---------------------------------------------------------------------------

void TestLibraryDatabase::createsItsSchemaAndReportsTheVersion()
{
    QVERIFY(m_db.isOpen());
    QCOMPARE(m_db.schemaVersion(), std::optional<int>{LibraryDatabase::kSchemaVersion});
    QCOMPARE(m_db.totalCount(), std::optional<int>{0});
    QVERIFY(m_db.lastError().isEmpty());
}

void TestLibraryDatabase::refusesALibraryFromANewerBuild()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("future.xp60lib"));

    {
        LibraryDatabase written;
        QVERIFY2(written.open(path), qPrintable(written.lastError()));
    }
    // Forge a newer schema version, as a library written by a later build.
    {
        auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("forge"));
        db.setDatabaseName(path);
        QVERIFY(db.open());
        QSqlQuery sql(db);
        QVERIFY(sql.exec(QStringLiteral("UPDATE schema_info SET version = %1")
                             .arg(LibraryDatabase::kSchemaVersion + 1)));
        db.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("forge"));

    LibraryDatabase reopened;
    QVERIFY2(!reopened.open(path), "a newer schema must not be opened");
    QVERIFY(reopened.lastError().contains(QStringLiteral("newer version")));
    QVERIFY(!reopened.isOpen());
}

void TestLibraryDatabase::storesAWholeBankInOneTransaction()
{
    const auto entries = fixtureEntries();
    QCOMPARE(entries.size(), std::size_t{128});

    const auto ids = m_db.insertAll(entries);
    QVERIFY2(ids.has_value(), qPrintable(m_db.lastError()));
    QCOMPARE(ids->size(), std::size_t{128});
    QCOMPARE(m_db.totalCount(), std::optional<int>{128});

    // Every id is distinct and resolves to the entry it was created from.
    QCOMPARE(std::set<std::int64_t>(ids->begin(), ids->end()).size(), std::size_t{128});
    const auto first = m_db.record(ids->front());
    QVERIFY(first.has_value());
    QCOMPARE(first->name, entries.front().displayName());
    QCOMPARE(first->fingerprint, entries.front().fingerprint());
    QCOMPARE(first->provenance.userNumber, std::optional<int>{1});
    QCOMPARE(first->provenance.deviceId->displayNumber(), 17);
    QCOMPARE(first->originalSysExSize, static_cast<std::int64_t>(entries.front().originalSysEx().size()));
}

void TestLibraryDatabase::rollsBackAFailedBatchCompletely()
{
    auto entries = fixtureEntries();
    // LibraryEntry has no default constructor by design, so trim rather than resize.
    entries.erase(entries.begin() + 3, entries.end());
    // The third entry carries a rating the model does not allow.
    entries[2].userMetadata().rating = 9;

    QVERIFY(!m_db.insertAll(entries).has_value());
    QVERIFY(m_db.lastError().contains(QStringLiteral("0..5")));
    // Not one of the three landed: a half-imported bank is worse than none.
    QCOMPARE(m_db.totalCount(), std::optional<int>{0});
}

void TestLibraryDatabase::rebuildsThePatchFromThePreservedBytes()
{
    const auto entries = fixtureEntries();
    const auto ids = m_db.insertAll(entries);
    QVERIFY(ids.has_value());

    for (std::size_t i : {std::size_t{0}, std::size_t{63}, std::size_t{127}}) {
        const auto loaded = m_db.loadEntry((*ids)[i]);
        QVERIFY2(loaded.has_value(), qPrintable(m_db.lastError()));
        // The Patch that comes back is the Patch that went in, parameter for
        // parameter — decoded from the stored bytes, not from a stored copy.
        QVERIFY(loaded->hasSameParameters(entries[i]));
        QCOMPARE(loaded->fingerprint(), entries[i].fingerprint());
        QCOMPARE(loaded->name(), entries[i].name());
        QCOMPARE(loaded->provenance().userNumber, entries[i].provenance().userNumber);
        QCOMPARE(loaded->provenance().address, entries[i].provenance().address);
        QCOMPARE(loaded->provenance().importedAt, entries[i].provenance().importedAt);
    }
}

void TestLibraryDatabase::keepsTheOriginalBytesByteForByte()
{
    const auto entries = fixtureEntries();
    const auto ids = m_db.insertAll(entries);
    QVERIFY(ids.has_value());

    for (std::size_t i = 0; i < entries.size(); ++i) {
        const auto stored = m_db.originalSysEx((*ids)[i]);
        QVERIFY(stored.has_value());
        QCOMPARE(*stored, entries[i].originalSysEx());
    }
}

void TestLibraryDatabase::survivesAReopenOnDisk()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("library.xp60lib"));
    const auto entries = fixtureEntries();

    std::int64_t id = 0;
    {
        LibraryDatabase written;
        QVERIFY2(written.open(path), qPrintable(written.lastError()));
        const auto ids = written.insertAll(entries);
        QVERIFY2(ids.has_value(), qPrintable(written.lastError()));
        id = ids->at(5);
        PatchUserMetadata metadata;
        metadata.favourite = true;
        metadata.rating = 5;
        metadata.category = "Pad";
        QVERIFY(metadata.addTag("live"));
        QVERIFY(written.updateUserMetadata(id, metadata));
    }

    LibraryDatabase reopened;
    QVERIFY2(reopened.open(path), qPrintable(reopened.lastError()));
    QCOMPARE(reopened.totalCount(), std::optional<int>{128});
    const auto record = reopened.record(id);
    QVERIFY(record.has_value());
    QVERIFY(record->userMetadata.favourite);
    QCOMPARE(record->userMetadata.rating, 5);
    QCOMPARE(QString::fromStdString(record->userMetadata.category), QStringLiteral("Pad"));
    QCOMPARE(record->userMetadata.tags, std::vector<std::string>{"live"});
    QCOMPARE(record->fingerprint, entries[5].fingerprint());
    QCOMPARE(*reopened.originalSysEx(id), entries[5].originalSysEx());
}

// ---------------------------------------------------------------------------
// Search
// ---------------------------------------------------------------------------

void TestLibraryDatabase::searchesNamesCaseInsensitivelyAcrossTerms()
{
    const auto entries = fixtureEntries();
    QVERIFY(m_db.insertAll(entries).has_value());

    // Take a real name from the bank and search for it in the wrong case.
    const std::string sample = entries.front().displayName();
    QVERIFY(!sample.empty());

    LibraryQuery query;
    query.text = QString::fromStdString(sample).toUpper().toStdString();
    const auto upper = m_db.search(query);
    query.text = QString::fromStdString(sample).toLower().toStdString();
    const auto lower = m_db.search(query);
    QVERIFY(!upper.empty());
    QCOMPARE(namesOf(upper), namesOf(lower));
    QVERIFY(std::any_of(upper.begin(), upper.end(),
                        [&](const LibraryRecord& r) { return r.name == sample; }));

    // Every term must match, so a nonsense term added to a real one finds none.
    query.text = sample + " zzzznotaname";
    QVERIFY(m_db.search(query).empty());

    // An empty search is not a filter.
    query.text.clear();
    QCOMPARE(m_db.search(query).size(), std::size_t{128});
}

void TestLibraryDatabase::combinesFiltersAsNarrowing()
{
    const auto entries = fixtureEntries();
    const auto ids = m_db.insertAll(entries);
    QVERIFY(ids.has_value());

    PatchUserMetadata pad;
    pad.favourite = true;
    pad.rating = 5;
    pad.category = "Pad";
    QVERIFY(pad.addTag("live"));
    QVERIFY(pad.addTag("warm"));
    QVERIFY(m_db.updateUserMetadata(ids->at(0), pad));

    PatchUserMetadata bass;
    bass.favourite = true;
    bass.rating = 3;
    bass.category = "Bass";
    QVERIFY(bass.addTag("live"));
    QVERIFY(m_db.updateUserMetadata(ids->at(1), bass));

    LibraryQuery query;
    query.favourite = true;
    QCOMPARE(m_db.search(query).size(), std::size_t{2});

    query.minimumRating = 4;
    QCOMPARE(m_db.search(query).size(), std::size_t{1});

    query = LibraryQuery{};
    query.tags = {"live"};
    QCOMPARE(m_db.search(query).size(), std::size_t{2});
    // All requested tags must be present, not any of them.
    query.tags = {"live", "warm"};
    QCOMPARE(m_db.search(query).size(), std::size_t{1});
    query.tags = {"live", "warm", "absent"};
    QVERIFY(m_db.search(query).empty());

    query = LibraryQuery{};
    query.category = "Bass";
    QCOMPARE(m_db.search(query).size(), std::size_t{1});

    // Everything from one source file.
    query = LibraryQuery{};
    query.sourceDigest = entries.front().provenance().sourceDigest;
    QCOMPARE(m_db.search(query).size(), std::size_t{128});
    query.sourceDigest = "not-a-digest";
    QVERIFY(m_db.search(query).empty());
}

void TestLibraryDatabase::ordersResultsAsAsked()
{
    const auto entries = fixtureEntries();
    const auto ids = m_db.insertAll(entries);
    QVERIFY(ids.has_value());

    LibraryQuery query;
    query.order = LibraryQuery::Order::NameAscending;
    auto ascending = namesOf(m_db.search(query));
    QCOMPARE(ascending.size(), std::size_t{128});
    QVERIFY(std::is_sorted(ascending.begin(), ascending.end(), [](const auto& a, const auto& b) {
        return QString::fromStdString(a).toLower() < QString::fromStdString(b).toLower();
    }));

    query.order = LibraryQuery::Order::NameDescending;
    auto descending = namesOf(m_db.search(query));
    std::reverse(descending.begin(), descending.end());
    QCOMPARE(descending, ascending);

    // Source slot order reproduces the bank as it sat in the instrument.
    query.order = LibraryQuery::Order::SourceSlotAscending;
    const auto bySlot = m_db.search(query);
    for (std::size_t i = 0; i < bySlot.size(); ++i) {
        QCOMPARE(bySlot[i].provenance.userNumber, std::optional<int>{static_cast<int>(i) + 1});
    }

    QVERIFY(m_db.updateUserMetadata(ids->at(70), PatchUserMetadata{false, 5, "", {}, ""}));
    query.order = LibraryQuery::Order::RatingDescending;
    const auto byRating = m_db.search(query);
    QCOMPARE(byRating.front().id, ids->at(70));
}

void TestLibraryDatabase::pagesResultsForAVirtualizedModel()
{
    QVERIFY(m_db.insertAll(fixtureEntries()).has_value());

    LibraryQuery query;
    query.order = LibraryQuery::Order::SourceSlotAscending;
    const auto all = m_db.search(query);
    QCOMPARE(all.size(), std::size_t{128});

    const auto allNames = namesOf(all);

    query.limit = 25;
    const auto firstPage = m_db.search(query);
    QCOMPARE(firstPage.size(), std::size_t{25});
    QCOMPARE(namesOf(firstPage), std::vector<std::string>(allNames.begin(), allNames.begin() + 25));

    query.offset = 25;
    const auto secondPage = m_db.search(query);
    QCOMPARE(secondPage.size(), std::size_t{25});
    QCOMPARE(secondPage.front().id, all[25].id);

    // An offset without a limit still skips.
    query.limit = 0;
    query.offset = 120;
    QCOMPARE(m_db.search(query).size(), std::size_t{8});

    // Past the end is empty, not an error.
    query.offset = 500;
    QVERIFY(m_db.search(query).empty());
    QVERIFY(m_db.lastError().isEmpty());
}

void TestLibraryDatabase::countsIndependentlyOfPaging()
{
    const auto ids = m_db.insertAll(fixtureEntries());
    QVERIFY(ids.has_value());
    PatchUserMetadata metadata;
    metadata.favourite = true;
    QVERIFY(m_db.updateUserMetadata(ids->at(3), metadata));

    LibraryQuery query;
    query.limit = 10;
    // The row count a virtualized model needs is the match count, not the page.
    QCOMPARE(m_db.count(query), std::optional<int>{128});
    QCOMPARE(m_db.search(query).size(), std::size_t{10});

    query.favourite = true;
    QCOMPARE(m_db.count(query), std::optional<int>{1});
}

// ---------------------------------------------------------------------------
// User metadata
// ---------------------------------------------------------------------------

void TestLibraryDatabase::roundTripsUserMetadata()
{
    const auto entries = fixtureEntries();
    const auto ids = m_db.insertAll(entries);
    QVERIFY(ids.has_value());

    PatchUserMetadata metadata;
    metadata.favourite = true;
    metadata.rating = 4;
    metadata.category = "Strings";
    metadata.notes = "Used on the second verse.";
    QVERIFY(metadata.addTag("warm"));
    QVERIFY(metadata.addTag("Warm")); // a different tag: case is never folded
    QVERIFY(m_db.updateUserMetadata(ids->at(9), metadata));

    const auto record = m_db.record(ids->at(9));
    QVERIFY(record.has_value());
    QCOMPARE(record->userMetadata.favourite, true);
    QCOMPARE(record->userMetadata.rating, 4);
    QCOMPARE(QString::fromStdString(record->userMetadata.category), QStringLiteral("Strings"));
    QCOMPARE(QString::fromStdString(record->userMetadata.notes), QStringLiteral("Used on the second verse."));
    QCOMPARE(record->userMetadata.tags.size(), std::size_t{2});
    QVERIFY(record->userMetadata.hasTag("warm"));
    QVERIFY(record->userMetadata.hasTag("Warm"));

    // Metadata is the user's; the Roland data is untouched by writing it.
    QCOMPARE(record->fingerprint, entries[9].fingerprint());
    QCOMPARE(*m_db.originalSysEx(ids->at(9)), entries[9].originalSysEx());
}

void TestLibraryDatabase::rejectsAnOutOfRangeRating()
{
    const auto ids = m_db.insertAll(fixtureEntries());
    QVERIFY(ids.has_value());

    PatchUserMetadata good;
    good.rating = 5;
    good.category = "Keys";
    QVERIFY(m_db.updateUserMetadata(ids->front(), good));

    PatchUserMetadata bad;
    bad.rating = 6;
    bad.category = "Overwritten";
    QVERIFY(!m_db.updateUserMetadata(ids->front(), bad));
    QVERIFY(m_db.lastError().contains(QStringLiteral("0..5")));

    // The rejected write changed nothing at all.
    const auto record = m_db.record(ids->front());
    QVERIFY(record.has_value());
    QCOMPARE(record->userMetadata.rating, 5);
    QCOMPARE(QString::fromStdString(record->userMetadata.category), QStringLiteral("Keys"));

    // An unknown id is refused rather than silently doing nothing.
    QVERIFY(!m_db.updateUserMetadata(999999, good));
    QVERIFY(m_db.lastError().contains(QStringLiteral("999999")));
}

void TestLibraryDatabase::replacesTagsOnUpdateWithoutTouchingThePatch()
{
    const auto ids = m_db.insertAll(fixtureEntries());
    QVERIFY(ids.has_value());
    const auto id = ids->at(2);

    PatchUserMetadata first;
    QVERIFY(first.addTag("one"));
    QVERIFY(first.addTag("two"));
    QVERIFY(m_db.updateUserMetadata(id, first));
    QCOMPARE(m_db.record(id)->userMetadata.tags.size(), std::size_t{2});

    PatchUserMetadata second;
    QVERIFY(second.addTag("three"));
    QVERIFY(m_db.updateUserMetadata(id, second));
    const auto record = m_db.record(id);
    QCOMPARE(record->userMetadata.tags, std::vector<std::string>{"three"});

    // Clearing every tag is a legal edit, not a no-op.
    QVERIFY(m_db.updateUserMetadata(id, PatchUserMetadata{}));
    QVERIFY(m_db.record(id)->userMetadata.tags.empty());
    QVERIFY(m_db.tagsInUse().empty());
}

void TestLibraryDatabase::removesAnEntryAndItsTags()
{
    const auto ids = m_db.insertAll(fixtureEntries());
    QVERIFY(ids.has_value());
    const auto id = ids->at(4);

    PatchUserMetadata metadata;
    QVERIFY(metadata.addTag("doomed"));
    QVERIFY(m_db.updateUserMetadata(id, metadata));
    QVERIFY(!m_db.tagsInUse().empty());

    QVERIFY(m_db.remove(id));
    QCOMPARE(m_db.totalCount(), std::optional<int>{127});
    QVERIFY(!m_db.record(id).has_value());
    QVERIFY(!m_db.originalSysEx(id).has_value());
    // The tag went with it rather than being orphaned.
    QVERIFY(m_db.tagsInUse().empty());

    QVERIFY(!m_db.remove(id));
    QVERIFY(m_db.lastError().contains(QString::number(id)));
}

// ---------------------------------------------------------------------------
// Duplicates and filter vocabulary
// ---------------------------------------------------------------------------

void TestLibraryDatabase::reportsDuplicatesWithoutMergingThem()
{
    const auto entries = fixtureEntries();
    const auto ids = m_db.insertAll(entries);
    QVERIFY(ids.has_value());

    // Import the same bank a second time, as a user re-importing a file they
    // already have. Nothing is discarded: 256 entries, each with its own
    // provenance, and the duplicates are reported.
    const auto again = m_db.insertAll(entries);
    QVERIFY(again.has_value());
    QCOMPARE(m_db.totalCount(), std::optional<int>{256});

    const auto duplicates = m_db.findDuplicatesOf(ids->front());
    QVERIFY(!duplicates.empty());
    QVERIFY(std::any_of(duplicates.begin(), duplicates.end(),
                        [&](const LibraryRecord& r) { return r.id == again->front(); }));
    for (const auto& duplicate : duplicates) {
        QCOMPARE(duplicate.fingerprint, entries.front().fingerprint());
        QVERIFY(duplicate.id != ids->front()); // never reports itself
    }

    // A fingerprint match is a candidate; the parameters give the verdict.
    const auto left = m_db.loadEntry(ids->front());
    const auto right = m_db.loadEntry(again->front());
    QVERIFY(left.has_value() && right.has_value());
    QVERIFY(left->hasSameParameters(*right));
}

void TestLibraryDatabase::listsCategoriesAndTagsInUse()
{
    const auto ids = m_db.insertAll(fixtureEntries());
    QVERIFY(ids.has_value());
    QVERIFY(m_db.categoriesInUse().empty());
    QVERIFY(m_db.tagsInUse().empty());

    PatchUserMetadata pad;
    pad.category = "Pad";
    QVERIFY(pad.addTag("warm"));
    QVERIFY(m_db.updateUserMetadata(ids->at(0), pad));

    PatchUserMetadata bass;
    bass.category = "Bass";
    QVERIFY(bass.addTag("warm"));
    QVERIFY(bass.addTag("analog"));
    QVERIFY(m_db.updateUserMetadata(ids->at(1), bass));

    QCOMPARE(m_db.categoriesInUse(), (std::vector<std::string>{"Bass", "Pad"}));
    QCOMPARE(m_db.tagsInUse(), (std::vector<std::string>{"analog", "warm"}));
}

// ---------------------------------------------------------------------------
// Phase 7 — derived expansion data
// ---------------------------------------------------------------------------

void TestLibraryDatabase::derivesWhatEachPatchNeedsFromAnExpansionBoard()
{
    const auto ids = m_db.insertAll(fixtureEntries());
    QVERIFY2(ids.has_value(), qPrintable(m_db.lastError()));

    const auto groupsById = m_db.expansionGroupsOf(*ids);
    // Every entry is accounted for: an internal-only Patch is a present, empty
    // entry, never an absent one that would read as "nobody looked".
    QCOMPARE(groupsById.size(), ids->size());

    std::set<int> everyGroup;
    int usingExpansion = 0;
    for (const auto& [id, groups] : groupsById) {
        if (!groups.empty()) {
            ++usingExpansion;
        }
        everyGroup.insert(groups.begin(), groups.end());
    }
    QVERIFY2(usingExpansion > 0, "the fixture bank uses expansion waves");
    QVERIFY(usingExpansion < static_cast<int>(ids->size()));

    // The records agree with the batch lookup, and both say the entry was
    // scanned.
    const auto record = m_db.record(ids->front());
    QVERIFY(record.has_value());
    QVERIFY(record->expansionScanned);
    QCOMPARE(record->expansionGroups, groupsById.at(ids->front()));

    // The filters partition the library exactly.
    LibraryQuery internal;
    internal.expansion = LibraryQuery::Expansion::InternalOnly;
    LibraryQuery expansion;
    expansion.expansion = LibraryQuery::Expansion::UsesExpansion;
    QCOMPARE(m_db.count(expansion).value_or(-1), usingExpansion);
    QCOMPARE(m_db.count(internal).value_or(-1) + usingExpansion, static_cast<int>(ids->size()));

    // "Needs a group outside the profile" and "plays with the profile" are
    // complements at every profile, including the empty one.
    for (const auto& provided : {std::set<int>{}, everyGroup}) {
        LibraryQuery needs;
        needs.expansion = LibraryQuery::Expansion::NeedsGroupOutsideProfile;
        needs.providedGroups = provided;
        LibraryQuery plays = needs;
        plays.expansion = LibraryQuery::Expansion::PlaysWithProfile;
        QCOMPARE(m_db.count(needs).value_or(-1) + m_db.count(plays).value_or(-1), static_cast<int>(ids->size()));
    }

    // Declaring every group in use leaves nothing needing a board.
    LibraryQuery satisfied;
    satisfied.expansion = LibraryQuery::Expansion::NeedsGroupOutsideProfile;
    satisfied.providedGroups = everyGroup;
    QCOMPARE(m_db.count(satisfied).value_or(-1), 0);

    // Deleting an entry takes its derived rows with it.
    QVERIFY(m_db.remove(ids->front()));
    QCOMPARE(m_db.expansionGroupsOf({ids->front()}).size(), std::size_t{0});
}

// A library written before schema 5 has no derived rows. Opening it fills them
// from the bytes already stored — nothing is asked of the user, and nothing
// stored is altered.
void TestLibraryDatabase::backfillsTheDerivedExpansionDataForAnOlderLibrary()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("older.xp60lib"));

    std::vector<std::int64_t> ids;
    {
        LibraryDatabase db;
        QVERIFY2(db.open(path), qPrintable(db.lastError()));
        const auto inserted = db.insertAll(fixtureEntries());
        QVERIFY2(inserted.has_value(), qPrintable(db.lastError()));
        ids = *inserted;
        db.close();
    }

    // Age the file by hand: drop the derived rows and stamp the older version,
    // which is exactly the state a library written by the previous build is in.
    {
        const QString connection = QStringLiteral("tst_backfill");
        {
            auto sql = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
            sql.setDatabaseName(path);
            QVERIFY(sql.open());
            QSqlQuery query(sql);
            QVERIFY(query.exec(QStringLiteral("DELETE FROM patch_expansion_groups")));
            QVERIFY(query.exec(QStringLiteral("DELETE FROM patch_expansion_scan")));
            QVERIFY(query.exec(QStringLiteral("UPDATE schema_info SET version = 4")));
        }
        QSqlDatabase::removeDatabase(connection);
    }

    LibraryDatabase reopened;
    QVERIFY2(reopened.open(path), qPrintable(reopened.lastError()));
    QCOMPARE(reopened.schemaVersion().value_or(-1), LibraryDatabase::kSchemaVersion);

    const auto groupsById = reopened.expansionGroupsOf(ids);
    QCOMPARE(groupsById.size(), ids.size());
    const int usingExpansion = static_cast<int>(std::count_if(
        groupsById.begin(), groupsById.end(), [](const auto& pair) { return !pair.second.empty(); }));
    QVERIFY(usingExpansion > 0);

    LibraryQuery expansion;
    expansion.expansion = LibraryQuery::Expansion::UsesExpansion;
    QCOMPARE(reopened.count(expansion).value_or(-1), usingExpansion);
}

QTEST_MAIN(TestLibraryDatabase)
#include "tst_library_database.moc"
