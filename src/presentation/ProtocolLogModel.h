#pragma once

#include "diagnostics/ProtocolLogEntry.h"

#include <QAbstractListModel>
#include <QString>

#include <deque>

namespace xp60studio::presentation {

// Bounded, append-only view of protocol diagnostics for the activity panel.
class ProtocolLogModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        TimeTextRole = Qt::UserRole + 1,
        DirectionRole,   // "IN" / "OUT" / "SYS"
        KindRole,        // diagnostics::LogKind name
        SeverityRole,    // "Info" / "Warning" / "Error"
        SummaryRole,
        DetailRole,
        RawHexRole,
        ChecksumRole,    // "OK" / "INVALID" / "-"
        RequestIdRole,   // 0 when none
        EndpointRole,
        LineRole,        // formatted one-line form
        IsRolandRole,
    };

    explicit ProtocolLogModel(QObject* parent = nullptr);

    void append(const diagnostics::ProtocolLogEntry& entry);
    void clear();
    void setLimit(std::size_t limit);

    [[nodiscard]] int count() const { return rowCount(); }
    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QString lineAt(int row) const;
    [[nodiscard]] const diagnostics::ProtocolLogEntry* entryAt(int row) const;

signals:
    void countChanged();

private:
    std::deque<diagnostics::ProtocolLogEntry> m_entries;
    std::size_t m_limit = 2000;
};

} // namespace xp60studio::presentation
