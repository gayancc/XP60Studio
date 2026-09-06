#pragma once

#include "library/InstrumentSnapshot.h"

#include <QString>
#include <QStringList>

#include <optional>
#include <vector>

namespace xp60studio::services {

struct SnapshotSaveResult
{
    QString sysExPath;     // the .syx
    QString manifestPath;  // the .json beside it
    std::string digest;
    std::size_t byteCount = 0;
    bool ok = false;
    QString error;  // set exactly when ok is false
};

struct SnapshotLoadResult
{
    std::optional<library::InstrumentSnapshot> snapshot;
    // The digest the manifest recorded, and the one the bytes actually hash to.
    // They differ only when the file changed after it was written.
    std::string expectedDigest;
    std::string actualDigest;
    bool digestMatched = false;
    bool manifestFound = false;
    bool ok = false;
    QString error;

    // Everything about this snapshot the caller should show before restoring
    // from it: a missing manifest, a digest that no longer matches, messages
    // that could not be read.
    QStringList warnings;
};

// Saves and loads instrument snapshots on disk.
//
// A snapshot is two files that share a stem:
//
//   `<name>.syx`   the captured Data Sets, verbatim and in order
//   `<name>.json`  when it was taken, from what, what the user called it,
//                  and the SHA-256 of the `.syx`
//
// Two files rather than one container, deliberately. The `.syx` is the backup,
// and it is a plain, ordinary `.syx` — restorable by any other librarian, or by
// `amidi`, or by a version of this application that no longer exists. A backup
// readable only by the program that wrote it is a worse backup. The manifest
// adds what SysEx cannot carry and is *not* required to restore: losing it
// costs provenance and the integrity check, not the data.
//
// The digest is checked on load and reported, never enforced silently. A
// snapshot whose bytes no longer match its manifest may still be exactly what
// the user needs; what must not happen is restoring it without saying so.
//
// Writes go through a temporary file and a rename, so an interrupted save
// cannot leave a truncated `.syx` that looks like a complete backup.
class SnapshotStore
{
public:
    // `path` is the `.syx` path; the manifest is written beside it with the
    // same stem. Refuses to overwrite unless `overwrite` is set — a snapshot is
    // the thing standing between the user and lost data.
    [[nodiscard]] static SnapshotSaveResult save(const library::InstrumentSnapshot& snapshot,
                                                 const QString& path, bool overwrite = false);

    [[nodiscard]] static SnapshotLoadResult load(const QString& path);

    // The `.syx` files in a directory that have a snapshot manifest beside
    // them, newest capture first. A `.syx` without a manifest is a `.syx`, not
    // a snapshot, and is not listed as one.
    [[nodiscard]] static std::vector<QString> list(const QString& directory);
};

} // namespace xp60studio::services
