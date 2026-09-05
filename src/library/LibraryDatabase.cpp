#include "library/LibraryDatabase.h"

#include "library/SyxImport.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>

#include <algorithm>
#include <map>

namespace xp60studio::library {
namespace {

// Schema version 1.
//
// `original_sysex` holds the exact bytes the Patch arrived in. The decoded
// Patch is not stored: it is rebuilt from these bytes on demand, so there is
// never a second copy of the same data that could drift from the first.
//
// `name_search` is the lower-cased name, kept beside the real one so a search
// never has to alter the stored bytes to match them.
constexpr const char* kSchemaStatements[] = {
    "CREATE TABLE IF NOT EXISTS schema_info ("
    "  version INTEGER NOT NULL"
    ")",
    "CREATE TABLE IF NOT EXISTS patches ("
    "  id                 INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  fingerprint        TEXT    NOT NULL,"
    "  name               TEXT    NOT NULL,"
    "  name_search        TEXT    NOT NULL,"
    "  original_sysex     BLOB    NOT NULL,"
    "  origin             INTEGER NOT NULL,"
    "  source_name        TEXT    NOT NULL,"
    "  source_digest      TEXT    NOT NULL,"
    "  source_byte_offset INTEGER NOT NULL,"
    "  source_byte_count  INTEGER NOT NULL,"
    "  address            INTEGER NOT NULL,"
    "  user_number        INTEGER,"
    "  device_id          INTEGER,"
    "  model_id           BLOB,"
    "  imported_at        INTEGER NOT NULL,"
    "  favourite          INTEGER NOT NULL DEFAULT 0,"
    "  rating             INTEGER NOT NULL DEFAULT 0,"
    "  category           TEXT    NOT NULL DEFAULT '',"
    "  notes              TEXT    NOT NULL DEFAULT ''"
    ")",
    "CREATE INDEX IF NOT EXISTS idx_patches_fingerprint ON patches(fingerprint)",
    "CREATE INDEX IF NOT EXISTS idx_patches_name ON patches(name_search)",
    "CREATE INDEX IF NOT EXISTS idx_patches_category ON patches(category)",
    "CREATE INDEX IF NOT EXISTS idx_patches_source ON patches(source_digest)",
    "CREATE TABLE IF NOT EXISTS patch_tags ("
    "  patch_id INTEGER NOT NULL REFERENCES patches(id) ON DELETE CASCADE,"
    "  tag      TEXT    NOT NULL,"
    "  PRIMARY KEY (patch_id, tag)"
    ")",
    "CREATE INDEX IF NOT EXISTS idx_patch_tags_tag ON patch_tags(tag)",
};

QString toQt(const std::string& text)
{
    return QString::fromStdString(text);
}

std::string fromQt(const QString& text)
{
    return text.toStdString();
}

QByteArray toBlob(roland::ByteSpan bytes)
{
    return QByteArray(reinterpret_cast<const char*>(bytes.data()), static_cast<qsizetype>(bytes.size()));
}

roland::ByteVector fromBlob(const QByteArray& blob)
{
    const auto* begin = reinterpret_cast<const roland::Byte*>(blob.constData());
    return roland::ByteVector(begin, begin + blob.size());
}

std::vector<std::string> splitTerms(const std::string& text)
{
    std::vector<std::string> terms;
    for (const auto& part : toQt(text).simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        terms.push_back(fromQt(part.toLower()));
    }
    return terms;
}

const char* orderClause(LibraryQuery::Order order)
{
    switch (order) {
    case LibraryQuery::Order::NameAscending:
        return " ORDER BY p.name_search ASC, p.id ASC";
    case LibraryQuery::Order::NameDescending:
        return " ORDER BY p.name_search DESC, p.id DESC";
    case LibraryQuery::Order::ImportedNewestFirst:
        return " ORDER BY p.imported_at DESC, p.id DESC";
    case LibraryQuery::Order::ImportedOldestFirst:
        return " ORDER BY p.imported_at ASC, p.id ASC";
    case LibraryQuery::Order::RatingDescending:
        return " ORDER BY p.rating DESC, p.name_search ASC, p.id ASC";
    case LibraryQuery::Order::SourceSlotAscending:
        // Entries with no User bank slot sort last rather than as slot zero.
        return " ORDER BY p.user_number IS NULL ASC, p.user_number ASC, p.id ASC";
    }
    return " ORDER BY p.name_search ASC, p.id ASC";
}

// WHERE clause and its bound values, built from the query's set filters only.
struct Filter
{
    QString where;
    QVariantList values;
};

Filter buildFilter(const LibraryQuery& query)
{
    Filter filter;
    QStringList clauses;

    for (const auto& term : splitTerms(query.text)) {
        clauses << QStringLiteral("p.name_search LIKE ?");
        filter.values << QStringLiteral("%%1%").arg(toQt(term));
    }
    if (!query.category.empty()) {
        clauses << QStringLiteral("p.category = ?");
        filter.values << toQt(query.category);
    }
    if (!query.sourceDigest.empty()) {
        clauses << QStringLiteral("p.source_digest = ?");
        filter.values << toQt(query.sourceDigest);
    }
    if (query.favourite) {
        clauses << QStringLiteral("p.favourite = ?");
        filter.values << (*query.favourite ? 1 : 0);
    }
    if (query.minimumRating) {
        clauses << QStringLiteral("p.rating >= ?");
        filter.values << *query.minimumRating;
    }
    if (query.fingerprint) {
        clauses << QStringLiteral("p.fingerprint = ?");
        filter.values << toQt(query.fingerprint->toHexString());
    }
    if (!query.tags.empty()) {
        // Every requested tag must be present, not just one of them.
        QStringList placeholders;
        for (const auto& tag : query.tags) {
            placeholders << QStringLiteral("?");
            filter.values << toQt(tag);
        }
        clauses << QStringLiteral("(SELECT COUNT(DISTINCT t.tag) FROM patch_tags t "
                                  "WHERE t.patch_id = p.id AND t.tag IN (%1)) = %2")
                       .arg(placeholders.join(QStringLiteral(", ")))
                       .arg(query.tags.size());
    }

    if (!clauses.isEmpty()) {
        filter.where = QStringLiteral(" WHERE ") + clauses.join(QStringLiteral(" AND "));
    }
    return filter;
}

void bindAll(QSqlQuery& sql, const QVariantList& values)
{
    for (const auto& value : values) {
        sql.addBindValue(value);
    }
}

} // namespace

// ---------------------------------------------------------------------------

class LibraryDatabase::Private
{
public:
    QString connectionName;
    QSqlDatabase database;

    [[nodiscard]] QSqlQuery query() const { return QSqlQuery(database); }
};

LibraryDatabase::LibraryDatabase()
    : m_d(std::make_unique<Private>())
{
    m_d->connectionName = QStringLiteral("xp60studio-library-")
                          + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

LibraryDatabase::~LibraryDatabase()
{
    close();
}

bool LibraryDatabase::isOpen() const noexcept
{
    return m_d->database.isValid() && m_d->database.isOpen();
}

void LibraryDatabase::close()
{
    if (m_d->database.isValid()) {
        if (m_d->database.isOpen()) {
            m_d->database.close();
        }
        m_d->database = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_d->connectionName);
    }
}

bool LibraryDatabase::open(const QString& path)
{
    close();

    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"))) {
        m_lastError = QStringLiteral("The QSQLITE driver is not available in this Qt build.");
        return false;
    }
    m_d->database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_d->connectionName);
    m_d->database.setDatabaseName(path);
    if (!m_d->database.open()) {
        m_lastError = m_d->database.lastError().text();
        close();
        return false;
    }

    auto sql = m_d->query();
    // Referential integrity is off by default in SQLite; patch_tags depends on
    // it to disappear with its Patch.
    if (!sql.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
        m_lastError = sql.lastError().text();
        close();
        return false;
    }

    for (const auto* statement : kSchemaStatements) {
        if (!sql.exec(QString::fromLatin1(statement))) {
            m_lastError = sql.lastError().text() + QStringLiteral(" [") + QString::fromLatin1(statement) + QLatin1Char(']');
            close();
            return false;
        }
    }

    const auto version = schemaVersion();
    if (!version) {
        if (!sql.exec(QStringLiteral("INSERT INTO schema_info (version) VALUES (%1)").arg(kSchemaVersion))) {
            m_lastError = sql.lastError().text();
            close();
            return false;
        }
    } else if (*version > kSchemaVersion) {
        m_lastError = QStringLiteral("This library was written by a newer version of XP60Studio "
                                     "(schema %1; this build understands %2). Refusing to open it "
                                     "rather than risk losing data.")
                          .arg(*version)
                          .arg(kSchemaVersion);
        close();
        return false;
    }
    // A future *older* version would migrate forward here.

    m_lastError.clear();
    return true;
}

std::optional<int> LibraryDatabase::schemaVersion() const
{
    if (!isOpen()) {
        return std::nullopt;
    }
    auto sql = m_d->query();
    if (!sql.exec(QStringLiteral("SELECT version FROM schema_info LIMIT 1")) || !sql.next()) {
        return std::nullopt;
    }
    return sql.value(0).toInt();
}

// ---------------------------------------------------------------------------
// Writing
// ---------------------------------------------------------------------------

namespace {

bool insertOne(QSqlQuery& sql, const LibraryEntry& entry, std::int64_t& idOut, QString& errorOut)
{
    const auto& provenance = entry.provenance();
    const auto& metadata = entry.userMetadata();
    const QString name = toQt(entry.displayName());

    sql.prepare(QStringLiteral(
        "INSERT INTO patches (fingerprint, name, name_search, original_sysex, origin, source_name, "
        "source_digest, source_byte_offset, source_byte_count, address, user_number, device_id, "
        "model_id, imported_at, favourite, rating, category, notes) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    sql.addBindValue(toQt(entry.fingerprint().toHexString()));
    sql.addBindValue(name);
    sql.addBindValue(name.toLower());
    sql.addBindValue(toBlob(entry.originalSysEx()));
    sql.addBindValue(static_cast<int>(provenance.origin));
    sql.addBindValue(toQt(provenance.sourceName));
    sql.addBindValue(toQt(provenance.sourceDigest));
    sql.addBindValue(QVariant::fromValue<qlonglong>(static_cast<qlonglong>(provenance.sourceByteOffset)));
    sql.addBindValue(QVariant::fromValue<qlonglong>(static_cast<qlonglong>(provenance.sourceByteCount)));
    sql.addBindValue(QVariant::fromValue<qlonglong>(provenance.address.value()));
    sql.addBindValue(provenance.userNumber ? QVariant(*provenance.userNumber) : QVariant(QMetaType(QMetaType::Int)));
    sql.addBindValue(provenance.deviceId ? QVariant(static_cast<int>(provenance.deviceId->byte()))
                                         : QVariant(QMetaType(QMetaType::Int)));
    sql.addBindValue(provenance.modelId ? QVariant(toBlob(provenance.modelId->bytes()))
                                        : QVariant(QMetaType(QMetaType::QByteArray)));
    sql.addBindValue(QVariant::fromValue<qlonglong>(
        std::chrono::duration_cast<std::chrono::seconds>(provenance.importedAt.time_since_epoch()).count()));
    sql.addBindValue(metadata.favourite ? 1 : 0);
    sql.addBindValue(metadata.rating);
    sql.addBindValue(toQt(metadata.category));
    sql.addBindValue(toQt(metadata.notes));

    if (!sql.exec()) {
        errorOut = sql.lastError().text();
        return false;
    }
    idOut = sql.lastInsertId().toLongLong();

    for (const auto& tag : metadata.tags) {
        sql.prepare(QStringLiteral("INSERT OR IGNORE INTO patch_tags (patch_id, tag) VALUES (?, ?)"));
        sql.addBindValue(QVariant::fromValue<qlonglong>(idOut));
        sql.addBindValue(toQt(tag));
        if (!sql.exec()) {
            errorOut = sql.lastError().text();
            return false;
        }
    }
    return true;
}

} // namespace

std::optional<std::int64_t> LibraryDatabase::insert(const LibraryEntry& entry)
{
    const auto ids = insertAll({entry});
    if (!ids || ids->empty()) {
        return std::nullopt;
    }
    return ids->front();
}

std::optional<std::vector<std::int64_t>> LibraryDatabase::insertAll(const std::vector<LibraryEntry>& entries)
{
    if (!isOpen()) {
        m_lastError = QStringLiteral("The library is not open.");
        return std::nullopt;
    }
    for (const auto& entry : entries) {
        if (!PatchUserMetadata::isValidRating(entry.userMetadata().rating)) {
            m_lastError = QStringLiteral("Rating %1 is outside 0..5.").arg(entry.userMetadata().rating);
            return std::nullopt;
        }
    }

    // All or nothing: a failed or cancelled bank import must not leave the
    // library half-populated.
    if (!m_d->database.transaction()) {
        m_lastError = m_d->database.lastError().text();
        return std::nullopt;
    }

    std::vector<std::int64_t> ids;
    ids.reserve(entries.size());
    auto sql = m_d->query();
    for (const auto& entry : entries) {
        std::int64_t id = 0;
        if (!insertOne(sql, entry, id, m_lastError)) {
            m_d->database.rollback();
            return std::nullopt;
        }
        ids.push_back(id);
    }
    if (!m_d->database.commit()) {
        m_lastError = m_d->database.lastError().text();
        m_d->database.rollback();
        return std::nullopt;
    }
    m_lastError.clear();
    return ids;
}

bool LibraryDatabase::updateUserMetadata(std::int64_t id, const PatchUserMetadata& metadata)
{
    if (!isOpen()) {
        m_lastError = QStringLiteral("The library is not open.");
        return false;
    }
    if (!PatchUserMetadata::isValidRating(metadata.rating)) {
        m_lastError = QStringLiteral("Rating %1 is outside 0..5.").arg(metadata.rating);
        return false;
    }
    if (!m_d->database.transaction()) {
        m_lastError = m_d->database.lastError().text();
        return false;
    }

    auto sql = m_d->query();
    sql.prepare(QStringLiteral(
        "UPDATE patches SET favourite = ?, rating = ?, category = ?, notes = ? WHERE id = ?"));
    sql.addBindValue(metadata.favourite ? 1 : 0);
    sql.addBindValue(metadata.rating);
    sql.addBindValue(toQt(metadata.category));
    sql.addBindValue(toQt(metadata.notes));
    sql.addBindValue(QVariant::fromValue<qlonglong>(id));
    if (!sql.exec()) {
        m_lastError = sql.lastError().text();
        m_d->database.rollback();
        return false;
    }
    if (sql.numRowsAffected() == 0) {
        m_lastError = QStringLiteral("No library entry with id %1.").arg(id);
        m_d->database.rollback();
        return false;
    }

    sql.prepare(QStringLiteral("DELETE FROM patch_tags WHERE patch_id = ?"));
    sql.addBindValue(QVariant::fromValue<qlonglong>(id));
    if (!sql.exec()) {
        m_lastError = sql.lastError().text();
        m_d->database.rollback();
        return false;
    }
    for (const auto& tag : metadata.tags) {
        sql.prepare(QStringLiteral("INSERT OR IGNORE INTO patch_tags (patch_id, tag) VALUES (?, ?)"));
        sql.addBindValue(QVariant::fromValue<qlonglong>(id));
        sql.addBindValue(toQt(tag));
        if (!sql.exec()) {
            m_lastError = sql.lastError().text();
            m_d->database.rollback();
            return false;
        }
    }

    if (!m_d->database.commit()) {
        m_lastError = m_d->database.lastError().text();
        m_d->database.rollback();
        return false;
    }
    m_lastError.clear();
    return true;
}

bool LibraryDatabase::remove(std::int64_t id)
{
    if (!isOpen()) {
        m_lastError = QStringLiteral("The library is not open.");
        return false;
    }
    auto sql = m_d->query();
    sql.prepare(QStringLiteral("DELETE FROM patches WHERE id = ?"));
    sql.addBindValue(QVariant::fromValue<qlonglong>(id));
    if (!sql.exec()) {
        m_lastError = sql.lastError().text();
        return false;
    }
    if (sql.numRowsAffected() == 0) {
        m_lastError = QStringLiteral("No library entry with id %1.").arg(id);
        return false;
    }
    m_lastError.clear();
    return true;
}

// ---------------------------------------------------------------------------
// Reading
// ---------------------------------------------------------------------------

namespace {

constexpr const char* kRecordColumns =
    "p.id, p.fingerprint, p.name, p.origin, p.source_name, p.source_digest, p.source_byte_offset, "
    "p.source_byte_count, p.address, p.user_number, p.device_id, p.model_id, p.imported_at, "
    "p.favourite, p.rating, p.category, p.notes, LENGTH(p.original_sysex)";

LibraryRecord readRecord(const QSqlQuery& sql)
{
    LibraryRecord record;
    record.id = sql.value(0).toLongLong();
    if (const auto fingerprint = PatchFingerprint::fromHexString(fromQt(sql.value(1).toString()))) {
        record.fingerprint = *fingerprint;
    }
    record.name = fromQt(sql.value(2).toString());

    auto& provenance = record.provenance;
    provenance.origin = static_cast<PatchOrigin>(sql.value(3).toInt());
    provenance.sourceName = fromQt(sql.value(4).toString());
    provenance.sourceDigest = fromQt(sql.value(5).toString());
    provenance.sourceByteOffset = static_cast<std::uint64_t>(sql.value(6).toLongLong());
    provenance.sourceByteCount = static_cast<std::uint64_t>(sql.value(7).toLongLong());
    if (const auto address = roland::RolandAddress::fromValue(static_cast<std::uint64_t>(sql.value(8).toLongLong()))) {
        provenance.address = *address;
    }
    if (!sql.value(9).isNull()) {
        provenance.userNumber = sql.value(9).toInt();
    }
    if (!sql.value(10).isNull()) {
        provenance.deviceId = roland::RolandDeviceId::fromByte(static_cast<roland::Byte>(sql.value(10).toInt()));
    }
    if (!sql.value(11).isNull()) {
        const auto bytes = fromBlob(sql.value(11).toByteArray());
        provenance.modelId = roland::RolandModelId::fromBytes(bytes);
    }
    provenance.importedAt =
        std::chrono::system_clock::time_point(std::chrono::seconds(sql.value(12).toLongLong()));

    auto& metadata = record.userMetadata;
    metadata.favourite = sql.value(13).toInt() != 0;
    metadata.rating = sql.value(14).toInt();
    metadata.category = fromQt(sql.value(15).toString());
    metadata.notes = fromQt(sql.value(16).toString());

    record.originalSysExSize = sql.value(17).toLongLong();
    return record;
}

} // namespace

std::optional<LibraryRecord> LibraryDatabase::record(std::int64_t id) const
{
    if (!isOpen()) {
        m_lastError = QStringLiteral("The library is not open.");
        return std::nullopt;
    }
    auto sql = m_d->query();
    sql.prepare(QStringLiteral("SELECT %1 FROM patches p WHERE p.id = ?").arg(QString::fromLatin1(kRecordColumns)));
    sql.addBindValue(QVariant::fromValue<qlonglong>(id));
    if (!sql.exec() || !sql.next()) {
        m_lastError = sql.lastError().isValid() ? sql.lastError().text()
                                                : QStringLiteral("No library entry with id %1.").arg(id);
        return std::nullopt;
    }
    auto record = readRecord(sql);

    auto tags = m_d->query();
    tags.prepare(QStringLiteral("SELECT tag FROM patch_tags WHERE patch_id = ? ORDER BY tag"));
    tags.addBindValue(QVariant::fromValue<qlonglong>(id));
    if (tags.exec()) {
        while (tags.next()) {
            record.userMetadata.tags.push_back(fromQt(tags.value(0).toString()));
        }
    }
    m_lastError.clear();
    return record;
}

std::optional<roland::ByteVector> LibraryDatabase::originalSysEx(std::int64_t id) const
{
    if (!isOpen()) {
        m_lastError = QStringLiteral("The library is not open.");
        return std::nullopt;
    }
    auto sql = m_d->query();
    sql.prepare(QStringLiteral("SELECT original_sysex FROM patches WHERE id = ?"));
    sql.addBindValue(QVariant::fromValue<qlonglong>(id));
    if (!sql.exec() || !sql.next()) {
        m_lastError = QStringLiteral("No library entry with id %1.").arg(id);
        return std::nullopt;
    }
    m_lastError.clear();
    return fromBlob(sql.value(0).toByteArray());
}

std::optional<LibraryEntry> LibraryDatabase::loadEntry(std::int64_t id) const
{
    const auto record = this->record(id);
    const auto bytes = originalSysEx(id);
    if (!record || !bytes) {
        return std::nullopt;
    }

    // The Patch comes back through the same codec an import uses, from the
    // bytes that were preserved. Performance-mode Parts are allowed here
    // because an entry may legitimately have come from one.
    SyxImportOptions options;
    options.sourceName = record->provenance.sourceName;
    options.includePerformanceParts = true;
    const auto imported = importSyxStream(*bytes, options);
    if (imported.entries.size() != 1) {
        m_lastError = QStringLiteral("The stored SysEx for entry %1 no longer decodes to exactly one Patch (%2).")
                          .arg(id)
                          .arg(toQt(imported.summary()));
        return std::nullopt;
    }

    LibraryEntry entry(imported.entries.front().patch(), *bytes, record->provenance);
    entry.userMetadata() = record->userMetadata;
    if (entry.fingerprint() != record->fingerprint) {
        m_lastError = QStringLiteral("Entry %1 no longer fingerprints as it did when stored "
                                     "(stored %2, recomputed %3).")
                          .arg(id)
                          .arg(toQt(record->fingerprint.toShortString()))
                          .arg(toQt(entry.fingerprint().toShortString()));
        return std::nullopt;
    }
    m_lastError.clear();
    return entry;
}

std::vector<LibraryRecord> LibraryDatabase::search(const LibraryQuery& query) const
{
    std::vector<LibraryRecord> records;
    if (!isOpen()) {
        m_lastError = QStringLiteral("The library is not open.");
        return records;
    }

    const auto filter = buildFilter(query);
    QString text = QStringLiteral("SELECT %1 FROM patches p").arg(QString::fromLatin1(kRecordColumns))
                   + filter.where + QString::fromLatin1(orderClause(query.order));
    if (query.limit > 0) {
        text += QStringLiteral(" LIMIT %1").arg(query.limit);
    } else if (query.offset > 0) {
        text += QStringLiteral(" LIMIT -1");
    }
    if (query.offset > 0) {
        text += QStringLiteral(" OFFSET %1").arg(query.offset);
    }

    auto sql = m_d->query();
    sql.prepare(text);
    bindAll(sql, filter.values);
    if (!sql.exec()) {
        m_lastError = sql.lastError().text();
        return records;
    }
    std::map<std::int64_t, std::size_t> indexById;
    while (sql.next()) {
        indexById.emplace(sql.value(0).toLongLong(), records.size());
        records.push_back(readRecord(sql));
    }

    // Tags for the page that was actually returned, in one query rather than
    // one per row.
    if (!records.empty()) {
        QStringList ids;
        for (const auto& [id, index] : indexById) {
            ids << QString::number(id);
        }
        auto tags = m_d->query();
        if (tags.exec(QStringLiteral("SELECT patch_id, tag FROM patch_tags WHERE patch_id IN (%1) ORDER BY tag")
                          .arg(ids.join(QStringLiteral(", "))))) {
            while (tags.next()) {
                const auto it = indexById.find(tags.value(0).toLongLong());
                if (it != indexById.end()) {
                    records[it->second].userMetadata.tags.push_back(fromQt(tags.value(1).toString()));
                }
            }
        }
    }

    m_lastError.clear();
    return records;
}

std::optional<int> LibraryDatabase::count(const LibraryQuery& query) const
{
    if (!isOpen()) {
        m_lastError = QStringLiteral("The library is not open.");
        return std::nullopt;
    }
    const auto filter = buildFilter(query);
    auto sql = m_d->query();
    sql.prepare(QStringLiteral("SELECT COUNT(*) FROM patches p") + filter.where);
    bindAll(sql, filter.values);
    if (!sql.exec() || !sql.next()) {
        m_lastError = sql.lastError().text();
        return std::nullopt;
    }
    m_lastError.clear();
    return sql.value(0).toInt();
}

std::optional<int> LibraryDatabase::totalCount() const
{
    return count(LibraryQuery{});
}

std::vector<LibraryRecord> LibraryDatabase::findDuplicatesOf(std::int64_t id) const
{
    std::vector<LibraryRecord> duplicates;
    const auto source = record(id);
    if (!source) {
        return duplicates;
    }
    LibraryQuery query;
    query.fingerprint = source->fingerprint;
    query.order = LibraryQuery::Order::ImportedOldestFirst;
    for (auto& candidate : search(query)) {
        if (candidate.id != id) {
            duplicates.push_back(std::move(candidate));
        }
    }
    return duplicates;
}

std::vector<std::string> LibraryDatabase::categoriesInUse() const
{
    std::vector<std::string> values;
    if (!isOpen()) {
        return values;
    }
    auto sql = m_d->query();
    if (!sql.exec(QStringLiteral("SELECT DISTINCT category FROM patches WHERE category <> '' ORDER BY category"))) {
        m_lastError = sql.lastError().text();
        return values;
    }
    while (sql.next()) {
        values.push_back(fromQt(sql.value(0).toString()));
    }
    return values;
}

std::vector<std::string> LibraryDatabase::tagsInUse() const
{
    std::vector<std::string> values;
    if (!isOpen()) {
        return values;
    }
    auto sql = m_d->query();
    if (!sql.exec(QStringLiteral("SELECT DISTINCT tag FROM patch_tags ORDER BY tag"))) {
        m_lastError = sql.lastError().text();
        return values;
    }
    while (sql.next()) {
        values.push_back(fromQt(sql.value(0).toString()));
    }
    return values;
}

} // namespace xp60studio::library
