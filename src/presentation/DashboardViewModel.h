#pragma once

#include "library/LibraryDatabase.h"

#include <QObject>
#include <QString>
#include <QVariantList>

namespace xp60studio::presentation {

// Summaries for the Dashboard / Command Center (mockup panel M1).
//
// The Dashboard reads from everywhere and owns nothing. It exists so the screen
// can ask one object "how much is in the library, is a bank feature here yet,
// what is the device doing" without reaching into the Library screen's own
// models — which keeps this independent of how the Library screen is built.
//
// Two rules shape it, both from the roadmap.
//
// It reports what exists and nothing else. The mockup shows an expansion count,
// bank occupancy and patch category tags; none of those are knowable yet.
// Which expansion boards are fitted is unestablished (DEVICE_ACCEPTANCE.md area
// 9 found EXP references whose boards are unknown), banks are a later phase, and
// the XP-60 Parameter Address Map defines no category byte. Rather than invent
// a number, each says plainly that it is not known yet.
//
// It offers no action that does not work. A quick action that navigates
// somewhere unbuilt would be a promise the application cannot keep, so the
// availability of each is a property the screen must honour.
class DashboardViewModel : public QObject
{
    Q_OBJECT

    // Library --------------------------------------------------------------
    Q_PROPERTY(int libraryPatchCount READ libraryPatchCount NOTIFY libraryChanged)
    Q_PROPERTY(bool libraryAvailable READ libraryAvailable NOTIFY libraryChanged)
    // The catalog the wave browser searches. A real count, not a guess.
    Q_PROPERTY(int waveCount READ waveCount CONSTANT)
    // Which expansion boards are fitted is not established, so the card says so
    // instead of showing a number.
    Q_PROPERTY(bool expansionCountKnown READ expansionCountKnown CONSTANT)

    // Banks ----------------------------------------------------------------
    // Phase 6 is implemented. The card is an entry point, while bank-specific
    // counts remain owned by the Bank Builder itself.
    Q_PROPERTY(bool banksAvailable READ banksAvailable CONSTANT)

public:
    explicit DashboardViewModel(QObject* parent = nullptr);

    void setDatabase(library::LibraryDatabase* database);
    // Re-reads the counts. The Dashboard is not live-bound to the library: it
    // refreshes when shown, which is enough for a summary and costs nothing
    // while the user is elsewhere.
    Q_INVOKABLE void refresh();

    [[nodiscard]] int libraryPatchCount() const { return m_libraryPatchCount; }
    [[nodiscard]] bool libraryAvailable() const;
    [[nodiscard]] int waveCount() const;
    [[nodiscard]] bool expansionCountKnown() const { return false; }
    [[nodiscard]] bool banksAvailable() const { return true; }

Q_SIGNALS:
    void libraryChanged();

private:
    library::LibraryDatabase* m_database = nullptr;
    int m_libraryPatchCount = 0;
};

} // namespace xp60studio::presentation
