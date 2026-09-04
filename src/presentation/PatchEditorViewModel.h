#pragma once

#include "presentation/ToneViewModel.h"
#include "services/DeviceSession.h"
#include "services/PatchTransfer.h"
#include "xpmodel/Xp60Patch.h"
#include "xpmodel/Xp60PatchDiff.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

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

    // Tones
    Q_PROPERTY(QVariantList tones READ tones CONSTANT)
    Q_PROPERTY(int selectedTone READ selectedTone WRITE setSelectedTone NOTIFY selectedToneChanged)
    Q_PROPERTY(int enabledToneCount READ enabledToneCount NOTIFY patchChanged)

    // Signal flow
    Q_PROPERTY(QString structureText READ structureText NOTIFY patchChanged)
    Q_PROPERTY(QString mfxText READ mfxText NOTIFY patchChanged)
    Q_PROPERTY(QString chorusText READ chorusText NOTIFY patchChanged)
    Q_PROPERTY(QString reverbText READ reverbText NOTIFY patchChanged)
    Q_PROPERTY(QString outputText READ outputText NOTIFY patchChanged)

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
    Q_PROPERTY(QString writeStateText READ writeStateText NOTIFY writeChanged)
    Q_PROPERTY(QString writeTone READ writeTone NOTIFY writeChanged)
    Q_PROPERTY(QString writeMessage READ writeMessage NOTIFY writeChanged)
    Q_PROPERTY(bool writeArmed READ writeArmed NOTIFY writeChanged)
    Q_PROPERTY(bool canArmWrite READ canArmWrite NOTIFY writeChanged)

public:
    enum Section {
        Sound = 0,
        Filter,
        Amp,
        Motion,
        Effects,
    };
    Q_ENUM(Section)

    explicit PatchEditorViewModel(services::DeviceSession& session, services::PatchTransfer* transfer = nullptr,
                                  QObject* parent = nullptr);

    // Model access used by ToneViewModel -------------------------------------
    [[nodiscard]] bool hasPatch() const noexcept { return m_current.has_value(); }
    [[nodiscard]] const xpmodel::Xp60Patch& patch() const { return *m_current; }
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

    [[nodiscard]] bool comparing() const noexcept { return m_comparing; }
    void setComparing(bool comparing);
    [[nodiscard]] bool canUndo() const noexcept { return !m_undo.empty(); }
    [[nodiscard]] bool canRedo() const noexcept { return !m_redo.empty(); }
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

signals:
    void patchChanged();
    void sectionChanged();
    void selectedToneChanged();
    void envelopeChanged();
    void rangeChanged();
    void writeChanged();

private:
    void adoptFetchedPatch();
    void pushUndo();
    void emitAll();
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
    std::optional<xpmodel::Xp60Patch> m_original;
    std::optional<xpmodel::Xp60Patch> m_current;
    std::vector<std::unique_ptr<ToneViewModel>> m_tones;
    std::deque<xpmodel::Xp60Patch> m_undo;
    std::deque<xpmodel::Xp60Patch> m_redo;
    int m_section = Sound;
    int m_selectedTone = 1;
    bool m_comparing = false;
    QString m_sourceText;
};

} // namespace xp60studio::presentation
