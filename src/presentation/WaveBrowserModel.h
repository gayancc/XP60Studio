#pragma once

#include <QAbstractListModel>
#include <QVariantMap>
#include <vector>

namespace xp60studio::presentation {

// Catalog browsing cannot mutate a Patch or access a MIDI service.
class WaveBrowserModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY filterChanged)
    Q_PROPERTY(int sourceFilter READ sourceFilter WRITE setSourceFilter NOTIFY filterChanged)
    Q_PROPERTY(int count READ count NOTIFY filterChanged)
    Q_PROPERTY(QVariantMap selected READ selected NOTIFY selectionChanged)
    Q_PROPERTY(int selectedRow READ selectedRow NOTIFY selectionChanged)
public:
    enum Role { NameRole = Qt::UserRole + 1, BankRole, NumberRole, KeyRole };
    explicit WaveBrowserModel(QObject* parent = nullptr);
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    QString query() const { return m_query; }
    void setQuery(const QString& query);
    int sourceFilter() const { return m_source; }
    void setSourceFilter(int source);
    int count() const { return rowCount(); }
    QVariantMap selected() const;
    int selectedRow() const;
    Q_INVOKABLE void selectRow(int row);
signals:
    void filterChanged();
    void selectionChanged();
private:
    void rebuild();
    QString m_query;
    int m_source = 0;
    int m_selected = -1;
    std::vector<int> m_rows;
};
} // namespace xp60studio::presentation
