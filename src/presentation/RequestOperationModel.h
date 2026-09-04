#pragma once

#include "protocol/RolandRequestTracker.h"

#include <QAbstractListModel>

#include <vector>

namespace xp60studio::presentation {

// Snapshot of request operations, newest first, for the Operations panel.
class RequestOperationModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int outstandingCount READ outstandingCount NOTIFY countChanged)

public:
    enum Roles {
        RequestIdRole = Qt::UserRole + 1,
        StateNameRole,
        StateLabelRole,
        IsTerminalRole,
        IsSuccessRole,
        AddressHexRole,
        SizeHexRole,
        ExpectedBytesRole,
        ReceivedBytesRole,
        ChunkCountRole,
        ProgressRole,
        FailureReasonRole,
        NotesRole,
        DataHexRole,
        DataTextRole,
    };

    explicit RequestOperationModel(QObject* parent = nullptr);

    void refresh(const protocol::RolandRequestTracker& tracker);
    void clear();

    [[nodiscard]] int count() const { return rowCount(); }
    [[nodiscard]] int outstandingCount() const;
    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

signals:
    void countChanged();

private:
    std::vector<protocol::RequestOperation> m_operations; // newest first
};

} // namespace xp60studio::presentation
