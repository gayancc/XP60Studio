#pragma once

#include "xpmodel/Xp60Patch.h"

#include <QAbstractListModel>
#include <QString>

#include <vector>

namespace xp60studio::presentation {

// Flat, read-only list of every parameter of a decoded Patch (Common first,
// then Tone 1..4) with display-ready text. Used by the Devices inspection
// panel; later screens use richer per-section models.
class PatchParameterModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        BlockRole = Qt::UserRole + 1, // "Common", "Tone 1", ...
        ToneNumberRole,               // 0 for Common
        CategoryRole,
        NameRole,
        ValueTextRole,
        RawValueRole,
        OffsetRole,
        IdRole,
        IsEnumRole,
    };

    explicit PatchParameterModel(QObject* parent = nullptr);

    void setPatch(const xpmodel::Xp60Patch& patch);
    void clear();

    [[nodiscard]] int count() const { return rowCount(); }
    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

signals:
    void countChanged();

private:
    struct Row
    {
        QString block;
        int toneNumber = 0;
        QString category;
        QString name;
        QString valueText;
        int raw = 0;
        int offset = 0;
        QString id;
        bool isEnum = false;
    };
    std::vector<Row> m_rows;
};

} // namespace xp60studio::presentation
