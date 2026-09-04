#include "presentation/MidiEndpointListModel.h"

namespace xp60studio::presentation {

MidiEndpointListModel::MidiEndpointListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

void MidiEndpointListModel::setEndpoints(std::vector<midi::MidiEndpointInfo> endpoints)
{
    if (endpoints == m_endpoints) {
        return;
    }
    beginResetModel();
    m_endpoints = std::move(endpoints);
    endResetModel();
    emit countChanged();
}

int MidiEndpointListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_endpoints.size());
}

QVariant MidiEndpointListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }
    const auto& endpoint = m_endpoints[static_cast<std::size_t>(index.row())];
    switch (role) {
    case EndpointIdRole:
        return QString::fromStdString(endpoint.id);
    case Qt::DisplayRole:
    case DisplayNameRole:
        return QString::fromStdString(endpoint.displayName);
    case BackendNameRole:
        return QString::fromStdString(endpoint.backendName);
    case IsVirtualRole:
        return endpoint.isVirtual;
    default:
        return {};
    }
}

QHash<int, QByteArray> MidiEndpointListModel::roleNames() const
{
    return {
        {EndpointIdRole, "endpointId"},
        {DisplayNameRole, "displayName"},
        {BackendNameRole, "backendName"},
        {IsVirtualRole, "isVirtual"},
    };
}

QString MidiEndpointListModel::endpointIdAt(int row) const
{
    if (row < 0 || row >= rowCount()) {
        return {};
    }
    return QString::fromStdString(m_endpoints[static_cast<std::size_t>(row)].id);
}

QString MidiEndpointListModel::displayNameAt(int row) const
{
    if (row < 0 || row >= rowCount()) {
        return {};
    }
    return QString::fromStdString(m_endpoints[static_cast<std::size_t>(row)].displayName);
}

int MidiEndpointListModel::indexOfId(const QString& endpointId) const
{
    const std::string id = endpointId.toStdString();
    for (std::size_t i = 0; i < m_endpoints.size(); ++i) {
        if (m_endpoints[i].id == id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

} // namespace xp60studio::presentation
