#pragma once

#include "midi/MidiTypes.h"

#include <QAbstractListModel>
#include <QString>

#include <vector>

namespace xp60studio::presentation {

// Read-only list of MIDI endpoints for the endpoint pickers.
class MidiEndpointListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        EndpointIdRole = Qt::UserRole + 1,
        DisplayNameRole,
        BackendNameRole,
        IsVirtualRole,
    };

    explicit MidiEndpointListModel(QObject* parent = nullptr);

    void setEndpoints(std::vector<midi::MidiEndpointInfo> endpoints);
    [[nodiscard]] const std::vector<midi::MidiEndpointInfo>& endpoints() const noexcept { return m_endpoints; }

    [[nodiscard]] int count() const { return rowCount(); }
    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QString endpointIdAt(int row) const;
    Q_INVOKABLE QString displayNameAt(int row) const;
    Q_INVOKABLE int indexOfId(const QString& endpointId) const;

signals:
    void countChanged();

private:
    std::vector<midi::MidiEndpointInfo> m_endpoints;
};

} // namespace xp60studio::presentation
