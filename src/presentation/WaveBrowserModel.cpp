#include "presentation/WaveBrowserModel.h"
#include "presentation/Xp60WaveCatalog.generated.h"
#include <algorithm>
#include <QStringList>

namespace xp60studio::presentation {
namespace {
QString key(const catalog::Wave& wave)
{
    return QStringLiteral("%1 %2").arg(QString::fromLatin1(wave.bank))
        .arg(wave.number, 3, 10, QLatin1Char('0'));
}
}
WaveBrowserModel::WaveBrowserModel(QObject* parent) : QAbstractListModel(parent) { rebuild(); }
int WaveBrowserModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}
QVariant WaveBrowserModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.model() != this || index.column() != 0 || index.row() < 0 || index.row() >= rowCount()) return {};
    const auto& wave = catalog::waves[static_cast<std::size_t>(m_rows[static_cast<std::size_t>(index.row())])];
    switch (role) {
    case Qt::DisplayRole:
    case NameRole: return QString::fromUtf8(wave.name);
    case BankRole: return QString::fromLatin1(wave.bank);
    case NumberRole: return wave.number;
    case KeyRole: return key(wave);
    default: return {};
    }
}
QHash<int, QByteArray> WaveBrowserModel::roleNames() const
{
    return {{NameRole, "waveName"}, {BankRole, "bank"}, {NumberRole, "number"}, {KeyRole, "waveKey"}};
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
void WaveBrowserModel::rebuild()
{
    beginResetModel();
    m_rows.clear();
    const auto terms = m_query.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (std::size_t i = 0; i < catalog::waves.size(); ++i) {
        const auto& wave = catalog::waves[i];
        const auto bank = QString::fromLatin1(wave.bank);
        if (m_source == 3 || (m_source == 1 && bank != "INT-A") || (m_source == 2 && bank != "INT-B")) continue;
        const auto text = key(wave) + QLatin1Char(' ') + QString::fromUtf8(wave.name);
        if (std::all_of(terms.begin(), terms.end(), [&text](const QString& term) { return text.contains(term, Qt::CaseInsensitive); }))
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
    const auto& wave = catalog::waves[static_cast<std::size_t>(m_selected)];
    return {{"name", QString::fromUtf8(wave.name)}, {"key", key(wave)},
            {"bank", QString::fromLatin1(wave.bank)}, {"page", wave.page}};
}
} // namespace xp60studio::presentation
