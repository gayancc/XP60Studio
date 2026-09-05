#include "presentation/DashboardViewModel.h"

#include "presentation/Xp60WaveCatalog.generated.h"

namespace xp60studio::presentation {

DashboardViewModel::DashboardViewModel(QObject* parent)
    : QObject(parent)
{
}

void DashboardViewModel::setDatabase(library::LibraryDatabase* database)
{
    if (m_database == database) {
        return;
    }
    m_database = database;
    refresh();
}

void DashboardViewModel::refresh()
{
    // A library that could not be opened reports nothing rather than zero: an
    // empty library and an unreachable one are different situations and the
    // card says which.
    const int previous = m_libraryPatchCount;
    m_libraryPatchCount = 0;
    if (m_database && m_database->isOpen()) {
        if (const auto total = m_database->totalCount()) {
            m_libraryPatchCount = *total;
        }
    }
    if (m_libraryPatchCount != previous) {
        emit libraryChanged();
    }
}

bool DashboardViewModel::libraryAvailable() const
{
    return m_database != nullptr && m_database->isOpen();
}

int DashboardViewModel::waveCount() const
{
    return static_cast<int>(catalog::waves.size());
}

} // namespace xp60studio::presentation
