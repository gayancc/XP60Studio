#pragma once

#include "library/LibraryDatabase.h"
#include "library/SyxExport.h"

#include <QObject>
#include <QString>

#include <cstdint>
#include <vector>

namespace xp60studio::services {

// What one export produced.
struct LibraryExportResult
{
    QString path;
    std::size_t patchCount = 0;
    std::size_t messageCount = 0;
    std::size_t byteCount = 0;
    // Everything the export did that the caller did not literally ask for,
    // carried through from library::exportEntries: a Patch re-addressed, a
    // device ID substituted. An export that changes something says so.
    QStringList notes;
    // Ids the database could not load. The export refuses rather than writing
    // a file that silently contains fewer Patches than were asked for.
    std::vector<std::int64_t> missingIds;

    bool ok = false;
    QString error; // set exactly when ok is false

    [[nodiscard]] QString summary() const;
};

// Writes library entries to a `.syx` file.
//
// `library::exportEntries` deliberately performs no file I/O so that it stays
// deterministic and testable. This service is the other half: it loads the
// requested entries, asks that function for the bytes, and owns the write.
//
// It refuses more readily than it repairs. If any requested id cannot be
// loaded, nothing is written and the missing ids are reported — a file with
// three Patches where four were asked for is worse than no file, because
// nothing downstream would reveal the omission. If the export itself reports
// an error, that error is returned unchanged rather than being retried with
// different options behind the user's back.
//
// The file is written whole or not at all: bytes go to a temporary file beside
// the target which is renamed into place, so an interrupted write cannot leave
// a truncated `.syx` that looks importable.
class LibraryExportService : public QObject
{
    Q_OBJECT

public:
    explicit LibraryExportService(library::LibraryDatabase& database, QObject* parent = nullptr);

    [[nodiscard]] LibraryExportResult exportToFile(const std::vector<std::int64_t>& ids, const QString& path,
        const library::SyxExportOptions& options = {});

private:
    library::LibraryDatabase& m_database;
};

} // namespace xp60studio::services
