#include "presentation/LibraryListModel.h"

#include <QDateTime>

#include <algorithm>

namespace xp60studio::presentation {
namespace {

QString toQt(const std::string& text)
{
    return QString::fromStdString(text);
}

QStringList toQt(const std::vector<std::string>& values)
{
    QStringList list;
    list.reserve(static_cast<qsizetype>(values.size()));
    for (const auto& value : values) {
        list << toQt(value);
    }
    return list;
}

std::vector<std::string> fromQt(const QStringList& values)
{
    std::vector<std::string> out;
    out.reserve(static_cast<std::size_t>(values.size()));
    for (const auto& value : values) {
        out.push_back(value.toStdString());
    }
    return out;
}

library::LibraryQuery::Order toOrder(int sortOrder)
{
    using Order = library::LibraryQuery::Order;
    switch (sortOrder) {
    case LibraryListModel::NameDescending:
        return Order::NameDescending;
    case LibraryListModel::NewestFirst:
        return Order::ImportedNewestFirst;
    case LibraryListModel::OldestFirst:
        return Order::ImportedOldestFirst;
    case LibraryListModel::HighestRated:
        return Order::RatingDescending;
    case LibraryListModel::SourceSlot:
        return Order::SourceSlotAscending;
    case LibraryListModel::NameAscending:
    default:
        return Order::NameAscending;
    }
}

QString slotLabel(const library::PatchProvenance& provenance)
{
    if (provenance.userNumber) {
        return QStringLiteral("USER:%1").arg(*provenance.userNumber, 3, 10, QLatin1Char('0'));
    }
    // No User bank slot: say where it actually lived rather than inventing one.
    return toQt(provenance.address.toHexString());
}

QVariantMap recordMap(const library::LibraryRecord& record)
{
    QVariantMap map;
    map.insert(QStringLiteral("id"), QVariant::fromValue<qlonglong>(record.id));
    map.insert(QStringLiteral("name"), toQt(record.name));
    map.insert(QStringLiteral("slotLabel"), slotLabel(record.provenance));
    map.insert(QStringLiteral("sourceName"), toQt(record.provenance.sourceName));
    return map;
}

} // namespace

LibraryListModel::LibraryListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

void LibraryListModel::setDatabase(library::LibraryDatabase* database)
{
    if (m_database == database) {
        return;
    }
    m_database = database;
    rebuild();
}

// ---------------------------------------------------------------------------
// Model
// ---------------------------------------------------------------------------

int LibraryListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_count;
}

QHash<int, QByteArray> LibraryListModel::roleNames() const
{
    return {
        {IdRole, "entryId"},
        {NameRole, "name"},
        {SourceNameRole, "sourceName"},
        {SlotLabelRole, "slotLabel"},
        {OriginLabelRole, "originLabel"},
        {ImportedAtRole, "importedAt"},
        {FingerprintRole, "fingerprint"},
        {FavouriteRole, "favourite"},
        {RatingRole, "rating"},
        {CategoryRole, "category"},
        {TagsRole, "tags"},
        {NotesRole, "notes"},
        {EditingRole, "editing"},
        {EditedRole, "edited"},
    };
}

void LibraryListModel::setWorkspace(services::PatchWorkspace* workspace)
{
    if (m_workspace == workspace) {
        return;
    }
    if (m_workspace) {
        disconnect(m_workspace, nullptr, this, nullptr);
    }
    m_workspace = workspace;
    if (m_workspace) {
        // Only two roles can move, and only for one row, but which row that is
        // changes with the origin. Refreshing the whole visible range is a few
        // hundred cheap reads against an already-fetched page, and it is
        // correct without tracking the previous origin.
        const auto refresh = [this] {
            if (rowCount() > 0) {
                emit dataChanged(index(0, 0), index(rowCount() - 1, 0), {NameRole, EditingRole, EditedRole});
            }
        };
        connect(m_workspace, &services::PatchWorkspace::changed, this, refresh);
        connect(m_workspace, &services::PatchWorkspace::originChanged, this, refresh);
    }
    if (rowCount() > 0) {
        emit dataChanged(index(0, 0), index(rowCount() - 1, 0), {NameRole, EditingRole, EditedRole});
    }
}

bool LibraryListModel::editRow(int row)
{
    const auto* record = recordAt(row);
    return record ? editEntry(record->id) : false;
}

bool LibraryListModel::editEntry(qlonglong entryId)
{
    if (!m_database || !m_workspace || entryId <= 0) {
        return false;
    }
    const auto entry = m_database->loadEntry(entryId);
    if (!entry) {
        return false;
    }
    // Adopting replaces whatever was being worked on, history included. The
    // screen is responsible for confirming that with the user first when the
    // outgoing Patch has unsaved changes; this is the mechanism, not the policy.
    m_workspace->adopt(entry->patch(), services::PatchOrigin::library(entryId));
    return true;
}

QVariant LibraryListModel::data(const QModelIndex& index, int role) const
{
    const auto* record = recordAt(index.row());
    if (!record) {
        return {};
    }
    // The row for the Patch open in the Editor answers from the working copy,
    // not from the database. Without this the Library would keep showing the
    // stored name after a rename, which is the disagreement this whole
    // architecture exists to prevent.
    const bool editing = m_workspace && m_workspace->hasPatch()
        && m_workspace->origin().isLibraryEntry(record->id);

    switch (role) {
    case IdRole:
        return QVariant::fromValue<qlonglong>(record->id);
    case NameRole:
        return editing ? m_workspace->displayName() : toQt(record->name);
    case EditingRole:
        return editing;
    case EditedRole:
        return editing && m_workspace->modified();
    case SourceNameRole:
        return toQt(record->provenance.sourceName);
    case SlotLabelRole:
        return slotLabel(record->provenance);
    case OriginLabelRole:
        return QString::fromUtf8(library::patchOriginName(record->provenance.origin).data(),
                                 static_cast<qsizetype>(library::patchOriginName(record->provenance.origin).size()));
    case ImportedAtRole:
        return QDateTime::fromSecsSinceEpoch(
            std::chrono::duration_cast<std::chrono::seconds>(record->provenance.importedAt.time_since_epoch()).count());
    case FingerprintRole:
        return toQt(record->fingerprint.toShortString());
    case FavouriteRole:
        return record->userMetadata.favourite;
    case RatingRole:
        return record->userMetadata.rating;
    case CategoryRole:
        return toQt(record->userMetadata.category);
    case TagsRole:
        return toQt(record->userMetadata.tags);
    case NotesRole:
        return toQt(record->userMetadata.notes);
    default:
        return {};
    }
}

// ---------------------------------------------------------------------------
// Paging
// ---------------------------------------------------------------------------

library::LibraryQuery LibraryListModel::baseQuery() const
{
    library::LibraryQuery query;
    query.text = m_searchText.toStdString();
    query.category = m_category.toStdString();
    query.sourceDigest = m_sourceDigest.toStdString();
    query.tags = fromQt(m_tags);
    if (m_favouritesOnly) {
        query.favourite = true;
    }
    if (m_minimumRating > 0) {
        query.minimumRating = m_minimumRating;
    }
    query.order = toOrder(m_sortOrder);
    return query;
}

const library::LibraryRecord* LibraryListModel::recordAt(int row) const
{
    if (!m_database || row < 0 || row >= m_count) {
        return nullptr;
    }
    const int page = row / kPageSize;
    auto it = m_pages.find(page);
    if (it == m_pages.end()) {
        auto query = baseQuery();
        query.limit = kPageSize;
        query.offset = page * kPageSize;
        auto records = m_database->search(query);
        if (records.empty() && !m_database->lastError().isEmpty()) {
            m_lastError = m_database->lastError();
            return nullptr;
        }
        it = m_pages.emplace(page, std::move(records)).first;
    }
    const auto indexInPage = static_cast<std::size_t>(row - page * kPageSize);
    if (indexInPage >= it->second.size()) {
        return nullptr;
    }
    return &it->second[indexInPage];
}

library::LibraryRecord* LibraryListModel::mutableRecordAt(int row)
{
    return const_cast<library::LibraryRecord*>(recordAt(row));
}

void LibraryListModel::invalidatePages()
{
    m_pages.clear();
}

void LibraryListModel::rebuild()
{
    beginResetModel();
    invalidatePages();

    if (m_database) {
        const auto matched = m_database->count(baseQuery());
        const auto total = m_database->totalCount();
        m_count = matched.value_or(0);
        m_libraryTotal = total.value_or(0);
        if (!matched || !total) {
            setError(m_database->lastError());
        }
    } else {
        m_count = 0;
        m_libraryTotal = 0;
    }
    // A row that no longer exists must not stay selected.
    if (m_selectedRow >= m_count) {
        m_selectedRow = -1;
    }
    endResetModel();

    Q_EMIT filterChanged();
    Q_EMIT vocabularyChanged();
    Q_EMIT selectionChanged();
}

void LibraryListModel::refresh()
{
    rebuild();
}

// ---------------------------------------------------------------------------
// Filters
// ---------------------------------------------------------------------------

void LibraryListModel::setSearchText(const QString& text)
{
    if (m_searchText == text) {
        return;
    }
    m_searchText = text;
    rebuild();
}

void LibraryListModel::setCategory(const QString& category)
{
    if (m_category == category) {
        return;
    }
    m_category = category;
    rebuild();
}

void LibraryListModel::setSourceDigest(const QString& digest)
{
    if (m_sourceDigest == digest) {
        return;
    }
    m_sourceDigest = digest;
    rebuild();
}

void LibraryListModel::setTags(const QStringList& tags)
{
    if (m_tags == tags) {
        return;
    }
    m_tags = tags;
    rebuild();
}

void LibraryListModel::setFavouritesOnly(bool enabled)
{
    if (m_favouritesOnly == enabled) {
        return;
    }
    m_favouritesOnly = enabled;
    rebuild();
}

void LibraryListModel::setMinimumRating(int rating)
{
    const int clamped = std::clamp(rating, 0, library::PatchUserMetadata::kMaxRating);
    if (m_minimumRating == clamped) {
        return;
    }
    m_minimumRating = clamped;
    rebuild();
}

void LibraryListModel::setSortOrder(int order)
{
    if (m_sortOrder == order) {
        return;
    }
    m_sortOrder = order;
    rebuild();
}

void LibraryListModel::clearFilters()
{
    if (!filtered()) {
        return;
    }
    m_searchText.clear();
    m_category.clear();
    m_sourceDigest.clear();
    m_tags.clear();
    m_favouritesOnly = false;
    m_minimumRating = 0;
    rebuild();
}

bool LibraryListModel::filtered() const
{
    return !m_searchText.isEmpty() || !m_category.isEmpty() || !m_sourceDigest.isEmpty()
        || !m_tags.isEmpty() || m_favouritesOnly
           || m_minimumRating > 0;
}

QStringList LibraryListModel::categoriesInUse() const
{
    return m_database ? toQt(m_database->categoriesInUse()) : QStringList{};
}

QStringList LibraryListModel::tagsInUse() const
{
    return m_database ? toQt(m_database->tagsInUse()) : QStringList{};
}

// ---------------------------------------------------------------------------
// Selection
// ---------------------------------------------------------------------------

void LibraryListModel::selectRow(int row)
{
    const int resolved = (row >= 0 && row < m_count) ? row : -1;
    if (m_selectedRow == resolved) {
        return;
    }
    m_selectedRow = resolved;
    Q_EMIT selectionChanged();
}

QVariantMap LibraryListModel::rowData(int row) const
{
    const auto* record = recordAt(row);
    if (!record) {
        return {};
    }
    QVariantMap map = recordMap(*record);
    map.insert(QStringLiteral("row"), row);
    map.insert(QStringLiteral("favourite"), record->userMetadata.favourite);
    map.insert(QStringLiteral("rating"), record->userMetadata.rating);
    map.insert(QStringLiteral("category"), toQt(record->userMetadata.category));
    map.insert(QStringLiteral("tags"), toQt(record->userMetadata.tags));
    map.insert(QStringLiteral("notes"), toQt(record->userMetadata.notes));
    return map;
}

QVariantMap LibraryListModel::selected() const
{
    const auto* record = recordAt(m_selectedRow);
    if (!record) {
        return {};
    }
    QVariantMap map = recordMap(*record);
    map.insert(QStringLiteral("row"), m_selectedRow);
    map.insert(QStringLiteral("originLabel"),
               QString::fromUtf8(library::patchOriginName(record->provenance.origin).data(),
                                 static_cast<qsizetype>(library::patchOriginName(record->provenance.origin).size())));
    map.insert(QStringLiteral("fingerprint"), toQt(record->fingerprint.toShortString()));
    map.insert(QStringLiteral("favourite"), record->userMetadata.favourite);
    map.insert(QStringLiteral("rating"), record->userMetadata.rating);
    map.insert(QStringLiteral("category"), toQt(record->userMetadata.category));
    map.insert(QStringLiteral("tags"), toQt(record->userMetadata.tags));
    map.insert(QStringLiteral("notes"), toQt(record->userMetadata.notes));
    map.insert(QStringLiteral("sysExBytes"), QVariant::fromValue<qlonglong>(record->originalSysExSize));
    map.insert(QStringLiteral("sourceDigest"), toQt(record->provenance.sourceDigest));
    return map;
}

// ---------------------------------------------------------------------------
// Editing the user's own metadata
// ---------------------------------------------------------------------------

void LibraryListModel::setError(const QString& message)
{
    m_lastError = message;
    if (!message.isEmpty()) {
        Q_EMIT errorOccurred(message);
    }
}

const library::LibraryRecord* LibraryListModel::requireRecord(int row)
{
    if (!m_database) {
        setError(QStringLiteral("The library is not open."));
        return nullptr;
    }
    const auto* record = recordAt(row);
    if (!record) {
        setError(QStringLiteral("Row %1 is not in the current results.").arg(row));
    }
    return record;
}

bool LibraryListModel::applyMetadata(int row, const library::PatchUserMetadata& metadata)
{
    if (!requireRecord(row)) {
        return false;
    }
    auto* record = mutableRecordAt(row);
    if (!m_database->updateUserMetadata(record->id, metadata)) {
        // Nothing local changes when the database refuses: the row on screen
        // keeps saying what is actually stored.
        setError(m_database->lastError());
        return false;
    }

    record->userMetadata = metadata;
    m_lastError.clear();
    const auto changed = index(row);
    Q_EMIT dataChanged(changed, changed,
                       {FavouriteRole, RatingRole, CategoryRole, TagsRole, NotesRole});
    if (row == m_selectedRow) {
        Q_EMIT selectionChanged();
    }
    Q_EMIT vocabularyChanged();

    // An edit can move a row out of the current filter, or change its place in
    // a rating-ordered list. Re-reading the counts keeps the list honest
    // instead of showing a row that no longer matches.
    if (filtered() || m_sortOrder == HighestRated) {
        rebuild();
    }
    return true;
}

bool LibraryListModel::setFavourite(int row, bool favourite)
{
    const auto* record = requireRecord(row);
    if (!record) {
        return false;
    }
    auto metadata = record->userMetadata;
    metadata.favourite = favourite;
    return applyMetadata(row, metadata);
}

bool LibraryListModel::setRating(int row, int rating)
{
    if (!library::PatchUserMetadata::isValidRating(rating)) {
        setError(QStringLiteral("A rating of %1 is outside 0..%2.")
                     .arg(rating)
                     .arg(library::PatchUserMetadata::kMaxRating));
        return false;
    }
    const auto* record = requireRecord(row);
    if (!record) {
        return false;
    }
    auto metadata = record->userMetadata;
    metadata.rating = rating;
    return applyMetadata(row, metadata);
}

bool LibraryListModel::setCategoryOf(int row, const QString& category)
{
    const auto* record = requireRecord(row);
    if (!record) {
        return false;
    }
    auto metadata = record->userMetadata;
    metadata.category = category.toStdString();
    return applyMetadata(row, metadata);
}

bool LibraryListModel::addTag(int row, const QString& tag)
{
    const auto* record = requireRecord(row);
    if (!record) {
        return false;
    }
    auto metadata = record->userMetadata;
    if (!metadata.addTag(tag.trimmed().toStdString())) {
        setError(tag.trimmed().isEmpty() ? QStringLiteral("A tag cannot be empty.")
                                         : QStringLiteral("'%1' is already a tag on this patch.").arg(tag.trimmed()));
        return false;
    }
    return applyMetadata(row, metadata);
}

bool LibraryListModel::removeTag(int row, const QString& tag)
{
    const auto* record = requireRecord(row);
    if (!record) {
        return false;
    }
    auto metadata = record->userMetadata;
    if (!metadata.removeTag(tag.toStdString())) {
        setError(QStringLiteral("'%1' is not a tag on this patch.").arg(tag));
        return false;
    }
    return applyMetadata(row, metadata);
}

bool LibraryListModel::setNotes(int row, const QString& notes)
{
    const auto* record = requireRecord(row);
    if (!record) {
        return false;
    }
    auto metadata = record->userMetadata;
    metadata.notes = notes.toStdString();
    return applyMetadata(row, metadata);
}

bool LibraryListModel::removeRow(int row)
{
    const auto* record = requireRecord(row);
    if (!record) {
        return false;
    }
    if (!m_database->remove(record->id)) {
        setError(m_database->lastError());
        return false;
    }
    m_lastError.clear();
    rebuild();
    return true;
}

QVariantList LibraryListModel::filteredIds() const
{
    QVariantList list;
    if (!m_database) {
        return list;
    }
    // No limit: the caller wants everything the filters match, and a partial
    // answer would silently export less than the user is looking at.
    auto query = baseQuery();
    query.limit = 0; // 0 means no limit
    query.offset = 0;
    for (const auto& record : m_database->search(query)) {
        list.append(QVariant::fromValue<qlonglong>(record.id));
    }
    return list;
}

QVariantList LibraryListModel::duplicatesOf(int row) const
{
    QVariantList list;
    const auto* record = recordAt(row);
    if (!m_database || !record) {
        return list;
    }
    for (const auto& duplicate : m_database->findDuplicatesOf(record->id)) {
        list.append(recordMap(duplicate));
    }
    return list;
}


QVariantList LibraryListModel::sourcesInUse() const
{
    QVariantList list;
    if (!m_database) {
        return list;
    }
    for (const auto& source : m_database->sourcesInUse()) {
        QVariantMap map;
        map.insert(QStringLiteral("digest"), QString::fromStdString(source.digest));
        map.insert(QStringLiteral("name"), QString::fromStdString(source.name));
        map.insert(QStringLiteral("patchCount"), source.patchCount);
        list.append(map);
    }
    return list;
}

} // namespace xp60studio::presentation
