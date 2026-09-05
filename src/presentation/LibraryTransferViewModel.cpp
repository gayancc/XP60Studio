#include "presentation/LibraryTransferViewModel.h"

#include <QFileInfo>

namespace xp60studio::presentation {

namespace {

// "3 files, 384 patches stored" reads better than a bare count, and the plural
// has to be right in both halves or the line looks machine-written.
QString countOf(std::size_t n, const QString& singular, const QString& plural)
{
    return QStringLiteral("%1 %2").arg(n).arg(n == 1 ? singular : plural);
}

} // namespace

LibraryTransferViewModel::LibraryTransferViewModel(services::LibraryImportService& importService,
    services::LibraryExportService& exportService, QObject* parent)
    : QObject(parent)
    , m_import(importService)
    , m_export(exportService)
{
    connect(&m_import, &services::LibraryImportService::started, this, [this](int fileCount) {
        setActivity(tr("Importing"));
        setProgress(0, fileCount, QString());
    });
    connect(&m_import, &services::LibraryImportService::fileStarted, this,
        [this](int index, const QString& path) { setProgress(index, m_total, QFileInfo(path).fileName()); });
    connect(&m_import, &services::LibraryImportService::progressChanged, this,
        [this](int completed, int total) { setProgress(completed, total, m_currentFile); });
    connect(&m_import, &services::LibraryImportService::finished, this,
        [this](const services::LibraryImportSummary& summary) {
            clearProgress();
            setActivity(QString());
            publishImportResult(summary);
            if (summary.anythingStored()) {
                emit libraryChanged();
            }
        });
}

bool LibraryTransferViewModel::busy() const
{
    return m_import.isRunning();
}

double LibraryTransferViewModel::progress() const
{
    if (m_total <= 0 || m_completed < 0) {
        return 0.0;
    }
    return static_cast<double>(m_completed) / static_cast<double>(m_total);
}

void LibraryTransferViewModel::setActivity(const QString& activity)
{
    if (m_activity == activity) {
        return;
    }
    m_activity = activity;
    emit stateChanged();
}

void LibraryTransferViewModel::setProgress(int completed, int total, const QString& currentFile)
{
    if (m_completed == completed && m_total == total && m_currentFile == currentFile) {
        return;
    }
    m_completed = completed;
    m_total = total;
    m_currentFile = currentFile;
    emit progressChanged();
}

void LibraryTransferViewModel::clearProgress()
{
    setProgress(-1, -1, QString());
}

bool LibraryTransferViewModel::importFiles(const QList<QUrl>& urls)
{
    if (busy()) {
        return false;
    }
    QStringList paths;
    paths.reserve(urls.size());
    for (const auto& url : urls) {
        // Only local files can be read. A remote URL is refused rather than
        // silently skipped, so the count the user sees matches what was tried.
        if (!url.isLocalFile()) {
            m_hasResult = true;
            m_resultHeadline = tr("Import refused");
            m_resultDetail = tr("%1 is not a local file.").arg(url.toString());
            m_resultTone = QStringLiteral("error");
            m_resultFiles.clear();
            m_hasDuplicates = false;
            emit resultChanged();
            return false;
        }
        paths.append(url.toLocalFile());
    }
    if (paths.isEmpty()) {
        return false;
    }
    return m_import.importFiles(paths);
}

bool LibraryTransferViewModel::cancelImport()
{
    return m_import.cancel();
}

void LibraryTransferViewModel::publishImportResult(const services::LibraryImportSummary& summary)
{
    m_resultFiles.clear();
    std::size_t duplicates = 0;
    std::size_t partial = 0;
    std::size_t rejected = 0;
    for (const auto& file : summary.files) {
        duplicates += file.duplicatesInLibrary;
        partial += file.partial;
        rejected += file.rejected;
        QVariantMap entry;
        entry.insert(QStringLiteral("name"), file.displayName);
        entry.insert(QStringLiteral("ok"), file.ok());
        entry.insert(QStringLiteral("description"), file.describe());
        entry.insert(QStringLiteral("stored"), static_cast<qulonglong>(file.patchesStored));
        entry.insert(QStringLiteral("duplicates"), static_cast<qulonglong>(file.duplicatesInLibrary));
        entry.insert(QStringLiteral("partial"), static_cast<qulonglong>(file.partial));
        entry.insert(QStringLiteral("rejected"), static_cast<qulonglong>(file.rejected));
        entry.insert(QStringLiteral("error"), file.error);
        m_resultFiles.append(entry);
    }
    m_hasDuplicates = duplicates > 0;

    if (summary.cancelled) {
        m_resultHeadline = tr("Import cancelled");
        m_resultTone = QStringLiteral("warning");
    } else if (summary.filesFailed > 0 && !summary.anythingStored()) {
        m_resultHeadline = tr("Import failed");
        m_resultTone = QStringLiteral("error");
    } else if (summary.filesFailed > 0) {
        m_resultHeadline = tr("Imported with problems");
        m_resultTone = QStringLiteral("warning");
    } else {
        m_resultHeadline = tr("Import complete");
        m_resultTone = QStringLiteral("success");
    }

    // Work already committed stays committed even when a later file failed, so
    // the detail always leads with what was stored rather than with the error.
    QStringList parts;
    parts << countOf(summary.patchesStored, tr("patch stored"), tr("patches stored"));
    if (summary.filesFailed > 0) {
        parts << countOf(summary.filesFailed, tr("file failed"), tr("files failed"));
    }
    if (duplicates > 0) {
        // Counted, never acted on. Say so here so nobody reads it as "skipped".
        parts << tr("%1 already in the library, kept").arg(duplicates);
    }
    if (partial > 0) {
        parts << countOf(partial, tr("partial patch"), tr("partial patches"));
    }
    if (rejected > 0) {
        parts << countOf(rejected, tr("rejected message"), tr("rejected messages"));
    }
    m_resultDetail = parts.join(QStringLiteral(" · "));
    m_hasResult = true;
    emit resultChanged();
}

bool LibraryTransferViewModel::exportIds(const QVariantList& ids, const QUrl& url, int userBankFrom)
{
    if (busy()) {
        return false;
    }
    std::vector<std::int64_t> selected;
    selected.reserve(static_cast<std::size_t>(ids.size()));
    for (const auto& id : ids) {
        selected.push_back(static_cast<std::int64_t>(id.toLongLong()));
    }

    library::SyxExportOptions options;
    if (userBankFrom >= 1) {
        // Moving a Patch means its original bytes no longer describe where it
        // lives, so the export must re-encode; asking for original bytes here
        // would be refused, and rightly.
        options.source = library::SyxExportSource::ReencodedFromModel;
        options.target.kind = library::SyxExportTarget::Kind::UserBankFrom;
        options.target.firstUserNumber = userBankFrom;
    }

    const auto path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    const auto result = m_export.exportToFile(selected, path, options);
    publishExportResult(result);
    return result.ok;
}

void LibraryTransferViewModel::publishExportResult(const services::LibraryExportResult& result)
{
    m_resultFiles.clear();
    m_hasDuplicates = false;
    m_hasResult = true;

    if (!result.ok) {
        m_resultHeadline = tr("Export failed");
        m_resultTone = QStringLiteral("error");
        m_resultDetail = result.error;
        emit resultChanged();
        return;
    }

    m_resultHeadline = tr("Export complete");
    // A note means the export did something the user did not literally ask
    // for, so it is a warning rather than a clean success.
    m_resultTone = result.notes.isEmpty() ? QStringLiteral("success") : QStringLiteral("warning");

    QStringList parts;
    parts << countOf(result.patchCount, tr("patch"), tr("patches"));
    parts << QFileInfo(result.path).fileName();
    m_resultDetail = parts.join(QStringLiteral(" · "));

    for (const auto& note : result.notes) {
        QVariantMap entry;
        entry.insert(QStringLiteral("name"), tr("Note"));
        entry.insert(QStringLiteral("ok"), true);
        entry.insert(QStringLiteral("description"), note);
        m_resultFiles.append(entry);
    }
    emit resultChanged();
}

void LibraryTransferViewModel::dismissResult()
{
    if (!m_hasResult) {
        return;
    }
    m_hasResult = false;
    m_resultHeadline.clear();
    m_resultDetail.clear();
    m_resultTone.clear();
    m_resultFiles.clear();
    m_hasDuplicates = false;
    emit resultChanged();
}

} // namespace xp60studio::presentation
