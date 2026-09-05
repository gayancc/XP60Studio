#include "library/LibraryDatabase.h"

#include "library/PatchCompatibility.h"
#include "library/SyxImport.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>

#include <algorithm>
#include <chrono>
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
    // Schema version 2 -- bank engineering.
    //
    // A bank is an arrangement of references, so `bank_slots.patch_id` is
    // ON DELETE SET NULL rather than CASCADE: deleting a Patch from the
    // library must not delete the destination it occupied in somebody's bank.
    // The cached name survives the deletion, which is what lets the Bank
    // Builder say "GrandPiano -- no longer in the library" instead of quietly
    // presenting A35 as free.
    "CREATE TABLE IF NOT EXISTS banks ("
    "  id         INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  name       TEXT    NOT NULL,"
    "  created_at INTEGER NOT NULL,"
    "  updated_at INTEGER NOT NULL"
    ")",
    "CREATE TABLE IF NOT EXISTS bank_slots ("
    "  bank_id     INTEGER NOT NULL REFERENCES banks(id) ON DELETE CASCADE,"
    "  slot_index  INTEGER NOT NULL,"
    "  patch_id    INTEGER          REFERENCES patches(id) ON DELETE SET NULL,"
    "  patch_name  TEXT    NOT NULL DEFAULT '',"
    "  source_name TEXT    NOT NULL DEFAULT '',"
    "  source_slot TEXT    NOT NULL DEFAULT '',"
    "  PRIMARY KEY (bank_id, slot_index)"
    ")",
    "CREATE INDEX IF NOT EXISTS idx_bank_slots_patch ON bank_slots(patch_id)",
    // Schema version 3 -- the section rail.
    //
    // A musician's own grouping of a bank's 128 destinations ("pianos at the
    // front, pads after them"). It is XP60Studio's organisation: it changes no
    // address, is never transmitted, and is deleted with its bank.
    "CREATE TABLE IF NOT EXISTS bank_sections ("
    "  bank_id    INTEGER NOT NULL REFERENCES banks(id) ON DELETE CASCADE,"
    "  first_slot INTEGER NOT NULL,"
    "  last_slot  INTEGER NOT NULL,"
    "  name       TEXT    NOT NULL,"
    "  PRIMARY KEY (bank_id, first_slot)"
    ")",
    // Schema version 4 -- the Wave Expansion configuration.
    //
    // One row per occupied slot. `wave_group_id` is nullable on purpose: a
    // board can be installed while XP60Studio does not yet know which Wave
    // Group ID its waves carry, and that is a different state from an empty
    // slot. Which board an ID denotes is not documented
    // (ROLAND_XP60_PROTOCOL_FACTS.md §7), so this is the musician's knowledge,
    // recorded rather than inferred.
    "CREATE TABLE IF NOT EXISTS expansion_slots ("
    "  slot          INTEGER PRIMARY KEY,"
    "  name          TEXT    NOT NULL,"
    "  wave_group_id INTEGER"
    ")",
    // Schema version 5 -- what each Patch needs from an expansion board.
    //
    // This is derived data: the Wave Group IDs already present in the stored
    // SysEx, extracted so that "which of these will play on my XP-60?" is an
    // indexed query rather than a decode of every Patch in the library. The
    // stored bytes remain the only source of truth; these rows can be dropped
    // and rebuilt from them at any time.
    //
    // Two tables, because "no groups" and "not analysed yet" are different
    // facts and a missing row cannot say which. An internal-only Patch has a
    // `patch_expansion_scan` row and no `patch_expansion_groups` rows.
    "CREATE TABLE IF NOT EXISTS patch_expansion_scan ("
    "  patch_id INTEGER PRIMARY KEY REFERENCES patches(id) ON DELETE CASCADE"
    ")",
    "CREATE TABLE IF NOT EXISTS patch_expansion_groups ("
    "  patch_id INTEGER NOT NULL REFERENCES patches(id) ON DELETE CASCADE,"
    "  group_id INTEGER NOT NULL,"
    "  PRIMARY KEY (patch_id, group_id)"
    ")",
    "CREATE INDEX IF NOT EXISTS idx_patch_expansion_group ON patch_expansion_groups(group_id)",
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

    // The expansion filters read only the derived tables, so narrowing a
    // library of thousands to "needs a board you do not have" stays one indexed
    // query and decodes nothing.
    const QString anyGroup = QStringLiteral("EXISTS (SELECT 1 FROM patch_expansion_groups g WHERE g.patch_id = p.id)");
    switch (query.expansion) {
    case LibraryQuery::Expansion::Any:
        break;
    case LibraryQuery::Expansion::InternalOnly:
        clauses << QStringLiteral("NOT ") + anyGroup;
        break;
    case LibraryQuery::Expansion::UsesExpansion:
        clauses << anyGroup;
        break;
    case LibraryQuery::Expansion::NeedsGroupOutsideProfile:
    case LibraryQuery::Expansion::PlaysWithProfile: {
        QStringList placeholders;
        for (const int group : query.providedGroups) {
            placeholders << QStringLiteral("?");
            filter.values << group;
        }
        // With no provided groups the NOT IN list is empty, so every expansion
        // reference qualifies — right for an instrument with no boards in it.
        const QString provided =
            placeholders.isEmpty() ? QString()
                                   : QStringLiteral(" AND g.group_id NOT IN (%1)").arg(placeholders.join(QStringLiteral(", ")));
        const QString needsOne = QStringLiteral("EXISTS (SELECT 1 FROM patch_expansion_groups g "
                                                "WHERE g.patch_id = p.id%1)")
                                     .arg(provided);
        clauses << (query.expansion == LibraryQuery::Expansion::PlaysWithProfile
                        ? QStringLiteral("NOT ") + needsOne
                        : needsOne);
        break;
    }
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

// Records what a Patch needs from an expansion board. The scan row is written
// even when there are no groups: "internal only" is an answer, and without it a
// later reader could not tell it from "never looked".
bool writeExpansionGroups(QSqlQuery& sql, std::int64_t patchId, const std::set<int>& groups, QString& errorOut)
{
    sql.prepare(QStringLiteral("DELETE FROM patch_expansion_groups WHERE patch_id = ?"));
    sql.addBindValue(QVariant::fromValue<qlonglong>(patchId));
    if (!sql.exec()) {
        errorOut = sql.lastError().text();
        return false;
    }
    for (const int group : groups) {
        sql.prepare(QStringLiteral("INSERT OR IGNORE INTO patch_expansion_groups (patch_id, group_id) VALUES (?, ?)"));
        sql.addBindValue(QVariant::fromValue<qlonglong>(patchId));
        sql.addBindValue(group);
        if (!sql.exec()) {
            errorOut = sql.lastError().text();
            return false;
        }
    }
    sql.prepare(QStringLiteral("INSERT OR IGNORE INTO patch_expansion_scan (patch_id) VALUES (?)"));
    sql.addBindValue(QVariant::fromValue<qlonglong>(patchId));
    if (!sql.exec()) {
        errorOut = sql.lastError().text();
        return false;
    }
    return true;
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
    } else if (*version < kSchemaVersion) {
        // Forward migration. Every statement above is CREATE ... IF NOT
        // EXISTS and has already run, so an older library has gained the new
        // tables and nothing else has changed; all that remains is to record
        // which schema this file now is.
        if (!sql.exec(QStringLiteral("UPDATE schema_info SET version = %1").arg(kSchemaVersion))) {
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

    backfillExpansionGroups();

    m_lastError.clear();
    return true;
}

// Derives the expansion-group rows for entries that have none — every entry in
// a library written before schema 5, and nothing else.
//
// This decodes stored bytes; it does not read them for anything but the Wave
// Group IDs already in them, writes no user-visible state, and asks nothing of
// the musician. An entry whose bytes no longer decode is left unscanned rather
// than recorded as internal-only: the library still opens, and every screen
// that reads the derived data says "not analysed" for that row instead of
// quietly calling it safe. Failure here is never a reason to refuse the
// library.
void LibraryDatabase::backfillExpansionGroups()
{
    auto ids = m_d->query();
    if (!ids.exec(QStringLiteral("SELECT p.id FROM patches p "
                                 "WHERE NOT EXISTS (SELECT 1 FROM patch_expansion_scan s WHERE s.patch_id = p.id)"))) {
        return;
    }
    std::vector<std::int64_t> pending;
    while (ids.next()) {
        pending.push_back(ids.value(0).toLongLong());
    }
    if (pending.empty()) {
        return;
    }

    const bool inTransaction = m_d->database.transaction();
    auto sql = m_d->query();
    QString ignored;
    for (const auto id : pending) {
        const auto entry = loadEntry(id);
        if (!entry) {
            continue;
        }
        (void)writeExpansionGroups(sql, id, requiredExpansionGroups(entry->patch()), ignored);
    }
    if (inTransaction && !m_d->database.commit()) {
        m_d->database.rollback();
    }
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
    return writeExpansionGroups(sql, idOut, requiredExpansionGroups(entry.patch()), errorOut);
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

    auto scan = m_d->query();
    scan.prepare(QStringLiteral("SELECT 1 FROM patch_expansion_scan WHERE patch_id = ?"));
    scan.addBindValue(QVariant::fromValue<qlonglong>(id));
    record.expansionScanned = scan.exec() && scan.next();

    auto groups = m_d->query();
    groups.prepare(QStringLiteral("SELECT group_id FROM patch_expansion_groups WHERE patch_id = ? ORDER BY group_id"));
    groups.addBindValue(QVariant::fromValue<qlonglong>(id));
    if (groups.exec()) {
        while (groups.next()) {
            record.expansionGroups.insert(groups.value(0).toInt());
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

        // Two more queries for the whole page, so a scrolling library pays for
        // its compatibility badges once per page rather than once per row —
        // and never by decoding a Patch.
        auto scan = m_d->query();
        if (scan.exec(QStringLiteral("SELECT patch_id FROM patch_expansion_scan WHERE patch_id IN (%1)")
                          .arg(ids.join(QStringLiteral(", "))))) {
            while (scan.next()) {
                const auto it = indexById.find(scan.value(0).toLongLong());
                if (it != indexById.end()) {
                    records[it->second].expansionScanned = true;
                }
            }
        }
        auto groups = m_d->query();
        if (groups.exec(QStringLiteral("SELECT patch_id, group_id FROM patch_expansion_groups "
                                       "WHERE patch_id IN (%1) ORDER BY group_id")
                            .arg(ids.join(QStringLiteral(", "))))) {
            while (groups.next()) {
                const auto it = indexById.find(groups.value(0).toLongLong());
                if (it != indexById.end()) {
                    records[it->second].expansionGroups.insert(groups.value(1).toInt());
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

// ---------------------------------------------------------------------------
// Bank engineering
// ---------------------------------------------------------------------------

namespace {

std::int64_t toEpochSeconds(std::chrono::system_clock::time_point when)
{
    return std::chrono::duration_cast<std::chrono::seconds>(when.time_since_epoch()).count();
}

std::chrono::system_clock::time_point fromEpochSeconds(std::int64_t seconds)
{
    return std::chrono::system_clock::time_point(std::chrono::seconds(seconds));
}

} // namespace

std::optional<std::int64_t> LibraryDatabase::saveBank(const std::string& name,
    const std::vector<BankSlotContent>& destinations, std::optional<std::int64_t> existingId,
    const std::vector<BankSection>& sections)
{
    if (!isOpen()) {
        m_lastError = QStringLiteral("The library is not open.");
        return std::nullopt;
    }
    if (name.empty()) {
        m_lastError = QStringLiteral("A saved bank needs a name.");
        return std::nullopt;
    }
    if (destinations.size() > static_cast<std::size_t>(BankDraft::kSlotCount)) {
        // Refuse rather than truncate: writing 128 of 140 destinations would
        // silently drop part of the user's arrangement.
        m_lastError = QStringLiteral("A User bank holds %1 destinations; %2 were given.")
                          .arg(BankDraft::kSlotCount)
                          .arg(destinations.size());
        return std::nullopt;
    }

    const auto now = toEpochSeconds(std::chrono::system_clock::now());

    if (!m_d->database.transaction()) {
        m_lastError = m_d->database.lastError().text();
        return std::nullopt;
    }

    auto fail = [this](const QString& error) -> std::optional<std::int64_t> {
        m_lastError = error;
        m_d->database.rollback();
        return std::nullopt;
    };

    auto sql = m_d->query();
    std::int64_t bankId = 0;
    if (existingId) {
        sql.prepare(QStringLiteral("UPDATE banks SET name = ?, updated_at = ? WHERE id = ?"));
        sql.addBindValue(toQt(name));
        sql.addBindValue(QVariant::fromValue<qlonglong>(now));
        sql.addBindValue(QVariant::fromValue<qlonglong>(*existingId));
        if (!sql.exec()) {
            return fail(sql.lastError().text());
        }
        if (sql.numRowsAffected() == 0) {
            return fail(QStringLiteral("No saved bank with id %1.").arg(*existingId));
        }
        bankId = *existingId;
        sql.prepare(QStringLiteral("DELETE FROM bank_slots WHERE bank_id = ?"));
        sql.addBindValue(QVariant::fromValue<qlonglong>(bankId));
        if (!sql.exec()) {
            return fail(sql.lastError().text());
        }
        sql.prepare(QStringLiteral("DELETE FROM bank_sections WHERE bank_id = ?"));
        sql.addBindValue(QVariant::fromValue<qlonglong>(bankId));
        if (!sql.exec()) {
            return fail(sql.lastError().text());
        }
    } else {
        sql.prepare(QStringLiteral("INSERT INTO banks (name, created_at, updated_at) VALUES (?, ?, ?)"));
        sql.addBindValue(toQt(name));
        sql.addBindValue(QVariant::fromValue<qlonglong>(now));
        sql.addBindValue(QVariant::fromValue<qlonglong>(now));
        if (!sql.exec()) {
            return fail(sql.lastError().text());
        }
        bankId = sql.lastInsertId().toLongLong();
    }

    // Only occupied destinations are stored. An empty destination is the
    // absence of a row, so a mostly empty bank costs almost nothing and the
    // 128 positions are always reconstructed from the slot index.
    for (std::size_t index = 0; index < destinations.size(); ++index) {
        const auto& content = destinations[index];
        if (content.empty()) {
            continue;
        }
        sql.prepare(QStringLiteral("INSERT INTO bank_slots (bank_id, slot_index, patch_id, patch_name, "
                                   "source_name, source_slot) VALUES (?, ?, ?, ?, ?, ?)"));
        sql.addBindValue(QVariant::fromValue<qlonglong>(bankId));
        sql.addBindValue(static_cast<int>(index));
        // A destination whose Patch is already gone keeps its null reference
        // and its name, so saving never repairs history by inventing an id.
        sql.addBindValue(content.patchId > 0 ? QVariant::fromValue<qlonglong>(content.patchId) : QVariant());
        sql.addBindValue(toQt(content.patchName));
        sql.addBindValue(toQt(content.sourceName));
        sql.addBindValue(toQt(content.sourceSlotLabel));
        if (!sql.exec()) {
            return fail(sql.lastError().text());
        }
    }

    for (const auto& section : sections) {
        if (section.name.empty()) {
            continue;
        }
        sql.prepare(QStringLiteral(
            "INSERT INTO bank_sections (bank_id, first_slot, last_slot, name) VALUES (?, ?, ?, ?)"));
        sql.addBindValue(QVariant::fromValue<qlonglong>(bankId));
        sql.addBindValue(section.firstSlot);
        sql.addBindValue(section.lastSlot);
        sql.addBindValue(toQt(section.name));
        if (!sql.exec()) {
            return fail(sql.lastError().text());
        }
    }

    if (!m_d->database.commit()) {
        const QString error = m_d->database.lastError().text();
        m_d->database.rollback();
        m_lastError = error;
        return std::nullopt;
    }
    m_lastError.clear();
    return bankId;
}

std::vector<SavedBankRecord> LibraryDatabase::banks() const
{
    std::vector<SavedBankRecord> records;
    if (!isOpen()) {
        return records;
    }
    auto sql = m_d->query();
    if (!sql.exec(QStringLiteral(
            "SELECT b.id, b.name, b.created_at, b.updated_at, "
            "  (SELECT COUNT(*) FROM bank_slots s WHERE s.bank_id = b.id), "
            "  (SELECT COUNT(*) FROM bank_slots s WHERE s.bank_id = b.id AND s.patch_id IS NULL) "
            "FROM banks b ORDER BY b.updated_at DESC, b.id DESC"))) {
        m_lastError = sql.lastError().text();
        return records;
    }
    while (sql.next()) {
        SavedBankRecord record;
        record.id = sql.value(0).toLongLong();
        record.name = fromQt(sql.value(1).toString());
        record.createdAt = fromEpochSeconds(sql.value(2).toLongLong());
        record.updatedAt = fromEpochSeconds(sql.value(3).toLongLong());
        record.occupiedCount = sql.value(4).toInt();
        record.missingCount = sql.value(5).toInt();
        records.push_back(std::move(record));
    }
    return records;
}

std::optional<SavedBank> LibraryDatabase::loadBank(std::int64_t id) const
{
    if (!isOpen()) {
        m_lastError = QStringLiteral("The library is not open.");
        return std::nullopt;
    }
    auto sql = m_d->query();
    sql.prepare(QStringLiteral("SELECT name, created_at, updated_at FROM banks WHERE id = ?"));
    sql.addBindValue(QVariant::fromValue<qlonglong>(id));
    if (!sql.exec()) {
        m_lastError = sql.lastError().text();
        return std::nullopt;
    }
    if (!sql.next()) {
        m_lastError = QStringLiteral("No saved bank with id %1.").arg(id);
        return std::nullopt;
    }

    SavedBank bank;
    bank.record.id = id;
    bank.record.name = fromQt(sql.value(0).toString());
    bank.record.createdAt = fromEpochSeconds(sql.value(1).toLongLong());
    bank.record.updatedAt = fromEpochSeconds(sql.value(2).toLongLong());
    bank.destinations.assign(static_cast<std::size_t>(BankDraft::kSlotCount), BankSlotContent{});

    auto slotQuery = m_d->query();
    slotQuery.prepare(QStringLiteral("SELECT slot_index, patch_id, patch_name, source_name, source_slot "
                                     "FROM bank_slots WHERE bank_id = ? ORDER BY slot_index"));
    slotQuery.addBindValue(QVariant::fromValue<qlonglong>(id));
    if (!slotQuery.exec()) {
        m_lastError = slotQuery.lastError().text();
        return std::nullopt;
    }
    while (slotQuery.next()) {
        const int index = slotQuery.value(0).toInt();
        if (index < 0 || index >= BankDraft::kSlotCount) {
            continue;
        }
        BankSlotContent content;
        const QVariant patchId = slotQuery.value(1);
        content.patchId = patchId.isNull() ? 0 : patchId.toLongLong();
        content.patchName = fromQt(slotQuery.value(2).toString());
        content.sourceName = fromQt(slotQuery.value(3).toString());
        content.sourceSlotLabel = fromQt(slotQuery.value(4).toString());
        // The row exists, so the user put something here. A null reference
        // means the Patch was deleted from the library afterwards.
        content.missing = content.patchId == 0;
        if (content.missing) {
            ++bank.record.missingCount;
        }
        ++bank.record.occupiedCount;
        bank.destinations[static_cast<std::size_t>(index)] = std::move(content);
    }

    auto sectionQuery = m_d->query();
    sectionQuery.prepare(QStringLiteral("SELECT first_slot, last_slot, name FROM bank_sections "
                                        "WHERE bank_id = ? ORDER BY first_slot"));
    sectionQuery.addBindValue(QVariant::fromValue<qlonglong>(id));
    if (!sectionQuery.exec()) {
        m_lastError = sectionQuery.lastError().text();
        return std::nullopt;
    }
    while (sectionQuery.next()) {
        BankSection section;
        section.firstSlot = sectionQuery.value(0).toInt();
        section.lastSlot = sectionQuery.value(1).toInt();
        section.name = fromQt(sectionQuery.value(2).toString());
        bank.sections.push_back(std::move(section));
    }

    m_lastError.clear();
    return bank;
}

bool LibraryDatabase::removeBank(std::int64_t id)
{
    if (!isOpen()) {
        m_lastError = QStringLiteral("The library is not open.");
        return false;
    }
    auto sql = m_d->query();
    sql.prepare(QStringLiteral("DELETE FROM banks WHERE id = ?"));
    sql.addBindValue(QVariant::fromValue<qlonglong>(id));
    if (!sql.exec()) {
        m_lastError = sql.lastError().text();
        return false;
    }
    if (sql.numRowsAffected() == 0) {
        m_lastError = QStringLiteral("No saved bank with id %1.").arg(id);
        return false;
    }
    m_lastError.clear();
    return true;
}

bool LibraryDatabase::saveExpansionProfile(const ExpansionProfile& profile)
{
    if (!isOpen()) {
        m_lastError = QStringLiteral("The library is not open.");
        return false;
    }
    if (!m_d->database.transaction()) {
        m_lastError = m_d->database.lastError().text();
        return false;
    }
    auto sql = m_d->query();
    if (!sql.exec(QStringLiteral("DELETE FROM expansion_slots"))) {
        m_lastError = sql.lastError().text();
        m_d->database.rollback();
        return false;
    }
    for (const auto& board : profile.boards()) {
        // An empty slot is the absence of a row, as an empty bank destination
        // is: there is nothing to record about a slot with no board in it.
        if (board.name.empty()) {
            continue;
        }
        sql.prepare(QStringLiteral("INSERT INTO expansion_slots (slot, name, wave_group_id) VALUES (?, ?, ?)"));
        sql.addBindValue(board.slot);
        sql.addBindValue(toQt(board.name));
        sql.addBindValue(board.waveGroupId ? QVariant(*board.waveGroupId) : QVariant());
        if (!sql.exec()) {
            m_lastError = sql.lastError().text();
            m_d->database.rollback();
            return false;
        }
    }
    if (!m_d->database.commit()) {
        const QString error = m_d->database.lastError().text();
        m_d->database.rollback();
        m_lastError = error;
        return false;
    }
    m_lastError.clear();
    return true;
}

std::optional<ExpansionProfile> LibraryDatabase::loadExpansionProfile() const
{
    if (!isOpen()) {
        m_lastError = QStringLiteral("The library is not open.");
        return std::nullopt;
    }
    auto sql = m_d->query();
    if (!sql.exec(QStringLiteral("SELECT slot, name, wave_group_id FROM expansion_slots ORDER BY slot"))) {
        m_lastError = sql.lastError().text();
        return std::nullopt;
    }
    ExpansionProfile profile;
    std::vector<ExpansionBoard> boards;
    while (sql.next()) {
        ExpansionBoard board;
        board.slot = sql.value(0).toInt();
        board.name = fromQt(sql.value(1).toString());
        const QVariant group = sql.value(2);
        // A null group is "installed, but we cannot yet tell which waves are
        // its" -- preserved rather than defaulted to a plausible number.
        if (!group.isNull()) {
            board.waveGroupId = group.toInt();
        }
        boards.push_back(std::move(board));
    }
    profile.setBoards(std::move(boards));
    m_lastError.clear();
    return profile;
}

std::map<std::int64_t, std::set<int>> LibraryDatabase::expansionGroupsOf(const std::vector<std::int64_t>& ids) const
{
    std::map<std::int64_t, std::set<int>> result;
    if (!isOpen() || ids.empty()) {
        return result;
    }
    QStringList list;
    for (const auto id : ids) {
        list << QString::number(id);
    }
    const auto joined = list.join(QStringLiteral(", "));

    // The scan rows first, so an internal-only Patch comes back as a present,
    // empty entry rather than looking like one nobody analysed.
    auto scan = m_d->query();
    if (!scan.exec(QStringLiteral("SELECT patch_id FROM patch_expansion_scan WHERE patch_id IN (%1)").arg(joined))) {
        m_lastError = scan.lastError().text();
        return result;
    }
    while (scan.next()) {
        result.emplace(scan.value(0).toLongLong(), std::set<int>{});
    }

    auto groups = m_d->query();
    if (groups.exec(QStringLiteral("SELECT patch_id, group_id FROM patch_expansion_groups "
                                   "WHERE patch_id IN (%1) ORDER BY group_id")
                        .arg(joined))) {
        while (groups.next()) {
            const auto it = result.find(groups.value(0).toLongLong());
            if (it != result.end()) {
                it->second.insert(groups.value(1).toInt());
            }
        }
    }
    m_lastError.clear();
    return result;
}

std::vector<LibrarySourceSummary> LibraryDatabase::sourcesInUse() const
{
    std::vector<LibrarySourceSummary> sources;
    if (!isOpen()) {
        return sources;
    }
    auto sql = m_d->query();
    // Grouped by digest, because that is what identifies one file no matter
    // what it was called when it was imported. Entries with no digest -- a
    // device read, for instance -- group by their source name instead.
    if (!sql.exec(QStringLiteral(
            "SELECT source_digest, source_name, COUNT(*), MAX(imported_at) FROM patches "
            "GROUP BY source_digest, source_name ORDER BY MAX(imported_at) DESC, source_name ASC"))) {
        m_lastError = sql.lastError().text();
        return sources;
    }
    while (sql.next()) {
        LibrarySourceSummary summary;
        summary.digest = fromQt(sql.value(0).toString());
        summary.name = fromQt(sql.value(1).toString());
        summary.patchCount = sql.value(2).toInt();
        summary.importedAt = fromEpochSeconds(sql.value(3).toLongLong());
        sources.push_back(std::move(summary));
    }
    return sources;
}

} // namespace xp60studio::library
