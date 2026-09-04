#pragma once

#include "xpmodel/ParameterDescriptor.h"

#include <QAbstractListModel>
#include <QStringList>

#include <vector>

namespace xp60studio::presentation {

class PatchEditorViewModel;

// Editable, scoped projection of the same Patch used by the mixer. Descriptors
// supply legal values and labels; protocol offsets never cross this boundary.
class EditorParameterModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QStringList groups READ groups NOTIFY groupsChanged)
    Q_PROPERTY(int group READ group WRITE setGroup NOTIFY groupsChanged)
    Q_PROPERTY(bool commonScope READ commonScope WRITE setCommonScope NOTIFY groupsChanged)
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY searchChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QString targetText READ targetText NOTIFY countChanged)
    Q_PROPERTY(int valuesTick READ valuesTick NOTIFY valuesChanged)

public:
    enum Role {
        NameRole = Qt::UserRole + 1, IdRole, ToneRole, CategoryRole,
        RawRole, ValueTextRole, MinimumRole, MaximumRole, ChoicesRole,
    };

    explicit EditorParameterModel(PatchEditorViewModel& editor, bool expert, QObject* parent = nullptr);
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    QString targetText() const;

    QStringList groups() const { return m_groups; }
    int group() const { return m_group; }
    void setGroup(int group);
    bool commonScope() const { return m_commonScope; }
    void setCommonScope(bool common);
    QString search() const { return m_search; }
    void setSearch(const QString& search);
    Q_INVOKABLE void edit(const QString& parameterId, int toneNumber, int raw);
    Q_INVOKABLE int rawForId(const QString& parameterId) const;
    Q_INVOKABLE QStringList choicesForId(const QString& parameterId) const;
    Q_INVOKABLE QString valueTextForId(const QString& parameterId) const;
    [[nodiscard]] int valuesTick() const noexcept { return m_valuesTick; }

signals:
    void groupsChanged();
    void searchChanged();
    void countChanged();
    void valuesChanged();

private:
    struct Row {
        const xpmodel::ParameterDescriptor* descriptor;
        std::size_t parameterIndex;
        int toneNumber; // 0 = Patch Common
    };
    void resetGroups();
    void rebuild();
    void refreshValues();
    static QString category(const xpmodel::ParameterDescriptor& descriptor);

    PatchEditorViewModel& m_editor;
    bool m_expert;
    bool m_commonScope = false;
    QStringList m_groups;
    int m_group = 0;
    QString m_search;
    std::vector<Row> m_rows;
    int m_valuesTick = 0;
};

} // namespace xp60studio::presentation
