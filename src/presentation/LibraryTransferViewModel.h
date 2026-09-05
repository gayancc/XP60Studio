#pragma once

#include "services/LibraryExportService.h"
#include "services/LibraryImportService.h"

#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <memory>

namespace xp60studio::presentation {

// Import and export, as the Library screen needs to show them.
//
// The services below already do the work and decide what is stored; this type
// exists so QML never touches a service, a file path or a Roland option
// struct. It holds three things the UI cannot derive for itself: whether an
// operation is running, how far it has got, and what the last one actually
// did.
//
// The last result is deliberately kept until the next operation replaces it,
// not cleared on a timer. Importing a bank can report duplicates, partial
// Patches and rejected messages, and a librarian needs to read that at their
// own pace rather than catch it in a toast.
class LibraryTransferViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(QString activity READ activity NOTIFY stateChanged)

    // Progress is per file, because a file is the unit that either lands or
    // does not. -1 for both when nothing is running.
    Q_PROPERTY(int filesCompleted READ filesCompleted NOTIFY progressChanged)
    Q_PROPERTY(int filesTotal READ filesTotal NOTIFY progressChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString currentFile READ currentFile NOTIFY progressChanged)

    // The last completed operation. `resultTone` maps to StatusPill tones so
    // the screen does not have to decide what a partial import looks like.
    Q_PROPERTY(bool hasResult READ hasResult NOTIFY resultChanged)
    Q_PROPERTY(QString resultHeadline READ resultHeadline NOTIFY resultChanged)
    Q_PROPERTY(QString resultDetail READ resultDetail NOTIFY resultChanged)
    Q_PROPERTY(QString resultTone READ resultTone NOTIFY resultChanged)
    // One entry per file: {name, ok, description, stored, duplicates, warnings}.
    Q_PROPERTY(QVariantList resultFiles READ resultFiles NOTIFY resultChanged)
    // True when the last import found Patches already in the library. Never
    // acted on -- reported so the user decides what a duplicate means.
    Q_PROPERTY(bool hasDuplicates READ hasDuplicates NOTIFY resultChanged)

public:
    LibraryTransferViewModel(services::LibraryImportService& importService,
        services::LibraryExportService& exportService, QObject* parent = nullptr);

    [[nodiscard]] bool busy() const;
    [[nodiscard]] QString activity() const { return m_activity; }
    [[nodiscard]] int filesCompleted() const { return m_completed; }
    [[nodiscard]] int filesTotal() const { return m_total; }
    [[nodiscard]] double progress() const;
    [[nodiscard]] QString currentFile() const { return m_currentFile; }

    [[nodiscard]] bool hasResult() const { return m_hasResult; }
    [[nodiscard]] QString resultHeadline() const { return m_resultHeadline; }
    [[nodiscard]] QString resultDetail() const { return m_resultDetail; }
    [[nodiscard]] QString resultTone() const { return m_resultTone; }
    [[nodiscard]] QVariantList resultFiles() const { return m_resultFiles; }
    [[nodiscard]] bool hasDuplicates() const { return m_hasDuplicates; }

    // Local file URLs from a FileDialog. Returns false when an import is
    // already running or nothing usable was given.
    Q_INVOKABLE bool importFiles(const QList<QUrl>& urls);
    Q_INVOKABLE bool cancelImport();

    // Writes `ids` to `url`. `userBankFrom` >= 1 re-addresses the Patches to
    // consecutive User slots from that number and re-encodes them; 0 keeps
    // every Patch at the address it came from and writes the original bytes.
    // Returns false and reports why through the result properties.
    Q_INVOKABLE bool exportIds(const QVariantList& ids, const QUrl& url, int userBankFrom = 0);

    // Writes a built bank to `url` as a `.syx`.
    //
    // `arrangement` is what BankBuilderViewModel::arrangementIds() returns:
    // 128 library ids in destination order, an empty destination as 0. Each
    // Patch is addressed to the User slot it actually occupies in the bank,
    // which is why this cannot be exportIds() with a starting number — a bank
    // has holes in it, and closing them would silently move a musician's
    // Patches to destinations they did not choose.
    //
    // An empty destination writes nothing at all rather than a blank Patch:
    // this application does not decide that a gap means "erase whatever is on
    // the instrument there". Returns false and reports why through the result
    // properties.
    Q_INVOKABLE bool exportBankArrangement(const QVariantList& arrangement, const QUrl& url);

    // Clears the last result. For "I have read this", not for hiding failures
    // automatically.
    Q_INVOKABLE void dismissResult();

Q_SIGNALS:
    void stateChanged();
    void progressChanged();
    void resultChanged();
    // Something was written into the library, so the list should re-read. Not
    // emitted for an export, which changes nothing.
    void libraryChanged();

private:
    void setActivity(const QString& activity);
    void setProgress(int completed, int total, const QString& currentFile);
    void publishImportResult(const services::LibraryImportSummary& summary);
    void publishExportResult(const services::LibraryExportResult& result);
    void clearProgress();

    services::LibraryImportService& m_import;
    services::LibraryExportService& m_export;

    QString m_activity;
    int m_completed = -1;
    int m_total = -1;
    QString m_currentFile;

    bool m_hasResult = false;
    QString m_resultHeadline;
    QString m_resultDetail;
    QString m_resultTone;
    QVariantList m_resultFiles;
    bool m_hasDuplicates = false;
};

} // namespace xp60studio::presentation
