#include "services/LibraryExportService.h"

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace xp60studio::services {

QString LibraryExportResult::summary() const
{
    if (!ok) {
        return error.isEmpty() ? QStringLiteral("Export failed") : error;
    }
    auto text = QStringLiteral("%1 patch%2, %3 message%4, %5 bytes")
                    .arg(patchCount)
                    .arg(patchCount == 1 ? QString() : QStringLiteral("es"))
                    .arg(messageCount)
                    .arg(messageCount == 1 ? QString() : QStringLiteral("s"))
                    .arg(byteCount);
    if (!notes.isEmpty()) {
        text += QStringLiteral(" (%1 note%2)").arg(notes.size()).arg(notes.size() == 1 ? QString() : QStringLiteral("s"));
    }
    return text;
}

LibraryExportService::LibraryExportService(library::LibraryDatabase& database, QObject* parent)
    : QObject(parent)
    , m_database(database)
{
}

LibraryExportResult LibraryExportService::exportToFile(const std::vector<std::int64_t>& ids, const QString& path,
    const library::SyxExportOptions& options)
{
    LibraryExportResult result;
    result.path = path;

    if (ids.empty()) {
        result.error = QStringLiteral("Nothing selected to export");
        return result;
    }
    if (path.isEmpty()) {
        result.error = QStringLiteral("No destination given");
        return result;
    }

    std::vector<library::LibraryEntry> entries;
    entries.reserve(ids.size());
    for (const auto id : ids) {
        auto entry = m_database.loadEntry(id);
        if (!entry) {
            result.missingIds.push_back(id);
            continue;
        }
        entries.push_back(std::move(*entry));
    }
    if (!result.missingIds.empty()) {
        // Refuse rather than write a short file. Nothing downstream would show
        // that Patches had gone missing between the selection and the file.
        result.error = QStringLiteral("%1 of %2 selected patches could not be read from the library; nothing written")
                           .arg(result.missingIds.size())
                           .arg(ids.size());
        return result;
    }

    const auto exported = library::exportEntries(entries, options);
    if (!exported.ok) {
        result.error = QString::fromStdString(exported.error);
        return result;
    }
    for (const auto& note : exported.notes) {
        result.notes.append(QString::fromStdString(note));
    }

    // QSaveFile writes to a temporary beside the target and renames on commit,
    // so an interrupted write cannot leave a truncated .syx in its place.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        result.error = QStringLiteral("Cannot write %1: %2").arg(QFileInfo(path).fileName(), file.errorString());
        return result;
    }
    const auto written = file.write(reinterpret_cast<const char*>(exported.bytes.data()),
        static_cast<qint64>(exported.bytes.size()));
    if (written != static_cast<qint64>(exported.bytes.size()) || !file.commit()) {
        result.error = QStringLiteral("Cannot write %1: %2").arg(QFileInfo(path).fileName(), file.errorString());
        return result;
    }

    result.ok = true;
    result.patchCount = exported.patchCount;
    result.messageCount = exported.messageCount;
    result.byteCount = exported.bytes.size();
    return result;
}

} // namespace xp60studio::services
