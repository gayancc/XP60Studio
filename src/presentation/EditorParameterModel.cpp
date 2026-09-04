#include "presentation/EditorParameterModel.h"

#include "presentation/PatchEditorViewModel.h"
#include "xpmodel/generated/Xp60PatchTables.h"
#include "xpmodel/Xp60Effects.h"

#include <algorithm>

namespace xp60studio::presentation {
namespace tables = xpmodel::xp60tables;
namespace {
QString qstr(std::string_view text)
{
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}
}

EditorParameterModel::EditorParameterModel(PatchEditorViewModel& editor, bool expert, QObject* parent)
    : QAbstractListModel(parent), m_editor(editor), m_expert(expert)
{
    connect(&editor, &PatchEditorViewModel::patchChanged, this, &EditorParameterModel::refreshValues);
    connect(&editor, &PatchEditorViewModel::selectedToneChanged, this, &EditorParameterModel::rebuild);
    if (!expert) {
        connect(&editor, &PatchEditorViewModel::sectionChanged, this, &EditorParameterModel::resetGroups);
    }
    resetGroups();
}

QString EditorParameterModel::category(const xpmodel::ParameterDescriptor& p)
{
    // The source table categorises both LFO waveforms as Wave. In the editor
    // they belong with their respective oscillator, not with PCM selection.
    if (p.id == "tone.lfo1_waveform") return QStringLiteral("LFO1");
    if (p.id == "tone.lfo2_waveform") return QStringLiteral("LFO2");
    return qstr(p.category);
}

void EditorParameterModel::resetGroups()
{
    m_group = 0;
    if (m_expert) {
        m_groups = {QStringLiteral("All")};
        const auto& table = m_commonScope ? tables::patchCommonTable() : tables::patchToneTable();
        for (const auto& p : table.parameters()) {
            if (!p.isText() && !m_groups.contains(category(p))) m_groups.append(category(p));
        }
    } else {
        switch (m_editor.section()) {
        case PatchEditorViewModel::Sound:
            m_groups = {QStringLiteral("Pitch"), QStringLiteral("Pitch Envelope"), QStringLiteral("Wave"),
                        QStringLiteral("Tone Delay"), QStringLiteral("Structure")};
            break;
        case PatchEditorViewModel::Filter:
            m_groups = {QStringLiteral("TVF"), QStringLiteral("TVF Envelope")};
            break;
        case PatchEditorViewModel::Amp:
            m_groups = {QStringLiteral("TVA"), QStringLiteral("TVA Envelope"), QStringLiteral("Pan")};
            break;
        case PatchEditorViewModel::Motion:
            m_groups = {QStringLiteral("LFO1"), QStringLiteral("LFO2"), QStringLiteral("Controllers"),
                        QStringLiteral("Control Switches"), QStringLiteral("Patch controls")};
            break;
        case PatchEditorViewModel::Effects:
            m_groups = {QStringLiteral("EFX"), QStringLiteral("Chorus"), QStringLiteral("Reverb"),
                        QStringLiteral("Tone routing")};
            break;
        }
    }
    rebuild();
    emit groupsChanged();
}

void EditorParameterModel::setGroup(int group)
{
    if (group < 0 || group >= m_groups.size() || group == m_group) return;
    m_group = group;
    rebuild();
    emit groupsChanged();
}

void EditorParameterModel::setCommonScope(bool common)
{
    if (!m_expert || m_commonScope == common) return;
    m_commonScope = common;
    resetGroups();
}

void EditorParameterModel::setSearch(const QString& search)
{
    if (m_search == search) return;
    m_search = search;
    rebuild();
    emit searchChanged();
}

void EditorParameterModel::rebuild()
{
    beginResetModel();
    m_rows.clear();
    if (m_editor.hasPatch() && !m_groups.isEmpty()) {
        const auto groupName = m_groups.at(m_group);
        const bool common = m_expert ? m_commonScope
            : (m_editor.section() == PatchEditorViewModel::Effects && groupName != QStringLiteral("Tone routing"))
              || groupName == QStringLiteral("Structure") || groupName == QStringLiteral("Patch controls");
        const auto params = (common ? tables::patchCommonTable() : tables::patchToneTable()).parameters();
        for (std::size_t i = 0; i < params.size(); ++i) {
            const auto& p = params[i];
            if (p.isText()) continue; // Patch names are edited as one validated string.
            bool matches = groupName == QStringLiteral("All") || category(p) == groupName;
            if (!m_expert && groupName == QStringLiteral("Tone routing")) {
                matches = p.category == "Output" || p.category == "EFX" || p.category == "Chorus" || p.category == "Reverb";
            } else if (!m_expert && groupName == QStringLiteral("Patch controls")) {
                matches = p.id == "common.patch_tempo" || p.id == "common.clock_source"
                    || p.id.starts_with("common.patch_control_source_") || p.id.find("hold_peak") != std::string_view::npos;
            }
            const auto query = m_search.trimmed();
            if (matches && (query.isEmpty() || qstr(p.name).contains(query, Qt::CaseInsensitive)
                            || qstr(p.id).contains(query, Qt::CaseInsensitive))) {
                m_rows.push_back({&p, i, common ? 0 : m_editor.selectedTone()});
            }
        }
    }
    endResetModel();
    ++m_valuesTick;
    emit countChanged();
    emit valuesChanged();
}

void EditorParameterModel::refreshValues()
{
    if (m_rows.empty() || !m_editor.hasPatch()) {
        rebuild();
    } else {
        // Preserve delegates and keyboard focus while a value is edited.
        emit dataChanged(index(0), index(rowCount() - 1), {RawRole, ValueTextRole, MinimumRole, MaximumRole});
        ++m_valuesTick;
        emit valuesChanged();
    }
}

int EditorParameterModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QString EditorParameterModel::targetText() const
{
    const bool common = m_expert ? m_commonScope : !m_rows.empty() && m_rows.front().toneNumber == 0;
    return common ? tr("Patch Common") : tr("Tone %1").arg(m_editor.selectedTone());
}

QVariant EditorParameterModel::data(const QModelIndex& idx, int role) const
{
    if (!idx.isValid() || idx.model() != this || idx.row() < 0 || idx.row() >= rowCount() || !m_editor.hasPatch()) return {};
    const auto& row = m_rows[static_cast<std::size_t>(idx.row())];
    const auto& p = *row.descriptor;
    const auto& block = row.toneNumber == 0 ? m_editor.patch().common()
        : m_editor.patch().tone(xpmodel::ToneIndex::all()[static_cast<std::size_t>(row.toneNumber - 1)]);
    const int raw = block.rawAt(row.parameterIndex);
    switch (role) {
    case Qt::DisplayRole:
    case NameRole: return qstr(p.name);
    case IdRole: return qstr(p.id);
    case ToneRole: return row.toneNumber;
    case CategoryRole: return category(p);
    case RawRole: return raw;
    case ValueTextRole:
        if (p.id == "common.efx_type") {
            if (const auto name = xpmodel::efxTypeName(raw)) return QStringLiteral("%1 · %2").arg(raw + 1).arg(qstr(*name));
        }
        return QString::fromStdString(p.formatDisplay(raw));
    case MinimumRole:
        if (p.id == "tone.keyboard_range_upper") return m_editor.keyRangeLower();
        if (p.id == "tone.velocity_range_upper") return m_editor.velocityLower();
        return p.rawMin;
    case MaximumRole:
        if (p.id == "tone.keyboard_range_lower") return m_editor.keyRangeUpper();
        if (p.id == "tone.velocity_range_lower") return m_editor.velocityUpper();
        return p.rawMax;
    case ChoicesRole: {
        QStringList labels;
        if (p.id == "common.efx_type") {
            for (std::size_t i = 0; i < xpmodel::kEfxTypeNames.size(); ++i)
                labels.append(QStringLiteral("%1 · %2").arg(i + 1, 2, 10, QLatin1Char('0')).arg(qstr(xpmodel::kEfxTypeNames[i])));
            return labels;
        }
        for (const auto label : p.enumLabels) labels.append(qstr(label));
        return labels;
    }
    default: return {};
    }
}

QHash<int, QByteArray> EditorParameterModel::roleNames() const
{
    return {{NameRole, "name"}, {IdRole, "parameterId"}, {ToneRole, "toneNumber"}, {CategoryRole, "category"},
            {RawRole, "rawValue"}, {ValueTextRole, "valueText"}, {MinimumRole, "minimum"},
            {MaximumRole, "maximum"}, {ChoicesRole, "choices"}};
}

void EditorParameterModel::edit(const QString& id, int toneNumber, int raw)
{
    if (!m_editor.hasPatch() || m_editor.comparing()) return;
    // Resolve identity, never a byte offset or a stale visual row number.
    const auto it = std::find_if(m_rows.begin(), m_rows.end(), [&](const Row& row) {
        return row.toneNumber == toneNumber && qstr(row.descriptor->id) == id;
    });
    if (it != m_rows.end()) {
        if (!it->descriptor->isRawInRange(raw)) return;
        if (toneNumber == 0) {
            m_editor.setCommonRaw(static_cast<xpmodel::CommonParameter>(it->parameterIndex), raw);
        } else {
            m_editor.setToneRaw(xpmodel::ToneIndex::all()[static_cast<std::size_t>(toneNumber - 1)],
                               static_cast<xpmodel::ToneParameter>(it->parameterIndex), raw);
        }
        return;
    }
    // Allow visual controls to edit known parameters for the selected Tone even
    // when a group filter hides the row (e.g. LFO shape chips).
    if (toneNumber <= 0 || toneNumber != m_editor.selectedTone()) return;
    const auto& table = tables::patchToneTable();
    const auto& parameters = table.parameters();
    for (std::size_t i = 0; i < parameters.size(); ++i) {
        if (qstr(parameters[i].id) != id || !parameters[i].isRawInRange(raw)) continue;
        m_editor.setToneRaw(xpmodel::ToneIndex::all()[static_cast<std::size_t>(toneNumber - 1)],
                           static_cast<xpmodel::ToneParameter>(i), raw);
        return;
    }
}

int EditorParameterModel::rawForId(const QString& parameterId) const
{
    if (!m_editor.hasPatch()) return 0;
    for (const auto& row : m_rows) {
        if (qstr(row.descriptor->id) != parameterId) continue;
        const auto& block = row.toneNumber == 0 ? m_editor.patch().common()
            : m_editor.patch().tone(xpmodel::ToneIndex::all()[static_cast<std::size_t>(row.toneNumber - 1)]);
        return block.rawAt(row.parameterIndex);
    }
    const int toneNumber = m_editor.selectedTone();
    if (toneNumber <= 0) return 0;
    const auto& table = tables::patchToneTable();
    const auto& parameters = table.parameters();
    for (std::size_t i = 0; i < parameters.size(); ++i) {
        if (qstr(parameters[i].id) != parameterId) continue;
        return m_editor.patch().tone(xpmodel::ToneIndex::all()[static_cast<std::size_t>(toneNumber - 1)]).rawAt(i);
    }
    return 0;
}

QStringList EditorParameterModel::choicesForId(const QString& parameterId) const
{
    for (int i = 0; i < rowCount(); ++i) {
        const auto idx = index(i, 0);
        if (data(idx, IdRole).toString() != parameterId) continue;
        return data(idx, ChoicesRole).toStringList();
    }
    // Fall back to the tone table so LFO shapes remain available even when filtered.
    const auto& table = tables::patchToneTable();
    for (const auto& p : table.parameters()) {
        if (qstr(p.id) != parameterId) continue;
        QStringList labels;
        for (const auto label : p.enumLabels) labels.append(qstr(label));
        return labels;
    }
    return {};
}

QString EditorParameterModel::valueTextForId(const QString& parameterId) const
{
    if (!m_editor.hasPatch()) return {};
    for (int i = 0; i < rowCount(); ++i) {
        const auto idx = index(i, 0);
        if (data(idx, IdRole).toString() == parameterId) return data(idx, ValueTextRole).toString();
    }
    return {};
}

} // namespace xp60studio::presentation
