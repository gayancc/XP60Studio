#pragma once

#include "library/ExpansionProfile.h"
#include "library/LibraryDatabase.h"
#include "services/PatchWorkspace.h"

#include <QAbstractListModel>
#include <QStringList>
#include <QVariantMap>

#include <map>
#include <optional>
#include <vector>

namespace xp60studio::presentation {

// The Library's result list.
//
// The roadmap requires the library to stay responsive with thousands of
// patches, so this model never materialises the whole result set. It asks the
// database how many rows match (`count`), then fetches a page of records only
// when a row is actually asked for — which, for a QML ListView, means only the
// rows on screen plus its cache buffer. Scrolling a library of ten thousand
// patches costs a handful of small indexed queries, and no Patch is decoded at
// all: a row carries the name, the user's own metadata and enough provenance to
// say where the sound came from (ARCHITECTURE.md §11).
//
// The model is read-mostly. It exposes the user's own metadata for editing —
// favourite, rating, category, tags — because that is the library's job. It
// cannot touch Patch parameters, cannot send MIDI, and cannot reach the
// protocol layer; editing a sound is the Patch Editor's job, and the raw bytes
// stay where they were imported.
class LibraryListModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY filterChanged)
    Q_PROPERTY(QString category READ category WRITE setCategory NOTIFY filterChanged)
    // One import source -- one file, or one device read -- by the digest its
    // provenance recorded. This is what "open a source bank" means in the Bank
    // Builder: the library narrowed to the patches that arrived together.
    // Empty means every source.
    Q_PROPERTY(QString sourceDigest READ sourceDigest WRITE setSourceDigest NOTIFY filterChanged)
    Q_PROPERTY(QVariantList sourcesInUse READ sourcesInUse NOTIFY vocabularyChanged)
    Q_PROPERTY(QStringList tags READ tags WRITE setTags NOTIFY filterChanged)
    Q_PROPERTY(bool favouritesOnly READ favouritesOnly WRITE setFavouritesOnly NOTIFY filterChanged)
    Q_PROPERTY(int minimumRating READ minimumRating WRITE setMinimumRating NOTIFY filterChanged)
    Q_PROPERTY(int sortOrder READ sortOrder WRITE setSortOrder NOTIFY filterChanged)
    // One of the ExpansionFilter values.
    Q_PROPERTY(int expansionFilter READ expansionFilter WRITE setExpansionFilter NOTIFY filterChanged)
    // True when the declared profile is too incomplete for "missing" to mean
    // anything, so every unprovided verdict reads as "cannot tell" instead.
    Q_PROPERTY(bool compatibilityUndecided READ compatibilityUndecided NOTIFY filterChanged)
    // What the compatibility filter can honestly promise right now, for the
    // caption beside it.
    Q_PROPERTY(QString compatibilityNote READ compatibilityNote NOTIFY filterChanged)
    // How many rows match the current filters, and how many the library holds
    // in total. Both are shown so "12 of 1,284" reads correctly.
    Q_PROPERTY(int count READ count NOTIFY filterChanged)
    Q_PROPERTY(int libraryTotal READ libraryTotal NOTIFY filterChanged)
    Q_PROPERTY(bool filtered READ filtered NOTIFY filterChanged)
    Q_PROPERTY(QStringList categoriesInUse READ categoriesInUse NOTIFY vocabularyChanged)
    Q_PROPERTY(QStringList tagsInUse READ tagsInUse NOTIFY vocabularyChanged)
    Q_PROPERTY(QVariantMap selected READ selected NOTIFY selectionChanged)
    Q_PROPERTY(int selectedRow READ selectedRow WRITE selectRow NOTIFY selectionChanged)

public:
    // Mirrors library::LibraryQuery::Order so QML never names a C++ enum
    // value it cannot see.
    enum SortOrder {
        NameAscending = 0,
        NameDescending,
        NewestFirst,
        OldestFirst,
        HighestRated,
        SourceSlot,
    };
    Q_ENUM(SortOrder)

    enum Role {
        IdRole = Qt::UserRole + 1,
        NameRole,
        SourceNameRole,     // the file or device it came from
        SlotLabelRole,      // "USER:007", or the address when there is no slot
        OriginLabelRole,    // "imported file", "fetched from device", ...
        ImportedAtRole,     // QDateTime
        FingerprintRole,    // short form, for diagnostics only
        FavouriteRole,
        RatingRole,
        CategoryRole,
        TagsRole,
        NotesRole,
        // True for the row holding the Patch currently open in the Editor. The
        // row then shows the *working* name rather than the stored one, so a
        // rename in the Editor is visible here immediately.
        EditingRole,
        // True when that working Patch differs from what this row holds.
        EditedRole,
        // Compatibility with the declared instrument, as one of "internal",
        // "available", "missing", "unknown" or "unscanned". Read from derived
        // data, so a scrolling list never decodes a Patch to draw a badge.
        CompatibilityRole,
        // A row-sized explanation of that verdict, e.g. "Needs wave group 14,
        // which no declared board provides".
        CompatibilityLabelRole,
        // The expansion wave groups this Patch refers to, ascending. Empty for
        // an internal-only Patch.
        ExpansionGroupsRole,
    };
    Q_ENUM(Role)

    // How the list is narrowed by what the instrument can play.
    //
    // `NeedsBoard` deliberately does not promise the Patch will fail: whether
    // "needs a group nothing declared provides" means *missing* or merely
    // *undecided* turns on whether the profile is complete, and the UI says
    // which by reading `compatibilityUndecided`.
    enum ExpansionFilter {
        AnyExpansion = 0,
        InternalOnly,
        UsesExpansion,
        NeedsBoard,
        // Everything `NeedsBoard` leaves out: internal-only Patches and the
        // expansion ones a declared board covers.
        PlaysHere,
    };
    Q_ENUM(ExpansionFilter)

    explicit LibraryListModel(QObject* parent = nullptr);

    // The database must outlive the model. Passing nullptr empties the model,
    // which is what a closed library looks like.
    void setDatabase(library::LibraryDatabase* database);
    [[nodiscard]] library::LibraryDatabase* database() const noexcept { return m_database; }

    // The shared working Patch. Optional: without it the model is exactly what
    // the database says, which is what the screenshot harness gets. With it,
    // the row for the Patch being edited reports live values, so the Library
    // and the Editor cannot disagree about a name.
    void setWorkspace(services::PatchWorkspace* workspace);

    // Opens the Patch in row `row` in the Editor by adopting it into the shared
    // workspace. False when the row is gone or the Patch cannot be loaded;
    // nothing is changed either way.
    Q_INVOKABLE bool editRow(int row);
    Q_INVOKABLE bool editEntry(qlonglong entryId);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] QString searchText() const { return m_searchText; }
    void setSearchText(const QString& text);
    [[nodiscard]] QString category() const { return m_category; }
    void setCategory(const QString& category);
    [[nodiscard]] QString sourceDigest() const { return m_sourceDigest; }
    void setSourceDigest(const QString& digest);
    [[nodiscard]] QStringList tags() const { return m_tags; }
    void setTags(const QStringList& tags);
    [[nodiscard]] bool favouritesOnly() const { return m_favouritesOnly; }
    void setFavouritesOnly(bool enabled);
    [[nodiscard]] int minimumRating() const { return m_minimumRating; }
    void setMinimumRating(int rating);
    [[nodiscard]] int sortOrder() const { return m_sortOrder; }
    void setSortOrder(int order);
    [[nodiscard]] int expansionFilter() const { return m_expansionFilter; }
    void setExpansionFilter(int filter);
    [[nodiscard]] bool compatibilityUndecided() const;
    [[nodiscard]] QString compatibilityNote() const;

    // The instrument the list is judged against. Optional: without it every
    // row's verdict is "unknown" and the compatibility filter says so rather
    // than pretending an empty profile means an empty instrument.
    void setExpansionProfile(const library::ExpansionProfile* profile);
    // Call when the profile behind that pointer changes; every verdict and the
    // filtered count depend on it.
    Q_INVOKABLE void expansionProfileChanged();

    [[nodiscard]] int count() const { return m_count; }
    [[nodiscard]] int libraryTotal() const { return m_libraryTotal; }
    [[nodiscard]] bool filtered() const;
    [[nodiscard]] QStringList categoriesInUse() const;
    [[nodiscard]] QStringList tagsInUse() const;
    // One entry per import source: {digest, name, patchCount}. Sorted most
    // recently imported first, which is the order a librarian looks in.
    [[nodiscard]] QVariantList sourcesInUse() const;

    [[nodiscard]] QVariantMap selected() const;
    [[nodiscard]] int selectedRow() const { return m_selectedRow; }

    // Clears every filter in one step, so "show everything again" is one
    // action rather than five.
    // One row as a map, for QML: QAbstractItemModel::data is not invokable
    // from QML, and a delegate is not the only thing that needs to read a row.
    // Empty when the row is not in the current results.
    Q_INVOKABLE QVariantMap rowData(int row) const;

    Q_INVOKABLE void clearFilters();
    Q_INVOKABLE void selectRow(int row);
    // Re-reads counts and drops cached pages. Call after an import.
    Q_INVOKABLE void refresh();

    // User metadata edits. Each returns false and leaves everything unchanged
    // when the database refuses; `lastError` says why.
    Q_INVOKABLE bool setFavourite(int row, bool favourite);
    Q_INVOKABLE bool setRating(int row, int rating);
    Q_INVOKABLE bool setCategoryOf(int row, const QString& category);
    Q_INVOKABLE bool addTag(int row, const QString& tag);
    Q_INVOKABLE bool removeTag(int row, const QString& tag);
    Q_INVOKABLE bool setNotes(int row, const QString& notes);
    Q_INVOKABLE bool removeRow(int row);

    // Rows whose parameters hash the same as `row`'s, as {id, name, slotLabel,
    // sourceName} maps. A fingerprint match is a candidate; the library layer
    // confirms against the parameters before anything is called a duplicate.
    Q_INVOKABLE QVariantList duplicatesOf(int row) const;

    // Every id the current filters match, in the current sort order, not just
    // the pages fetched so far. Export works on what the user can see, so this
    // has to be the whole filtered set rather than the visible window.
    Q_INVOKABLE QVariantList filteredIds() const;

    [[nodiscard]] QString lastError() const { return m_lastError; }

    // Rows fetched per page. Small enough that a jump into the middle of a
    // large library costs one query, large enough that a scroll is not a query
    // per row.
    static constexpr int kPageSize = 64;

Q_SIGNALS:
    void filterChanged();
    void selectionChanged();
    void vocabularyChanged();
    void errorOccurred(const QString& message);

private:
    [[nodiscard]] library::LibraryQuery baseQuery() const;
    void rebuild();
    void invalidatePages();
    // The record for `row`, fetching its page if needed. Null when the row is
    // out of range or the query failed.
    [[nodiscard]] const library::LibraryRecord* recordAt(int row) const;
    [[nodiscard]] library::LibraryRecord* mutableRecordAt(int row);
    // The record for `row`, or null with `lastError` explaining whether the
    // library is closed or the row simply is not in the current results.
    [[nodiscard]] const library::LibraryRecord* requireRecord(int row);
    bool applyMetadata(int row, const library::PatchUserMetadata& metadata);
    // The verdict for one row, from derived data and the declared profile.
    // Never decodes a Patch and never consults the instrument.
    [[nodiscard]] QString compatibilityOf(const library::LibraryRecord& record) const;
    [[nodiscard]] QString compatibilityLabelOf(const library::LibraryRecord& record) const;
    void setError(const QString& message);

    library::LibraryDatabase* m_database = nullptr;
    services::PatchWorkspace* m_workspace = nullptr;

    QString m_searchText;
    QString m_category;
    QString m_sourceDigest;
    QStringList m_tags;
    bool m_favouritesOnly = false;
    int m_minimumRating = 0;
    int m_sortOrder = NameAscending;
    int m_expansionFilter = AnyExpansion;
    const library::ExpansionProfile* m_expansionProfile = nullptr;

    int m_count = 0;
    int m_libraryTotal = 0;
    int m_selectedRow = -1;
    mutable QString m_lastError;

    // Page index -> the records it holds. Mutable because fetching a page is a
    // caching detail of a const read, not a change to what the model shows.
    mutable std::map<int, std::vector<library::LibraryRecord>> m_pages;
};

} // namespace xp60studio::presentation
