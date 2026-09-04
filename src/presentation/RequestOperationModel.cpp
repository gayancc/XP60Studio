#include "presentation/RequestOperationModel.h"

#include "roland/HexFormat.h"

#include <QStringList>

#include <algorithm>

namespace xp60studio::presentation {

namespace {

QString toQString(std::string_view text)
{
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

QString printableAscii(const std::vector<roland::Byte>& data, const std::vector<bool>& coverage)
{
    QString out;
    for (std::size_t i = 0; i < data.size(); ++i) {
        if (!coverage[i]) {
            out += QChar(u'·'); // middle dot for bytes not yet received
        } else if (data[i] >= 0x20 && data[i] < 0x7F) {
            out += QChar(static_cast<char16_t>(data[i]));
        } else {
            out += QChar(u'.');
        }
    }
    return out;
}

} // namespace

RequestOperationModel::RequestOperationModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

void RequestOperationModel::refresh(const protocol::RolandRequestTracker& tracker)
{
    beginResetModel();
    m_operations.assign(tracker.operations().rbegin(), tracker.operations().rend());
    endResetModel();
    emit countChanged();
}

void RequestOperationModel::clear()
{
    beginResetModel();
    m_operations.clear();
    endResetModel();
    emit countChanged();
}

int RequestOperationModel::outstandingCount() const
{
    return static_cast<int>(std::count_if(m_operations.begin(), m_operations.end(), [](const auto& op) {
        return !protocol::isTerminal(op.state);
    }));
}

int RequestOperationModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_operations.size());
}

QVariant RequestOperationModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }
    const auto& op = m_operations[static_cast<std::size_t>(index.row())];
    switch (role) {
    case RequestIdRole:
        return QVariant::fromValue(static_cast<qulonglong>(op.id.value));
    case StateNameRole:
        return toQString(protocol::requestStateName(op.state));
    case Qt::DisplayRole:
    case StateLabelRole:
        return toQString(protocol::requestStateLabel(op.state));
    case IsTerminalRole:
        return protocol::isTerminal(op.state);
    case IsSuccessRole:
        return op.state == protocol::RequestState::Completed;
    case AddressHexRole:
        return QString::fromStdString(op.request.address().toHexString());
    case SizeHexRole:
        return QString::fromStdString(op.request.size().toHexString());
    case ExpectedBytesRole:
        return static_cast<int>(op.expectedBytes);
    case ReceivedBytesRole:
        return static_cast<int>(op.receivedBytes);
    case ChunkCountRole:
        return static_cast<int>(op.chunkCount);
    case ProgressRole:
        return op.progress();
    case FailureReasonRole:
        return QString::fromStdString(op.failureReason);
    case NotesRole: {
        QStringList notes;
        for (const auto& note : op.notes) {
            notes << QString::fromStdString(note);
        }
        return notes.join(QStringLiteral("; "));
    }
    case DataHexRole: {
        // Show only received bytes to avoid presenting zero padding as data.
        std::vector<roland::Byte> received;
        for (std::size_t i = 0; i < op.data.size(); ++i) {
            if (op.coverage[i]) {
                received.push_back(op.data[i]);
            }
        }
        return QString::fromStdString(roland::toHex(roland::ByteSpan(received.data(), received.size())));
    }
    case DataTextRole:
        return printableAscii(op.data, op.coverage);
    default:
        return {};
    }
}

QHash<int, QByteArray> RequestOperationModel::roleNames() const
{
    return {
        {RequestIdRole, "requestId"},       {StateNameRole, "stateName"},     {StateLabelRole, "stateLabel"},
        {IsTerminalRole, "isTerminal"},     {IsSuccessRole, "isSuccess"},     {AddressHexRole, "addressHex"},
        {SizeHexRole, "sizeHex"},           {ExpectedBytesRole, "expectedBytes"}, {ReceivedBytesRole, "receivedBytes"},
        {ChunkCountRole, "chunkCount"},     {ProgressRole, "progress"},       {FailureReasonRole, "failureReason"},
        {NotesRole, "notes"},               {DataHexRole, "dataHex"},         {DataTextRole, "dataText"},
    };
}

} // namespace xp60studio::presentation
