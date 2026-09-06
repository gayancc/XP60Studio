#include "presentation/WaveBrowserModel.h"
#include "presentation/Xp60WaveCatalog.generated.h"

#include "library/ExpansionBoardCatalog.h"

#include <QStringList>
#include <algorithm>

namespace xp60studio::presentation {
namespace {

QString internalKey(const catalog::Wave& wave)
{
    return QStringLiteral("%1 %2").arg(QString::fromLatin1(wave.bank))
        .arg(wave.number, 3, 10, QLatin1Char('0'));
}

QString toQt(std::string_view text)
{
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

} // namespace

WaveBrowserModel::WaveBrowserModel(QObject* parent) : QAbstractListModel(parent) { rebuildAll(); }

int WaveBrowserModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QString WaveBrowserModel::nameOf(const Entry& entry) const
{
    if (!entry.expansion) {
        return QString::fromUtf8(catalog::waves[static_cast<std::size_t>(entry.catalogIndex)].name);
    }
    const auto name = library::srJv80WaveName(entry.board, entry.displayNumber);
    return name ? toQt(*name) : QString();
}

QString WaveBrowserModel::bankOf(const Entry& entry) const
{
    if (!entry.expansion) {
        return QString::fromLatin1(catalog::waves[static_cast<std::size_t>(entry.catalogIndex)].bank);
    }
    // The slot the board sits in is what the instrument's own display shows, so
    // that is what the row is labelled with.
    if (m_profile) {
        if (const auto slot = m_profile->slotProviding(entry.board)) {
            return toQt(library::slotLabel(*slot));
        }
    }
    return QStringLiteral("EXP");
}

int WaveBrowserModel::numberOf(const Entry& entry) const
{
    return entry.expansion ? entry.displayNumber
                           : catalog::waves[static_cast<std::size_t>(entry.catalogIndex)].number;
}

QString WaveBrowserModel::keyOf(const Entry& entry) const
{
    if (!entry.expansion) {
        return internalKey(catalog::waves[static_cast<std::size_t>(entry.catalogIndex)]);
    }
    return QStringLiteral("%1 %2").arg(bankOf(entry)).arg(entry.displayNumber, 3, 10, QLatin1Char('0'));
}

QVariant WaveBrowserModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.model() != this || index.column() != 0 || index.row() < 0 || index.row() >= rowCount()) return {};
    const auto& entry = m_all[static_cast<std::size_t>(m_rows[static_cast<std::size_t>(index.row())])];
    switch (role) {
    case Qt::DisplayRole:
    case NameRole: return nameOf(entry);
    case BankRole: return bankOf(entry);
    case NumberRole: return numberOf(entry);
    case KeyRole: return keyOf(entry);
    case PageRole:
        return entry.expansion ? 0 : catalog::waves[static_cast<std::size_t>(entry.catalogIndex)].page;
    // Every wave the browser lists is one this instrument can play: internal
    // waves always, expansion waves only from boards the musician declared.
    case AvailabilityRole: return QStringLiteral("available");
    case ExpansionRole: return entry.expansion;
    case WaveGroupRole: return entry.expansion ? entry.board : -1;
    case BoardRole: {
        if (!entry.expansion) return QString();
        const auto board = library::srJv80BoardName(entry.board);
        return board ? QString::fromStdString(*board) : QString();
    }
    default: return {};
    }
}

QHash<int, QByteArray> WaveBrowserModel::roleNames() const
{
    return {{NameRole, "waveName"}, {BankRole, "bank"}, {NumberRole, "number"}, {KeyRole, "waveKey"},
            {PageRole, "page"}, {AvailabilityRole, "availability"}, {ExpansionRole, "expansion"},
            {WaveGroupRole, "waveGroupId"}, {BoardRole, "boardName"}};
}

void WaveBrowserModel::setQuery(const QString& query)
{
    if (m_query == query) return;
    m_query = query;
    rebuild();
}

void WaveBrowserModel::setSourceFilter(int source)
{
    if (source < 0 || source > 3 || source == m_source) return;
    m_source = source;
    rebuild();
}

void WaveBrowserModel::setExpansionProfile(const library::ExpansionProfile* profile)
{
    if (m_profile == profile) return;
    m_profile = profile;
    expansionProfileChanged();
}

void WaveBrowserModel::expansionProfileChanged()
{
    rebuildAll();
}

int WaveBrowserModel::expansionCount() const
{
    return static_cast<int>(std::count_if(m_all.begin(), m_all.end(), [](const Entry& e) { return e.expansion; }));
}

// The whole browsable set: internal waves, then every wave of every declared
// board this project holds Roland's list for. Rebuilt rather than patched
// because a profile change can add or remove a whole board's worth of rows.
void WaveBrowserModel::rebuildAll()
{
    // Remember the selection as a value, not an index: the indices move.
    const auto previous = m_selected >= 0 ? std::optional<Entry>{m_all[static_cast<std::size_t>(m_selected)]}
                                          : std::nullopt;

    m_all.clear();
    for (std::size_t i = 0; i < catalog::waves.size(); ++i) {
        m_all.push_back(Entry{false, static_cast<int>(i), 0, 0});
    }
    if (m_profile) {
        for (const auto& board : m_profile->boards()) {
            if (board.name.empty() || !board.waveGroupId) {
                continue;
            }
            const int count = library::srJv80WaveCount(*board.waveGroupId);
            for (int n = 1; n <= count; ++n) {
                m_all.push_back(Entry{true, -1, *board.waveGroupId, n});
            }
        }
    }

    m_selected = -1;
    if (previous) {
        const auto found = std::find_if(m_all.begin(), m_all.end(), [&previous](const Entry& e) {
            return e.expansion == previous->expansion && e.catalogIndex == previous->catalogIndex
                && e.board == previous->board && e.displayNumber == previous->displayNumber;
        });
        if (found != m_all.end()) {
            m_selected = static_cast<int>(found - m_all.begin());
        }
    }
    rebuild();
}

void WaveBrowserModel::rebuild()
{
    beginResetModel();
    m_rows.clear();
    const auto terms = m_query.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (std::size_t i = 0; i < m_all.size(); ++i) {
        const auto& entry = m_all[i];
        if (m_source == 3 && !entry.expansion) continue;
        if (m_source != 3 && m_source != 0 && entry.expansion) continue;
        if (!entry.expansion) {
            const auto bank = bankOf(entry);
            if ((m_source == 1 && bank != "INT-A") || (m_source == 2 && bank != "INT-B")) continue;
        }
        // Searching matches the board name too, so "asia" finds that board's
        // waves without the musician knowing its group number.
        const auto board = entry.expansion ? QString::fromStdString(
                               library::srJv80BoardName(entry.board).value_or(std::string{}))
                                           : QString();
        const auto haystack = keyOf(entry) + QLatin1Char(' ') + nameOf(entry) + QLatin1Char(' ') + board;
        if (std::all_of(terms.begin(), terms.end(), [&haystack](const QString& term) { return haystack.contains(term, Qt::CaseInsensitive); }))
            m_rows.push_back(static_cast<int>(i));
    }
    if (std::find(m_rows.begin(), m_rows.end(), m_selected) == m_rows.end()) m_selected = -1;
    endResetModel();
    emit filterChanged();
    emit selectionChanged();
}

void WaveBrowserModel::selectRow(int row)
{
    if (row < 0 || row >= rowCount()) return;
    const int selected = m_rows[static_cast<std::size_t>(row)];
    if (m_selected == selected) return;
    m_selected = selected;
    emit selectionChanged();
}

int WaveBrowserModel::selectedRow() const
{
    const auto found = std::find(m_rows.begin(), m_rows.end(), m_selected);
    return found == m_rows.end() ? -1 : static_cast<int>(found - m_rows.begin());
}

QVariantMap WaveBrowserModel::selected() const
{
    if (m_selected < 0) return {};
    const auto& entry = m_all[static_cast<std::size_t>(m_selected)];
    // `number` is the 1-based number the XP-60 displays. Turning it into the
    // Tone's raw bytes is xpmodel::encodeWave's job, not this model's: the
    // catalog is display metadata and must not carry SysEx knowledge. The one
    // thing carried through is which board an expansion wave came from, because
    // without it the selection is not a wave at all.
    QVariantMap map{{"name", nameOf(entry)}, {"key", keyOf(entry)},
                    {"bank", bankOf(entry)}, {"number", numberOf(entry)},
                    {"expansion", entry.expansion}};
    map.insert(QStringLiteral("page"),
               entry.expansion ? 0 : catalog::waves[static_cast<std::size_t>(entry.catalogIndex)].page);
    map.insert(QStringLiteral("waveGroupId"), entry.expansion ? entry.board : -1);
    if (entry.expansion) {
        const auto board = library::srJv80BoardName(entry.board);
        map.insert(QStringLiteral("boardName"), board ? QString::fromStdString(*board) : QString());
    } else {
        map.insert(QStringLiteral("boardName"), QString());
    }
    return map;
}
} // namespace xp60studio::presentation
