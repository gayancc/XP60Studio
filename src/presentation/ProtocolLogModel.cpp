#include "presentation/ProtocolLogModel.h"

namespace xp60studio::presentation {

ProtocolLogModel::ProtocolLogModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

void ProtocolLogModel::append(const diagnostics::ProtocolLogEntry& entry)
{
    if (m_entries.size() >= m_limit) {
        beginRemoveRows(QModelIndex(), 0, 0);
        m_entries.pop_front();
        endRemoveRows();
    }
    const int row = rowCount();
    beginInsertRows(QModelIndex(), row, row);
    m_entries.push_back(entry);
    endInsertRows();
    emit countChanged();
}

void ProtocolLogModel::clear()
{
    if (m_entries.empty()) {
        return;
    }
    beginResetModel();
    m_entries.clear();
    endResetModel();
    emit countChanged();
}

void ProtocolLogModel::setLimit(std::size_t limit)
{
    m_limit = std::max<std::size_t>(limit, 1);
    if (m_entries.size() > m_limit) {
        beginResetModel();
        while (m_entries.size() > m_limit) {
            m_entries.pop_front();
        }
        endResetModel();
        emit countChanged();
    }
}

int ProtocolLogModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_entries.size());
}

const diagnostics::ProtocolLogEntry* ProtocolLogModel::entryAt(int row) const
{
    if (row < 0 || row >= rowCount()) {
        return nullptr;
    }
    return &m_entries[static_cast<std::size_t>(row)];
}

QVariant ProtocolLogModel::data(const QModelIndex& index, int role) const
{
    const auto* entry = index.isValid() ? entryAt(index.row()) : nullptr;
    if (!entry) {
        return {};
    }
    switch (role) {
    case TimeTextRole:
        return QString::fromStdString(diagnostics::formatTimeOfDay(entry->wallTime));
    case DirectionRole:
        return QString::fromUtf8(diagnostics::logDirectionName(entry->direction).data(),
                                 static_cast<qsizetype>(diagnostics::logDirectionName(entry->direction).size()));
    case KindRole:
        return QString::fromUtf8(diagnostics::logKindName(entry->kind).data(),
                                 static_cast<qsizetype>(diagnostics::logKindName(entry->kind).size()));
    case SeverityRole:
        return QString::fromUtf8(diagnostics::logSeverityName(entry->severity).data(),
                                 static_cast<qsizetype>(diagnostics::logSeverityName(entry->severity).size()));
    case Qt::DisplayRole:
    case SummaryRole:
        return QString::fromStdString(entry->summary);
    case DetailRole:
        return QString::fromStdString(entry->detail);
    case RawHexRole:
        return QString::fromStdString(entry->rawHex);
    case ChecksumRole:
        return QString::fromUtf8(diagnostics::checksumStatusName(entry->checksum).data(),
                                 static_cast<qsizetype>(diagnostics::checksumStatusName(entry->checksum).size()));
    case RequestIdRole:
        return entry->requestId ? QVariant::fromValue(static_cast<qulonglong>(*entry->requestId)) : QVariant::fromValue(0ULL);
    case EndpointRole:
        return QString::fromStdString(entry->endpoint);
    case LineRole:
        return QString::fromStdString(diagnostics::formatLogLine(*entry));
    case IsRolandRole:
        return entry->kind == diagnostics::LogKind::RolandDataRequest || entry->kind == diagnostics::LogKind::RolandDataSet
            || entry->kind == diagnostics::LogKind::RolandInvalid;
    default:
        return {};
    }
}

QHash<int, QByteArray> ProtocolLogModel::roleNames() const
{
    return {
        {TimeTextRole, "timeText"},   {DirectionRole, "direction"}, {KindRole, "kind"},
        {SeverityRole, "severity"},   {SummaryRole, "summary"},     {DetailRole, "detail"},
        {RawHexRole, "rawHex"},       {ChecksumRole, "checksum"},   {RequestIdRole, "requestId"},
        {EndpointRole, "endpoint"},   {LineRole, "line"},           {IsRolandRole, "isRoland"},
    };
}

QString ProtocolLogModel::lineAt(int row) const
{
    const auto* entry = entryAt(row);
    return entry ? QString::fromStdString(diagnostics::formatLogLine(*entry)) : QString();
}

} // namespace xp60studio::presentation
