#pragma once

#include "presentation/ConnectionState.h"
#include "presentation/MidiEndpointListModel.h"
#include "presentation/PatchParameterModel.h"
#include "presentation/ProtocolLogModel.h"
#include "presentation/RequestOperationModel.h"
#include "services/DeviceSession.h"

#include <QObject>
#include <QString>
#include <QStringList>

namespace xp60studio::presentation {

// Screen state for Devices / Diagnostics.
//
// QML binds to display-ready properties and calls the Q_INVOKABLE intents.
// Addresses and sizes typed into the expert RQ1 test area are validated and
// turned into protocol objects here; QML never sees Roland byte structures.
class DevicesViewModel : public QObject
{
    Q_OBJECT

    // Endpoints
    Q_PROPERTY(QAbstractItemModel* inputs READ inputs CONSTANT)
    Q_PROPERTY(QAbstractItemModel* outputs READ outputs CONSTANT)
    Q_PROPERTY(int selectedInputIndex READ selectedInputIndex WRITE setSelectedInputIndex NOTIFY selectionChanged)
    Q_PROPERTY(int selectedOutputIndex READ selectedOutputIndex WRITE setSelectedOutputIndex NOTIFY selectionChanged)
    Q_PROPERTY(bool hasEndpoints READ hasEndpoints NOTIFY endpointsChanged)
    Q_PROPERTY(QString backendName READ backendName CONSTANT)

    // Connection
    Q_PROPERTY(xp60studio::presentation::ConnectionState connectionState READ connectionState NOTIFY connectionChanged)
    Q_PROPERTY(QString connectionStateText READ connectionStateText NOTIFY connectionChanged)
    Q_PROPERTY(QString connectionDetail READ connectionDetail NOTIFY connectionChanged)
    Q_PROPERTY(QString connectedInputName READ connectedInputName NOTIFY connectionChanged)
    Q_PROPERTY(QString connectedOutputName READ connectedOutputName NOTIFY connectionChanged)
    Q_PROPERTY(bool canConnect READ canConnect NOTIFY connectionChanged)
    Q_PROPERTY(bool canDisconnect READ canDisconnect NOTIFY connectionChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY connectionChanged)

    // Device configuration
    Q_PROPERTY(int deviceId READ deviceId WRITE setDeviceId NOTIFY deviceIdChanged)
    Q_PROPERTY(int deviceIdMinimum READ deviceIdMinimum CONSTANT)
    Q_PROPERTY(int deviceIdMaximum READ deviceIdMaximum CONSTANT)
    Q_PROPERTY(QString modelIdText READ modelIdText CONSTANT)
    Q_PROPERTY(QString modelIdStatusText READ modelIdStatusText CONSTANT)

    // Expert RQ1 test area
    Q_PROPERTY(QStringList readPresetNames READ readPresetNames CONSTANT)
    Q_PROPERTY(int selectedReadPresetIndex READ selectedReadPresetIndex WRITE setSelectedReadPresetIndex NOTIFY readPresetChanged)
    Q_PROPERTY(QString readPresetDescription READ readPresetDescription NOTIFY readPresetChanged)
    Q_PROPERTY(QString readPresetStatusText READ readPresetStatusText NOTIFY readPresetChanged)
    Q_PROPERTY(QString requestAddress READ requestAddress WRITE setRequestAddress NOTIFY requestFieldsChanged)
    Q_PROPERTY(QString requestSize READ requestSize WRITE setRequestSize NOTIFY requestFieldsChanged)
    Q_PROPERTY(bool requestAddressValid READ requestAddressValid NOTIFY requestFieldsChanged)
    Q_PROPERTY(bool requestSizeValid READ requestSizeValid NOTIFY requestFieldsChanged)
    Q_PROPERTY(QString requestValidationMessage READ requestValidationMessage NOTIFY requestFieldsChanged)
    Q_PROPERTY(int requestByteCount READ requestByteCount NOTIFY requestFieldsChanged)
    Q_PROPERTY(bool canSendRequest READ canSendRequest NOTIFY canSendRequestChanged)
    Q_PROPERTY(bool hasOutstandingRequests READ hasOutstandingRequests NOTIFY operationsChanged)
    Q_PROPERTY(bool dataSetEnabled READ dataSetEnabled CONSTANT)
    Q_PROPERTY(QString dataSetDisabledReason READ dataSetDisabledReason CONSTANT)

    // Diagnostics
    Q_PROPERTY(QAbstractItemModel* log READ log CONSTANT)
    Q_PROPERTY(QAbstractItemModel* operations READ operations CONSTANT)
    Q_PROPERTY(int messagesIn READ messagesIn NOTIFY statisticsChanged)
    Q_PROPERTY(int messagesOut READ messagesOut NOTIFY statisticsChanged)
    Q_PROPERTY(int sysExIn READ sysExIn NOTIFY statisticsChanged)
    Q_PROPERTY(int sysExOut READ sysExOut NOTIFY statisticsChanged)
    Q_PROPERTY(int checksumFailures READ checksumFailures NOTIFY statisticsChanged)
    Q_PROPERTY(int parseFailures READ parseFailures NOTIFY statisticsChanged)
    Q_PROPERTY(int timeouts READ timeouts NOTIFY statisticsChanged)
    Q_PROPERTY(int requestsCompleted READ requestsCompleted NOTIFY statisticsChanged)
    Q_PROPERTY(int transportErrors READ transportErrors NOTIFY statisticsChanged)
    Q_PROPERTY(QString sysExHealthText READ sysExHealthText NOTIFY statisticsChanged)
    Q_PROPERTY(QString sysExHealthTone READ sysExHealthTone NOTIFY statisticsChanged)

    // Current Patch inspection (Phase 2)
    Q_PROPERTY(bool canFetchPatch READ canFetchPatch NOTIFY patchFetchChanged)
    Q_PROPERTY(bool patchFetchInProgress READ patchFetchInProgress NOTIFY patchFetchChanged)
    Q_PROPERTY(QString patchFetchStateText READ patchFetchStateText NOTIFY patchFetchChanged)
    Q_PROPERTY(QString patchFetchTone READ patchFetchTone NOTIFY patchFetchChanged)
    Q_PROPERTY(QString patchFetchMessage READ patchFetchMessage NOTIFY patchFetchChanged)
    Q_PROPERTY(int patchFetchCompletedBlocks READ patchFetchCompletedBlocks NOTIFY patchFetchChanged)
    Q_PROPERTY(int patchFetchTotalBlocks READ patchFetchTotalBlocks NOTIFY patchFetchChanged)
    Q_PROPERTY(bool currentPatchAvailable READ currentPatchAvailable NOTIFY patchFetchChanged)
    Q_PROPERTY(QString currentPatchName READ currentPatchName NOTIFY patchFetchChanged)
    Q_PROPERTY(QString currentPatchSummary READ currentPatchSummary NOTIFY patchFetchChanged)
    Q_PROPERTY(QString currentPatchDecodeReport READ currentPatchDecodeReport NOTIFY patchFetchChanged)
    Q_PROPERTY(QAbstractItemModel* patchParameters READ patchParameters CONSTANT)

public:
    explicit DevicesViewModel(services::DeviceSession& session, QObject* parent = nullptr);

    [[nodiscard]] QAbstractItemModel* inputs() { return &m_inputs; }
    [[nodiscard]] QAbstractItemModel* outputs() { return &m_outputs; }
    [[nodiscard]] int selectedInputIndex() const { return m_selectedInput; }
    [[nodiscard]] int selectedOutputIndex() const { return m_selectedOutput; }
    void setSelectedInputIndex(int index);
    void setSelectedOutputIndex(int index);
    [[nodiscard]] bool hasEndpoints() const;
    [[nodiscard]] QString backendName() const;

    [[nodiscard]] ConnectionState connectionState() const;
    [[nodiscard]] QString connectionStateText() const;
    [[nodiscard]] QString connectionDetail() const;
    [[nodiscard]] QString connectedInputName() const;
    [[nodiscard]] QString connectedOutputName() const;
    [[nodiscard]] bool canConnect() const;
    [[nodiscard]] bool canDisconnect() const;
    [[nodiscard]] QString lastError() const;

    [[nodiscard]] int deviceId() const;
    void setDeviceId(int displayNumber);
    [[nodiscard]] int deviceIdMinimum() const;
    [[nodiscard]] int deviceIdMaximum() const;
    [[nodiscard]] QString modelIdText() const;
    [[nodiscard]] QString modelIdStatusText() const;

    [[nodiscard]] QStringList readPresetNames() const;
    [[nodiscard]] int selectedReadPresetIndex() const { return m_selectedPreset; }
    void setSelectedReadPresetIndex(int index);
    [[nodiscard]] QString readPresetDescription() const;
    [[nodiscard]] QString readPresetStatusText() const;
    [[nodiscard]] QString requestAddress() const { return m_requestAddress; }
    void setRequestAddress(const QString& text);
    [[nodiscard]] QString requestSize() const { return m_requestSize; }
    void setRequestSize(const QString& text);
    [[nodiscard]] bool requestAddressValid() const;
    [[nodiscard]] bool requestSizeValid() const;
    [[nodiscard]] QString requestValidationMessage() const;
    [[nodiscard]] int requestByteCount() const;
    [[nodiscard]] bool canSendRequest() const;
    [[nodiscard]] bool hasOutstandingRequests() const;
    [[nodiscard]] bool dataSetEnabled() const { return false; }
    [[nodiscard]] QString dataSetDisabledReason() const;

    [[nodiscard]] QAbstractItemModel* log() { return &m_log; }
    [[nodiscard]] QAbstractItemModel* operations() { return &m_operations; }
    [[nodiscard]] int messagesIn() const;
    [[nodiscard]] int messagesOut() const;
    [[nodiscard]] int sysExIn() const;
    [[nodiscard]] int sysExOut() const;
    [[nodiscard]] int checksumFailures() const;
    [[nodiscard]] int parseFailures() const;
    [[nodiscard]] int timeouts() const;
    [[nodiscard]] int requestsCompleted() const;
    [[nodiscard]] int transportErrors() const;
    [[nodiscard]] QString sysExHealthText() const;
    [[nodiscard]] QString sysExHealthTone() const;

    [[nodiscard]] bool canFetchPatch() const;
    [[nodiscard]] bool patchFetchInProgress() const;
    [[nodiscard]] QString patchFetchStateText() const;
    [[nodiscard]] QString patchFetchTone() const;
    [[nodiscard]] QString patchFetchMessage() const;
    [[nodiscard]] int patchFetchCompletedBlocks() const;
    [[nodiscard]] int patchFetchTotalBlocks() const;
    [[nodiscard]] bool currentPatchAvailable() const;
    [[nodiscard]] QString currentPatchName() const;
    [[nodiscard]] QString currentPatchSummary() const;
    [[nodiscard]] QString currentPatchDecodeReport() const;
    [[nodiscard]] QAbstractItemModel* patchParameters() { return &m_patchParameters; }

    // Intents
    Q_INVOKABLE void refreshEndpoints();
    Q_INVOKABLE void fetchCurrentPatch();
    Q_INVOKABLE void cancelPatchFetch();
    Q_INVOKABLE void connectDevice();
    Q_INVOKABLE void disconnectDevice();
    Q_INVOKABLE void applyReadPreset(int index);
    Q_INVOKABLE bool sendRequest();
    Q_INVOKABLE void cancelAllRequests();
    Q_INVOKABLE void clearLog();

    [[nodiscard]] services::DeviceSession& session() noexcept { return m_session; }

signals:
    void selectionChanged();
    void endpointsChanged();
    void connectionChanged();
    void deviceIdChanged();
    void readPresetChanged();
    void requestFieldsChanged();
    void canSendRequestChanged();
    void operationsChanged();
    void statisticsChanged();
    void patchFetchChanged();

private:
    void syncEndpoints();
    void clampSelection();

    services::DeviceSession& m_session;
    MidiEndpointListModel m_inputs;
    MidiEndpointListModel m_outputs;
    ProtocolLogModel m_log;
    RequestOperationModel m_operations;
    PatchParameterModel m_patchParameters;
    int m_selectedInput = -1;
    int m_selectedOutput = -1;
    int m_selectedPreset = 0;
    QString m_requestAddress;
    QString m_requestSize;
};

} // namespace xp60studio::presentation
