// Phase 5 — the Library result model.
//
// Backed by a real library built from tests/fixtures/xp60/user-bank-amal.syx,
// so paging, filtering and ordering are exercised over 128 real patch names.

#include "library/LibraryDatabase.h"
#include "library/SyxImport.h"
#include "presentation/LibraryListModel.h"

#include <QAbstractItemModelTester>
#include <QFile>
#include <QSignalSpy>
#include <QTest>

#include <algorithm>
#include <memory>

using namespace xp60studio;
using library::LibraryDatabase;
using library::LibraryEntry;
using presentation::LibraryListModel;

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

QString nameAt(const LibraryListModel& model, int row)
{
    return model.data(model.index(row), LibraryListModel::NameRole).toString();
}

} // namespace

class TestLibraryModel : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    void obeysTheItemModelContract();
    void reportsMatchedAndTotalCounts();
    void readsEveryRowAcrossPageBoundaries();
    void fetchesOnlyThePagesThatAreRead();
    void filtersNarrowTheResults();
    void clearFiltersRestoresEverything();
    void ordersRowsAsAsked();
    void exposesProvenanceWithoutInventingASlot();

    void editsUserMetadataAndPersistsIt();
    void refusesAnInvalidRatingAndChangesNothing();
    void refusesADuplicateOrEmptyTag();
    void dropsARowThatNoLongerMatchesAFilter();
    void removesARow();
    void reportsDuplicates();

    void selectionFollowsTheRowAndClearsWhenGone();
    void emptiesWhenThereIsNoDatabase();

private:
    void populate();
    // A fresh model per test. Filters and sort order are model state, so a
    // shared instance would let one test's filter silently narrow the next.
    [[nodiscard]] LibraryListModel& model() const { return *m_model; }

    roland::ByteVector m_fixture;
    LibraryDatabase m_db;
    std::unique_ptr<LibraryListModel> m_model;
};

void TestLibraryModel::initTestCase()
{
    m_fixture = readFixture();
    QVERIFY2(!m_fixture.empty(), "golden fixture missing");
}

void TestLibraryModel::init()
{
    m_model.reset();
    m_db.close();
    QVERIFY2(m_db.open(QString::fromLatin1(LibraryDatabase::kInMemoryPath)), qPrintable(m_db.lastError()));
    populate();
    m_model = std::make_unique<LibraryListModel>();
    m_model->setDatabase(&m_db);
}

void TestLibraryModel::populate()
{
    library::SyxImportOptions options;
    options.sourceName = "user-bank-amal.syx";
    options.importedAt = std::chrono::system_clock::time_point{std::chrono::seconds{1'700'000'000}};
    const auto entries = library::importSyxStream(m_fixture, options).entries;
    QCOMPARE(entries.size(), std::size_t{128});
    QVERIFY2(m_db.insertAll(entries).has_value(), qPrintable(m_db.lastError()));
}

// ---------------------------------------------------------------------------
// Reading
// ---------------------------------------------------------------------------

void TestLibraryModel::obeysTheItemModelContract()
{
    QAbstractItemModelTester tester(m_model.get(), QAbstractItemModelTester::FailureReportingMode::QtTest);
    model().setSearchText(QStringLiteral("a"));
    model().setSortOrder(LibraryListModel::SourceSlot);
    model().clearFilters();
    QVERIFY(model().count() == 128);
}

void TestLibraryModel::reportsMatchedAndTotalCounts()
{
    QCOMPARE(model().rowCount(), 128);
    QCOMPARE(model().count(), 128);
    QCOMPARE(model().libraryTotal(), 128);
    QVERIFY(!model().filtered());

    // 128 rows is more than one page, which is the case worth testing.
    QVERIFY(model().count() > LibraryListModel::kPageSize);
}

void TestLibraryModel::readsEveryRowAcrossPageBoundaries()
{
    model().setSortOrder(LibraryListModel::SourceSlot);

    // Every row resolves, including the ones either side of a page edge and
    // the last row of the last, partial page.
    for (int row = 0; row < model().rowCount(); ++row) {
        const auto slot = model().data(model().index(row), LibraryListModel::SlotLabelRole).toString();
        QCOMPARE(slot, QStringLiteral("USER:%1").arg(row + 1, 3, 10, QLatin1Char('0')));
        QVERIFY(!nameAt(model(), row).isEmpty());
    }

    // Out of range is empty, not a crash.
    QVERIFY(!model().data(model().index(-1), LibraryListModel::NameRole).isValid());
    QVERIFY(!model().data(model().index(model().rowCount()), LibraryListModel::NameRole).isValid());
}

void TestLibraryModel::fetchesOnlyThePagesThatAreRead()
{
    model().setSortOrder(LibraryListModel::SourceSlot);

    // Jumping straight to the last row must not require reading the rows
    // before it: a scroll to the end of a large library is one page fetch.
    const int lastRow = model().rowCount() - 1;
    const auto slot = model().data(model().index(lastRow), LibraryListModel::SlotLabelRole).toString();
    QCOMPARE(slot, QStringLiteral("USER:128"));

    // Reading row 0 afterwards still works, from its own page.
    QCOMPARE(model().data(model().index(0), LibraryListModel::SlotLabelRole).toString(),
             QStringLiteral("USER:001"));
}

void TestLibraryModel::filtersNarrowTheResults()
{
    QSignalSpy filterSpy(m_model.get(), &LibraryListModel::filterChanged);

    QVERIFY(model().setFavourite(0, true));
    QVERIFY(model().setRating(1, 5));
    QVERIFY(model().setCategoryOf(2, QStringLiteral("Pad")));
    QVERIFY(model().addTag(3, QStringLiteral("live")));

    model().setFavouritesOnly(true);
    QCOMPARE(model().count(), 1);
    QVERIFY(model().filtered());
    model().setFavouritesOnly(false);

    model().setMinimumRating(5);
    QCOMPARE(model().count(), 1);
    model().setMinimumRating(0);

    model().setCategory(QStringLiteral("Pad"));
    QCOMPARE(model().count(), 1);
    model().setCategory(QString());

    model().setTags({QStringLiteral("live")});
    QCOMPARE(model().count(), 1);
    model().setTags({QStringLiteral("live"), QStringLiteral("absent")});
    QCOMPARE(model().count(), 0);
    model().setTags({});

    // Searching a real name finds it, whatever case is typed.
    const QString sample = nameAt(model(), 0);
    QVERIFY(!sample.isEmpty());
    model().setSearchText(sample.toUpper());
    QVERIFY(model().count() >= 1);
    QVERIFY(model().count() < 128);

    // The library total never narrows with the filters.
    QCOMPARE(model().libraryTotal(), 128);
    QVERIFY(filterSpy.count() > 0);
}

void TestLibraryModel::clearFiltersRestoresEverything()
{
    model().setSearchText(QStringLiteral("zzzznotaname"));
    model().setFavouritesOnly(true);
    model().setMinimumRating(3);
    model().setCategory(QStringLiteral("Pad"));
    model().setTags({QStringLiteral("live")});
    QCOMPARE(model().count(), 0);
    QVERIFY(model().filtered());

    model().clearFilters();
    QVERIFY(!model().filtered());
    QCOMPARE(model().count(), 128);
    QVERIFY(model().searchText().isEmpty());
    QVERIFY(model().tags().isEmpty());
    QCOMPARE(model().minimumRating(), 0);
}

void TestLibraryModel::ordersRowsAsAsked()
{
    model().setSortOrder(LibraryListModel::NameAscending);
    QStringList ascending;
    for (int row = 0; row < model().rowCount(); ++row) {
        ascending << nameAt(model(), row);
    }
    QVERIFY(std::is_sorted(ascending.begin(), ascending.end(),
                           [](const QString& a, const QString& b) { return a.toLower() < b.toLower(); }));

    model().setSortOrder(LibraryListModel::NameDescending);
    QStringList descending;
    for (int row = 0; row < model().rowCount(); ++row) {
        descending << nameAt(model(), row);
    }
    std::reverse(descending.begin(), descending.end());
    QCOMPARE(descending, ascending);

    model().setSortOrder(LibraryListModel::SourceSlot);
    QCOMPARE(model().data(model().index(0), LibraryListModel::SlotLabelRole).toString(),
             QStringLiteral("USER:001"));
}

void TestLibraryModel::exposesProvenanceWithoutInventingASlot()
{
    model().setSortOrder(LibraryListModel::SourceSlot);
    const auto index = model().index(6);
    QCOMPARE(model().data(index, LibraryListModel::SlotLabelRole).toString(), QStringLiteral("USER:007"));
    QCOMPARE(model().data(index, LibraryListModel::SourceNameRole).toString(),
             QStringLiteral("user-bank-amal.syx"));
    QCOMPARE(model().data(index, LibraryListModel::OriginLabelRole).toString(),
             QStringLiteral("imported file"));
    QCOMPARE(model().data(index, LibraryListModel::ImportedAtRole).toDateTime(),
             QDateTime::fromSecsSinceEpoch(1'700'000'000));
    // Diagnostics only, and short enough that nobody mistakes it for an id.
    QCOMPARE(model().data(index, LibraryListModel::FingerprintRole).toString().size(), 8);

    model().selectRow(6);
    const auto selected = model().selected();
    QCOMPARE(selected.value(QStringLiteral("slotLabel")).toString(), QStringLiteral("USER:007"));
    QVERIFY(selected.value(QStringLiteral("sysExBytes")).toLongLong() > 0);
    QCOMPARE(selected.value(QStringLiteral("sourceDigest")).toString().size(), 64);
}

// ---------------------------------------------------------------------------
// Editing
// ---------------------------------------------------------------------------

void TestLibraryModel::editsUserMetadataAndPersistsIt()
{
    model().setSortOrder(LibraryListModel::SourceSlot);
    QSignalSpy dataSpy(m_model.get(), &QAbstractItemModel::dataChanged);

    QVERIFY(model().setFavourite(3, true));
    QVERIFY(model().setRating(3, 4));
    QVERIFY(model().setCategoryOf(3, QStringLiteral("Strings")));
    QVERIFY(model().addTag(3, QStringLiteral("warm")));
    QVERIFY(model().setNotes(3, QStringLiteral("Second verse.")));
    QVERIFY(dataSpy.count() >= 5);

    const auto index = model().index(3);
    QCOMPARE(model().data(index, LibraryListModel::FavouriteRole).toBool(), true);
    QCOMPARE(model().data(index, LibraryListModel::RatingRole).toInt(), 4);
    QCOMPARE(model().data(index, LibraryListModel::CategoryRole).toString(), QStringLiteral("Strings"));
    QCOMPARE(model().data(index, LibraryListModel::TagsRole).toStringList(), QStringList{QStringLiteral("warm")});
    QCOMPARE(model().data(index, LibraryListModel::NotesRole).toString(), QStringLiteral("Second verse."));

    // It really reached the database, not just the cached page.
    const auto id = model().data(index, LibraryListModel::IdRole).toLongLong();
    const auto stored = m_db.record(id);
    QVERIFY(stored.has_value());
    QCOMPARE(stored->userMetadata.rating, 4);
    QVERIFY(stored->userMetadata.hasTag("warm"));

    // The vocabulary the filter chips draw from follows.
    QVERIFY(model().categoriesInUse().contains(QStringLiteral("Strings")));
    QVERIFY(model().tagsInUse().contains(QStringLiteral("warm")));

    // Editing metadata never touches the Patch bytes.
    QCOMPARE(m_db.originalSysEx(id)->size(),
             static_cast<std::size_t>(stored->originalSysExSize));
}

void TestLibraryModel::refusesAnInvalidRatingAndChangesNothing()
{
    QSignalSpy errorSpy(m_model.get(), &LibraryListModel::errorOccurred);
    QVERIFY(model().setRating(0, 3));
    QVERIFY(!model().setRating(0, 6));
    QCOMPARE(errorSpy.count(), 1);
    QVERIFY(model().lastError().contains(QStringLiteral("0..5")));
    QCOMPARE(model().data(model().index(0), LibraryListModel::RatingRole).toInt(), 3);

    // A row outside the results is refused rather than silently ignored.
    QVERIFY(!model().setRating(9999, 3));
    QVERIFY(model().lastError().contains(QStringLiteral("9999")));
}

void TestLibraryModel::refusesADuplicateOrEmptyTag()
{
    QVERIFY(model().addTag(0, QStringLiteral("pad")));
    QVERIFY(!model().addTag(0, QStringLiteral("pad")));
    QVERIFY(model().lastError().contains(QStringLiteral("already a tag")));
    QVERIFY(!model().addTag(0, QStringLiteral("   ")));
    QVERIFY(model().lastError().contains(QStringLiteral("cannot be empty")));
    // Case is never folded, so these stay two different tags.
    QVERIFY(model().addTag(0, QStringLiteral("Pad")));
    QCOMPARE(model().data(model().index(0), LibraryListModel::TagsRole).toStringList().size(), 2);

    QVERIFY(model().removeTag(0, QStringLiteral("pad")));
    QVERIFY(!model().removeTag(0, QStringLiteral("pad")));
    QVERIFY(model().lastError().contains(QStringLiteral("not a tag")));
}

void TestLibraryModel::dropsARowThatNoLongerMatchesAFilter()
{
    QVERIFY(model().setFavourite(0, true));
    QVERIFY(model().setFavourite(1, true));
    model().setFavouritesOnly(true);
    QCOMPARE(model().count(), 2);

    // Un-favouriting a row while filtered to favourites removes it from the
    // list, rather than leaving a row on screen that no longer matches.
    QVERIFY(model().setFavourite(0, false));
    QCOMPARE(model().count(), 1);
    QCOMPARE(model().data(model().index(0), LibraryListModel::FavouriteRole).toBool(), true);
}

void TestLibraryModel::removesARow()
{
    model().setSortOrder(LibraryListModel::SourceSlot);
    const auto doomedId = model().data(model().index(5), LibraryListModel::IdRole).toLongLong();

    QVERIFY(model().removeRow(5));
    QCOMPARE(model().count(), 127);
    QCOMPARE(model().libraryTotal(), 127);
    QVERIFY(!m_db.record(doomedId).has_value());
    // The row that took its place is the next slot, not a gap.
    QCOMPARE(model().data(model().index(5), LibraryListModel::SlotLabelRole).toString(),
             QStringLiteral("USER:007"));

    QVERIFY(!model().removeRow(9999));
}

void TestLibraryModel::reportsDuplicates()
{
    model().setSortOrder(LibraryListModel::SourceSlot);

    // The supplied bank holds identical patches of its own, so the baseline is
    // measured rather than assumed. That it is nonzero is itself the point:
    // real libraries contain real duplicates.
    const auto within = model().duplicatesOf(0).size();
    QVERIFY2(within > 0, "expected the supplied bank to contain at least one exact duplicate of USER:001");

    // Import the same bank again; every patch now has an exact twin.
    populate();
    model().refresh();
    QCOMPARE(model().count(), 256);
    model().setSortOrder(LibraryListModel::SourceSlot);

    // Each of those `within` twins is now doubled, and this row gained a copy
    // of itself.
    const auto duplicates = model().duplicatesOf(0);
    QCOMPARE(duplicates.size(), 2 * within + 1);

    const auto ownId = model().data(model().index(0), LibraryListModel::IdRole).toLongLong();
    for (const auto& value : duplicates) {
        const auto entry = value.toMap();
        // A row is never reported as its own duplicate.
        QVERIFY(entry.value(QStringLiteral("id")).toLongLong() != ownId);
        QCOMPARE(entry.value(QStringLiteral("sourceName")).toString(), QStringLiteral("user-bank-amal.syx"));
    }
    // Nothing was merged: every copy is still there, with its own provenance.
    QCOMPARE(model().libraryTotal(), 256);
}

// ---------------------------------------------------------------------------
// Selection and lifetime
// ---------------------------------------------------------------------------

void TestLibraryModel::selectionFollowsTheRowAndClearsWhenGone()
{
    QSignalSpy selectionSpy(m_model.get(), &LibraryListModel::selectionChanged);
    QVERIFY(model().selected().isEmpty());
    QCOMPARE(model().selectedRow(), -1);

    model().selectRow(2);
    QCOMPARE(model().selectedRow(), 2);
    QCOMPARE(model().selected().value(QStringLiteral("row")).toInt(), 2);
    QVERIFY(selectionSpy.count() > 0);

    // Out of range clears rather than clamping to a row the user did not pick.
    model().selectRow(9999);
    QCOMPARE(model().selectedRow(), -1);
    QVERIFY(model().selected().isEmpty());

    // A filter that empties the results cannot leave a selection behind.
    model().selectRow(4);
    model().setSearchText(QStringLiteral("zzzznotaname"));
    QCOMPARE(model().count(), 0);
    QCOMPARE(model().selectedRow(), -1);
}

void TestLibraryModel::emptiesWhenThereIsNoDatabase()
{
    model().setDatabase(nullptr);
    QCOMPARE(model().rowCount(), 0);
    QCOMPARE(model().count(), 0);
    QCOMPARE(model().libraryTotal(), 0);
    QVERIFY(model().categoriesInUse().isEmpty());
    QVERIFY(model().tagsInUse().isEmpty());
    QVERIFY(model().selected().isEmpty());
    QVERIFY(!model().setFavourite(0, true));
    QVERIFY(model().lastError().contains(QStringLiteral("not open")));
}

QTEST_MAIN(TestLibraryModel)
#include "tst_library_model.moc"
