#include "presentation/PatchParameterModel.h"

namespace xp60studio::presentation {

namespace {

QString toQString(std::string_view text)
{
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

} // namespace

PatchParameterModel::PatchParameterModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

void PatchParameterModel::setPatch(const xpmodel::Xp60Patch& patch)
{
    std::vector<Row> rows;
    auto addBlock = [&](const xpmodel::BlockValues& values, const QString& block, int toneNumber) {
        const auto parameters = values.table().parameters();
        for (std::size_t i = 0; i < parameters.size(); ++i) {
            const auto& p = parameters[i];
            Row row;
            row.block = block;
            row.toneNumber = toneNumber;
            row.category = toQString(p.category);
            row.name = toQString(p.name);
            row.raw = values.rawAt(i);
            row.valueText = QString::fromStdString(p.formatDisplay(row.raw));
            row.offset = static_cast<int>(p.offset);
            row.id = toQString(p.id);
            row.isEnum = p.isEnumeration();
            rows.push_back(std::move(row));
        }
    };
    addBlock(patch.common(), QStringLiteral("Common"), 0);
    for (const auto tone : xpmodel::ToneIndex::all()) {
        addBlock(patch.tone(tone), QStringLiteral("Tone %1").arg(tone.number()), tone.number());
    }
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
    emit countChanged();
}

void PatchParameterModel::clear()
{
    if (m_rows.empty()) {
        return;
    }
    beginResetModel();
    m_rows.clear();
    endResetModel();
    emit countChanged();
}

int PatchParameterModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant PatchParameterModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }
    const auto& row = m_rows[static_cast<std::size_t>(index.row())];
    switch (role) {
    case BlockRole:
        return row.block;
    case ToneNumberRole:
        return row.toneNumber;
    case CategoryRole:
        return row.category;
    case Qt::DisplayRole:
    case NameRole:
        return row.name;
    case ValueTextRole:
        return row.valueText;
    case RawValueRole:
        return row.raw;
    case OffsetRole:
        return row.offset;
    case IdRole:
        return row.id;
    case IsEnumRole:
        return row.isEnum;
    default:
        return {};
    }
}

QHash<int, QByteArray> PatchParameterModel::roleNames() const
{
    return {
        {BlockRole, "block"},       {ToneNumberRole, "toneNumber"}, {CategoryRole, "category"},
        {NameRole, "name"},         {ValueTextRole, "valueText"},   {RawValueRole, "rawValue"},
        {OffsetRole, "offset"},     {IdRole, "parameterId"},        {IsEnumRole, "isEnum"},
    };
}

} // namespace xp60studio::presentation
