#pragma once

#include "presentation/ToneViewModel.h"
#include "presentation/WaveBrowserModel.h"
#include "presentation/EditorParameterModel.h"
#include "services/DeviceSession.h"
#include "services/PatchTransfer.h"
#include "xpmodel/Xp60Patch.h"
#include "xpmodel/Xp60PatchDiff.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <deque>
#include <memory>
#include <optional>

namespace xp60studio::presentation {

// The Patch Editor screen (mockup panel M2).
//
// Editing is local and non-destructive: the fetched Patch is kept as the A
// (original) side, all changes apply to the working copy, and reaching the
// instrument is a separate, explicit, verified action through PatchTransfer.
class PatchEditorViewModel : public QObject
{
    Q_OBJECT

    // Identity
    Q_PROPERTY(bool hasPatch READ hasPatch NOTIFY patchChanged)
    Q_PROPERTY(QString patchName READ patchName WRITE setPatchName NOTIFY patchChanged)
    Q_PROPERTY(QString locationText READ locationText NOTIFY patchChanged)
    Q_PROPERTY(QString sourceText READ sourceText NOTIFY patchChanged)
    Q_PROPERTY(bool modified READ modified NOTIFY patchChanged)
    Q_PROPERTY(QString stateBadgeText READ stateBadgeText NOTIFY patchChanged)
    Q_PROPERTY(QString stateBadgeTone READ stateBadgeTone NOTIFY patchChanged)
    Q_PROPERTY(QString emptyStateMessage READ emptyStateMessage NOTIFY patchChanged)

    // Sections
    Q_PROPERTY(QStringList sectionNames READ sectionNames CONSTANT)
    Q_PROPERTY(int section READ section WRITE setSection NOTIFY sectionChanged)
    Q_PROPERTY(int disclosure READ disclosure WRITE setDisclosure NOTIFY disclosureChanged)
    Q_PROPERTY(EditorParameterModel* sectionParameters READ sectionParameters CONSTANT)
    Q_PROPERTY(EditorParameterModel* expertParameters READ expertParameters CONSTANT)

    // Tones
    Q_PROPERTY(QVariantList tones READ tones CONSTANT)
    Q_PROPERTY(WaveBrowserModel* waves READ waves CONSTANT)
    Q_PROPERTY(int selectedTone READ selectedTone WRITE setSelectedTone NOTIFY selectedToneChanged)
    Q_PROPERTY(bool canUseSelectedWave READ canUseSelectedWave NOTIFY patchChanged)
    Q_PROPERTY(int enabledToneCount READ enabledToneCount NOTIFY patchChanged)

    // Signal flow
    Q_PROPERTY(QString structureText READ structureText NOTIFY patchChanged)
    Q_PROPERTY(QString mfxText READ mfxText NOTIFY patchChanged)
    Q_PROPERTY(QString chorusText READ chorusText NOTIFY patchChanged)
    Q_PROPERTY(QString reverbText READ reverbText NOTIFY patchChanged)
    Q_PROPERTY(QString outputText READ outputText NOTIFY patchChanged)
    Q_PROPERTY(QString routingSummary READ routingSummary NOTIFY patchChanged)
    Q_PROPERTY(QVariantMap routing READ routing NOTIFY patchChanged)
    Q_PROPERTY(QVariantMap effectValues READ effectValues NOTIFY patchChanged)
    Q_PROPERTY(QVariantList effectAlgorithms READ effectAlgorithms CONSTANT)
    Q_PROPERTY(int effectPage READ effectPage WRITE setEffectPage NOTIFY effectPageChanged)

    // Envelope of the selected Tone, for the section in view
    Q_PROPERTY(bool envelopeAvailable READ envelopeAvailable NOTIFY envelopeChanged)
    Q_PROPERTY(QString envelopeTitle READ envelopeTitle NOTIFY envelopeChanged)
    Q_PROPERTY(QVariantList envelopePoints READ envelopePoints NOTIFY envelopeChanged)
    Q_PROPERTY(QVariantList envelopeStages READ envelopeStages NOTIFY envelopeChanged)
    Q_PROPERTY(QString envelopeUnitNote READ envelopeUnitNote CONSTANT)

    // Key / velocity range of the selected Tone
    Q_PROPERTY(int keyRangeLower READ keyRangeLower WRITE setKeyRangeLower NOTIFY rangeChanged)
    Q_PROPERTY(int keyRangeUpper READ keyRangeUpper WRITE setKeyRangeUpper NOTIFY rangeChanged)
    Q_PROPERTY(QString keyRangeLowerText READ keyRangeLowerText NOTIFY rangeChanged)
    Q_PROPERTY(QString keyRangeUpperText READ keyRangeUpperText NOTIFY rangeChanged)
    Q_PROPERTY(int velocityLower READ velocityLower WRITE setVelocityLower NOTIFY rangeChanged)
    Q_PROPERTY(int velocityUpper READ velocityUpper WRITE setVelocityUpper NOTIFY rangeChanged)
    // The span of notes the keybed control should draw, and whether the
    // selected range reaches outside the XP-60's own keys.
    Q_PROPERTY(int keyboardWindowLower READ keyboardWindowLower NOTIFY rangeChanged)
    Q_PROPERTY(int keyboardWindowUpper READ keyboardWindowUpper NOTIFY rangeChanged)
    Q_PROPERTY(bool keyRangeExceedsKeybed READ keyRangeExceedsKeybed NOTIFY rangeChanged)
    Q_PROPERTY(QString keyRangeNote READ keyRangeNote NOTIFY rangeChanged)

    // Contextual Tone settings
    Q_PROPERTY(QVariantList toneSettings READ toneSettings NOTIFY patchChanged)

    // A/B and history
    Q_PROPERTY(bool comparing READ comparing WRITE setComparing NOTIFY patchChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY patchChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY patchChanged)
    Q_PROPERTY(QString differenceSummary READ differenceSummary NOTIFY patchChanged)

    // Write
    Q_PROPERTY(bool canWrite READ canWrite NOTIFY writeChanged)
    Q_PROPERTY(bool writeBusy READ writeBusy NOTIFY writeChanged)
    Q_PROPERTY(QString writeStateText READ writeStateText NOTIFY writeChanged)
    Q_PROPERTY(QString writeTone READ writeTone NOTIFY writeChanged)
    Q_PROPERTY(QString writeMessage READ writeMessage NOTIFY writeChanged)
    Q_PROPERTY(bool writeArmed READ writeArmed NOTIFY writeChanged)
    Q_PROPERTY(bool canArmWrite READ canArmWrite NOTIFY writeChanged)
    Q_PROPERTY(bool liveAudition READ liveAudition NOTIFY writeChanged)
    Q_PROPERTY(bool liveStopping READ liveStopping NOTIFY writeChanged)
    Q_PROPERTY(bool canStartLiveAudition READ canStartLiveAudition NOTIFY writeChanged)
    Q_PROPERTY(QString auditionMessage READ auditionMessage NOTIFY writeChanged)

public:
    WaveBrowserModel* waves() { return &m_waves; }
    [[nodiscard]] bool writeBusy() const { return m_transfer && m_transfer->isBusy(); }
    Q_INVOKABLE void cancelWrite() { if (m_transfer) m_transfer->cancel(); }

    enum Section {
        Sound = 0,
        Filter,
        Amp,
        Motion,
        Effects,
    };
    Q_ENUM(Section)

    enum Disclosure { Play = 0, Design, Expert };
    Q_ENUM(Disclosure)
    int disclosure() const { return m_disclosure; }
    void setDisclosure(int mode);
    EditorParameterModel* sectionParameters() const { return m_sectionParameters; }
    EditorParameterModel* expertParameters() const { return m_expertParameters; }

    explicit PatchEditorViewModel(services::DeviceSession& session, services::PatchTransfer* transfer = nullptr,
                                  QObject* parent = nullptr);

    // Model access used by ToneViewModel -------------------------------------
    [[nodiscard]] bool hasPatch() const noexcept { return m_current.has_value(); }
    [[nodiscard]] const xpmodel::Xp60Patch& patch() const { return m_comparing && m_original ? *m_original : *m_current; }
    void setToneRaw(xpmodel::ToneIndex tone, xpmodel::ToneParameter parameter, int raw);
    void setCommonRaw(xpmodel::CommonParameter parameter, int raw);
    [[nodiscard]] bool anyToneSoloed() const;
    void notifyAuditionChanged();

    // Identity ----------------------------------------------------------------
    [[nodiscard]] QString patchName() const;
    void setPatchName(const QString& name);
    [[nodiscard]] QString locationText() const;
    [[nodiscard]] QString sourceText() const;
    [[nodiscard]] bool modified() const;
    [[nodiscard]] QString stateBadgeText() const;
    [[nodiscard]] QString stateBadgeTone() const;
    [[nodiscard]] QString emptyStateMessage() const;

    [[nodiscard]] QStringList sectionNames() const;
    [[nodiscard]] int section() const noexcept { return m_section; }
    void setSection(int section);

    [[nodiscard]] QVariantList tones() const;
    [[nodiscard]] int selectedTone() const noexcept { return m_selectedTone; }
    void setSelectedTone(int toneNumber);
    [[nodiscard]] int enabledToneCount() const;

    [[nodiscard]] QString structureText() const;
    [[nodiscard]] QString mfxText() const;
    [[nodiscard]] QString chorusText() const;
    [[nodiscard]] QString reverbText() const;
    [[nodiscard]] QString outputText() const;
    [[nodiscard]] QString routingSummary() const;
    [[nodiscard]] QVariantMap routing() const;
    QVariantMap effectValues() const;
    QVariantList effectAlgorithms() const;
    int effectPage() const { return m_effectPage; }
    void setEffectPage(int page);
    Q_INVOKABLE void editEffect(const QString& id, int value);
    Q_INVOKABLE void beginEffectGesture();
    Q_INVOKABLE void endEffectGesture();

    [[nodiscard]] bool envelopeAvailable() const;
    [[nodiscard]] QString envelopeTitle() const;
    [[nodiscard]] QVariantList envelopePoints() const;
    [[nodiscard]] QVariantList envelopeStages() const;
    [[nodiscard]] QString envelopeUnitNote() const;
    // Drags envelope point `index` to normalised (x, y) within its neighbours.
    Q_INVOKABLE void moveEnvelopePoint(int index, double x, double y);
    Q_INVOKABLE void setEnvelopeStageRaw(int stageIndex, bool isLevel, int raw);

    [[nodiscard]] int keyRangeLower() const;
    void setKeyRangeLower(int value);
    [[nodiscard]] int keyRangeUpper() const;
    void setKeyRangeUpper(int value);
    [[nodiscard]] QString keyRangeLowerText() const;
    [[nodiscard]] QString keyRangeUpperText() const;
    [[nodiscard]] int velocityLower() const;
    void setVelocityLower(int value);
    [[nodiscard]] int velocityUpper() const;
    void setVelocityUpper(int value);
    [[nodiscard]] int keyboardWindowLower() const;
    [[nodiscard]] int keyboardWindowUpper() const;
    [[nodiscard]] bool keyRangeExceedsKeybed() const;
    [[nodiscard]] QString keyRangeNote() const;

    [[nodiscard]] QVariantList toneSettings() const;
    Q_INVOKABLE void setToneSetting(const QString& parameterId, int raw);

    // Use in Tone. Points a Tone at an internal wave as one atomic local edit:
    // Wave Group Type, Group ID and Number move together or not at all, so a
    // single undo takes all three back. Nothing is transmitted -- the edit
    // reaches the instrument through the existing armed write or live audition
    // paths, like any other.
    //
    // Returns false, changing nothing and leaving history untouched, for a wave
    // the instrument could not select: a bank other than INT-A or INT-B, or a
    // number outside that bank. Values are never clamped to fit.
    Q_INVOKABLE bool useWaveInTone(int toneNumber, const QString& bank, int displayNumber);
    // The browser's current selection applied to the currently selected Tone.
    Q_INVOKABLE bool useSelectedWaveInTone();
    [[nodiscard]] bool canUseSelectedWave() const;

    [[nodiscard]] bool comparing() const noexcept { return m_comparing; }
    void setComparing(bool comparing);
    [[nodiscard]] bool canUndo() const noexcept { return !m_comparing && !m_undo.empty(); }
    [[nodiscard]] bool canRedo() const noexcept { return !m_comparing && !m_redo.empty(); }
    [[nodiscard]] QString differenceSummary() const;
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void revertToOriginal();

    [[nodiscard]] bool canWrite() const;
    [[nodiscard]] QString writeStateText() const;
    [[nodiscard]] QString writeTone() const;
    [[nodiscard]] QString writeMessage() const;
    [[nodiscard]] bool writeArmed() const;
    [[nodiscard]] bool canArmWrite() const;
    Q_INVOKABLE void armWrite();
    Q_INVOKABLE void disarmWrite();
    Q_INVOKABLE void writeToDevice();
    bool liveAudition() const { return m_transfer && m_transfer->liveActive(); }
    bool liveStopping() const { return m_transfer && m_transfer->liveStopping(); }
    bool canStartLiveAudition() const { return canWrite(); }
    QString auditionMessage() const;
    Q_INVOKABLE void startLiveAudition();
    Q_INVOKABLE void stopLiveAudition();
    Q_INVOKABLE void restoreBeforeAudition();

signals:
    void patchChanged();
    void effectPageChanged();
    void sectionChanged();
    void disclosureChanged();
    void selectedToneChanged();
    void envelopeChanged();
    void rangeChanged();
    void writeChanged();

private:
    void adoptFetchedPatch();
    void pushUndo();
    void emitAll();
    xpmodel::Xp60Patch auditionPatch() const;
    void queueAudition();
    void resetAuditionFlags();
    [[nodiscard]] xpmodel::ToneIndex selectedToneIndex() const;
    [[nodiscard]] std::optional<xpmodel::Xp60Patch::Envelope> currentEnvelope() const;
    // The Tone parameters backing the envelope in view, so a drag can write them.
    struct EnvelopeParameters
    {
        std::array<xpmodel::ToneParameter, 4> times{};
        std::array<xpmodel::ToneParameter, 4> levels{};
        int levelCount = 4;
        bool valid = false;
    };
    [[nodiscard]] EnvelopeParameters envelopeParameters() const;

    services::DeviceSession& m_session;
    services::PatchTransfer* m_transfer = nullptr;
    services::PatchTransfer::State m_lastTransferState = services::PatchTransfer::State::Idle;
    std::optional<xpmodel::Xp60Patch> m_original;
    std::optional<xpmodel::Xp60Patch> m_current;
    std::optional<xpmodel::Xp60Patch> m_hardware;
    std::vector<std::unique_ptr<ToneViewModel>> m_tones;
    std::deque<xpmodel::Xp60Patch> m_undo;
    std::deque<xpmodel::Xp60Patch> m_redo;
    int m_section = Sound;
    int m_disclosure = Design;
    EditorParameterModel* m_sectionParameters = nullptr;
    WaveBrowserModel m_waves{this};
    EditorParameterModel* m_expertParameters = nullptr;
    int m_selectedTone = 1;
    bool m_comparing = false;
    int m_effectPage = 0;
    bool m_effectGesture = false;
    bool m_effectGestureHasUndo = false;
    bool m_applyingEffectGesture = false;
    QString m_sourceText;
};

} // namespace xp60studio::presentation
