#pragma once

#include "library/LibraryDatabase.h"
#include "library/SyxImport.h"

#include <QFuture>
#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QStringList>

#include <atomic>
#include <memory>
#include <vector>

namespace xp60studio::services {

// What one file contributed to an import.
struct LibraryImportFileResult
{
    QString path;
    QString displayName;   // the file name, as recorded in provenance
    bool read = false;     // whether the bytes could be read at all
    QString error;         // set when `read` is false, or the database refused

    std::size_t patchesFound = 0;
    std::size_t patchesStored = 0;
    std::size_t partial = 0;
    std::size_t rejected = 0;
    std::size_t warnings = 0;
    std::size_t duplicatesWithinFile = 0;
    // Entries already in the library whose parameters hash the same. Reported,
    // never acted on: the user decides what a duplicate means.
    std::size_t duplicatesInLibrary = 0;
    std::size_t unattributedDataSets = 0;
    bool streamClean = true;

    [[nodiscard]] bool ok() const noexcept { return read && error.isEmpty(); }
    [[nodiscard]] QString describe() const;
};

struct LibraryImportSummary
{
    std::vector<LibraryImportFileResult> files;
    std::size_t patchesStored = 0;
    std::size_t filesFailed = 0;
    bool cancelled = false;

    [[nodiscard]] bool anythingStored() const noexcept { return patchesStored > 0; }
    // "3 files, 384 patches stored, 1 file failed".
    [[nodiscard]] QString summary() const;
};

// Imports `.syx` files into the library as one observable operation.
//
// Importing years of old SysEx banks is the long-running job this application
// has to get right: it reads files, decodes thousands of Patches and writes
// them, and none of that may block the QML scene (ARCHITECTURE.md §17). The
// service therefore owns what a service owns — file I/O, the worker thread,
// progress and cancellation — while the decisions stay in the layers below:
// `library::importSyxStream` decides what a file contains and
// `library::LibraryDatabase` decides what is stored.
//
// Each file is imported in its own database transaction. A file either lands
// completely or not at all; a failure part-way through a batch leaves the
// earlier files imported and says which one stopped it, rather than discarding
// work the user already waited for.
//
// Cancellation is honoured between files and before each file's write, so a
// cancelled import never leaves a half-written file behind. Work already
// committed stays committed and is reported.
//
// Nothing here deduplicates. Patches whose parameters already exist in the
// library are counted and reported so the user can decide; silently dropping
// an import is the data loss AGENTS.md forbids.
class LibraryImportService : public QObject
{
    Q_OBJECT

public:
    // The database must outlive the service and must not be used by anyone
    // else while an import is running.
    explicit LibraryImportService(library::LibraryDatabase& database, QObject* parent = nullptr);
    ~LibraryImportService() override;

    [[nodiscard]] bool isRunning() const noexcept;
    [[nodiscard]] const LibraryImportSummary& lastSummary() const noexcept { return m_summary; }

    // Starts importing `paths`. Returns false if an import is already running
    // or the list is empty; `started` is emitted on success.
    bool importFiles(const QStringList& paths);
    // Asks the running import to stop at the next safe point. Returns false if
    // nothing is running.
    bool cancel();

    // Largest file the service will read, as a guard against being handed
    // something that is not a SysEx dump at all. A real XP-60 whole-memory
    // dump is well under this.
    static constexpr qint64 kMaximumFileBytes = 64LL * 1024 * 1024;

Q_SIGNALS:
    void started(int fileCount);
    // `fileIndex` is zero-based; `path` is the file about to be read.
    void fileStarted(int fileIndex, const QString& path);
    void fileFinished(int fileIndex, const QString& path, bool ok, const QString& description);
    // Files completed out of the total. Progress is per file, because a file
    // is the unit that either lands or does not.
    void progressChanged(int completed, int total);
    void finished(const LibraryImportSummary& summary);
    void cancelled();

private:
    void handleWatcherFinished();
    [[nodiscard]] LibraryImportSummary runImport(const QStringList& paths);
    [[nodiscard]] LibraryImportFileResult importOneFile(const QString& path);

    library::LibraryDatabase& m_database;
    std::unique_ptr<QFutureWatcher<LibraryImportSummary>> m_watcher;
    std::atomic<bool> m_cancelRequested{false};
    LibraryImportSummary m_summary;
};

} // namespace xp60studio::services
