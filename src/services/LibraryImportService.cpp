#include "services/LibraryImportService.h"

#include <QFile>
#include <QFileInfo>
#include <QtConcurrent/QtConcurrent>

namespace xp60studio::services {
namespace {

roland::ByteVector toBytes(const QByteArray& data)
{
    const auto* begin = reinterpret_cast<const roland::Byte*>(data.constData());
    return roland::ByteVector(begin, begin + data.size());
}

QString countPhrase(std::size_t count, const char* singular, const char* plural)
{
    return QStringLiteral("%1 %2").arg(count).arg(QString::fromLatin1(count == 1 ? singular : plural));
}

} // namespace

QString LibraryImportFileResult::describe() const
{
    if (!error.isEmpty()) {
        return QStringLiteral("%1: %2").arg(displayName, error);
    }
    QStringList parts;
    parts << countPhrase(patchesStored, "patch stored", "patches stored");
    if (patchesFound != patchesStored) {
        parts << QStringLiteral("%1 found").arg(patchesFound);
    }
    if (partial > 0) {
        parts << countPhrase(partial, "incomplete patch", "incomplete patches");
    }
    if (rejected > 0) {
        parts << countPhrase(rejected, "rejected patch", "rejected patches");
    }
    if (warnings > 0) {
        parts << QStringLiteral("%1 with values outside the documented ranges").arg(warnings);
    }
    if (duplicatesWithinFile > 0) {
        parts << countPhrase(duplicatesWithinFile, "duplicate pair inside the file",
                             "duplicate pairs inside the file");
    }
    if (duplicatesInLibrary > 0) {
        parts << countPhrase(duplicatesInLibrary, "patch already in the library",
                             "patches already in the library");
    }
    if (unattributedDataSets > 0) {
        parts << QStringLiteral("%1 data sets outside any Patch").arg(unattributedDataSets);
    }
    if (!streamClean) {
        parts << QStringLiteral("stream not clean");
    }
    return QStringLiteral("%1: %2").arg(displayName, parts.join(QStringLiteral(", ")));
}

QString LibraryImportSummary::summary() const
{
    QStringList parts;
    parts << countPhrase(files.size(), "file", "files");
    parts << countPhrase(patchesStored, "patch stored", "patches stored");
    if (filesFailed > 0) {
        parts << countPhrase(filesFailed, "file failed", "files failed");
    }
    if (cancelled) {
        parts << QStringLiteral("cancelled");
    }
    return parts.join(QStringLiteral(", "));
}

// ---------------------------------------------------------------------------

LibraryImportService::LibraryImportService(library::LibraryDatabase& database, QObject* parent)
    : QObject(parent)
    , m_database(database)
{
}

LibraryImportService::~LibraryImportService()
{
    if (m_watcher) {
        m_cancelRequested.store(true);
        m_watcher->disconnect(this);
        m_watcher->waitForFinished();
    }
}

bool LibraryImportService::isRunning() const noexcept
{
    return m_watcher && m_watcher->isRunning();
}

bool LibraryImportService::importFiles(const QStringList& paths)
{
    if (isRunning() || paths.isEmpty()) {
        return false;
    }
    m_cancelRequested.store(false);
    m_summary = LibraryImportSummary{};

    m_watcher = std::make_unique<QFutureWatcher<LibraryImportSummary>>();
    connect(m_watcher.get(), &QFutureWatcherBase::finished, this, &LibraryImportService::handleWatcherFinished);

    Q_EMIT started(static_cast<int>(paths.size()));
    Q_EMIT progressChanged(0, static_cast<int>(paths.size()));
    m_watcher->setFuture(QtConcurrent::run([this, paths] { return runImport(paths); }));
    return true;
}

bool LibraryImportService::cancel()
{
    if (!isRunning()) {
        return false;
    }
    m_cancelRequested.store(true);
    return true;
}

void LibraryImportService::handleWatcherFinished()
{
    m_summary = m_watcher->result();
    m_watcher.reset();
    if (m_summary.cancelled) {
        Q_EMIT cancelled();
    }
    Q_EMIT finished(m_summary);
}

LibraryImportSummary LibraryImportService::runImport(const QStringList& paths)
{
    LibraryImportSummary summary;
    summary.files.reserve(static_cast<std::size_t>(paths.size()));

    for (int index = 0; index < paths.size(); ++index) {
        // Cancellation is checked between files, so a file is never left
        // half-imported.
        if (m_cancelRequested.load()) {
            summary.cancelled = true;
            break;
        }
        Q_EMIT fileStarted(index, paths[index]);

        auto result = importOneFile(paths[index]);
        summary.patchesStored += result.patchesStored;
        if (!result.ok()) {
            ++summary.filesFailed;
        }
        Q_EMIT fileFinished(index, paths[index], result.ok(), result.describe());
        summary.files.push_back(std::move(result));
        Q_EMIT progressChanged(index + 1, static_cast<int>(paths.size()));
    }

    return summary;
}

LibraryImportFileResult LibraryImportService::importOneFile(const QString& path)
{
    LibraryImportFileResult result;
    result.path = path;
    const QFileInfo info(path);
    result.displayName = info.fileName();

    if (!info.exists() || !info.isFile()) {
        result.error = QStringLiteral("No such file.");
        return result;
    }
    if (info.size() == 0) {
        result.error = QStringLiteral("The file is empty.");
        return result;
    }
    if (info.size() > kMaximumFileBytes) {
        result.error = QStringLiteral("The file is %1 bytes, past the %2-byte limit for a SysEx dump.")
                           .arg(info.size())
                           .arg(kMaximumFileBytes);
        return result;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        result.error = file.errorString();
        return result;
    }
    const QByteArray data = file.readAll();
    if (data.size() != info.size()) {
        result.error = QStringLiteral("Only %1 of %2 bytes could be read.").arg(data.size()).arg(info.size());
        return result;
    }
    result.read = true;

    library::SyxImportOptions options;
    options.sourceName = result.displayName.toStdString();
    const auto imported = library::importSyxStream(toBytes(data), options);

    result.patchesFound = imported.entries.size();
    result.partial = imported.partial.size();
    result.rejected = imported.rejected.size();
    result.warnings = imported.warnings.size();
    result.duplicatesWithinFile = imported.duplicates.size();
    result.unattributedDataSets = imported.unattributedDataSets;
    result.streamClean = imported.stream.isClean();

    if (imported.entries.empty()) {
        // Not an error: a Performance-only dump legitimately holds no Patches.
        // The counts above say what was in the file instead.
        return result;
    }

    // Which of these the library already holds. Counted before writing, so the
    // number means "already there", not "there because we just wrote it".
    for (const auto& entry : imported.entries) {
        library::LibraryQuery query;
        query.fingerprint = entry.fingerprint();
        if (const auto existing = m_database.count(query); existing && *existing > 0) {
            ++result.duplicatesInLibrary;
        }
    }

    if (m_cancelRequested.load()) {
        result.error = QStringLiteral("Cancelled before this file was written.");
        return result;
    }

    // One transaction per file: it lands whole or not at all.
    const auto ids = m_database.insertAll(imported.entries);
    if (!ids) {
        result.error = m_database.lastError();
        return result;
    }
    result.patchesStored = ids->size();
    return result;
}

} // namespace xp60studio::services
