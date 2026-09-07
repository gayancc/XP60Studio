#pragma once

#include "library/ExpansionProfile.h"
#include "library/PatchCompatibility.h"
#include "presentation/ToneViewModel.h"
#include "presentation/WaveBrowserModel.h"
#include "presentation/EditorParameterModel.h"
#include "services/DeviceSession.h"
#include "services/PatchTransfer.h"
#include "services/PatchWorkspace.h"
#include "sounddna/PatchFeatureExtractor.h"
#include "sounddna/SoundDnaAnalyzer.h"
#include "sounddna/SoundDnaTransformationEngine.h"
#include "xpmodel/Xp60Patch.h"
#include "xpmodel/Xp60PatchDiff.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <deque>
#include <set>
#include <memory>
#include <optional>

namespace xp60studio::presentation {

// The Patch Editor screen (mockup panel M2).
//
// Editing is local and non-destructive: the adopted Patch is kept as the A
// (original) side, all changes apply to the working copy, and reaching the
// instrument is a separate, explicit, verified action through PatchTransfer.
//
// The working copy is **not** owned here. It lives in services::PatchWorkspace,
// which every screen showing the same Patch projects — so a rename made in this
// editor reaches the Library row and the Bank Builder destination without any
// signal passing between screens. This class is the Editor's view of that one
// Patch, not a second copy of it.
class PatchEditorViewModel : public QObject
{
    Q_OBJECT

    // Identity
    Q_PROPERTY(bool hasPatch READ hasPatch NOTIFY patchChanged)
    Q_PROPERTY(QString patchName READ patchName WRITE setPatchName NOTIFY patchChanged)
    Q_PROPERTY(QString locationText READ locationText NOTIFY patchChanged)
    Q_PROPERTY(QString sourceText READ sourceText NOTIFY patchChanged)
    Q_PROPERTY(bool modified READ modified NOTIFY patchChanged)
    // The two independent axes of §4: is my work kept, and does the instrument
    // hold what I am looking at. Deliberately separate properties so no screen
    // can collapse them back into one badge.
    Q_PROPERTY(QString studioBadgeText READ studioBadgeText NOTIFY patchChanged)
    Q_PROPERTY(QString studioBadgeTone READ studioBadgeTone NOTIFY patchChanged)
    Q_PROPERTY(QString deviceBadgeText READ deviceBadgeText NOTIFY patchChanged)
    Q_PROPERTY(QString deviceBadgeTone READ deviceBadgeTone NOTIFY patchChanged)
    Q_PROPERTY(QString deviceMessage READ deviceMessage NOTIFY patchChanged)
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
    Q_PROPERTY(bool canUseSelectedWave READ canUseSelectedWave NOTIFY canUseSelectedWaveChanged)
    Q_PROPERTY(int enabledToneCount READ enabledToneCount NOTIFY patchChanged)

    // Per-Tone compatibility with the declared instrument, and what the
    // musician may do about it. Four entries in Tone order:
    // {toneNumber, status, label, tone, needsAttention, kept}.
    //
    // `needsAttention` is what the Tone card acts on: a Tone whose wave lives
    // on a board that is missing or unaccounted for, which the musician has not
    // already dismissed. XP60Studio offers three ways out and takes none of
    // them by itself — see the Q_INVOKABLEs below.
    Q_PROPERTY(QVariantList toneCompatibility READ toneCompatibility NOTIFY compatibilityChanged)
    Q_PROPERTY(int tonesNeedingAttention READ tonesNeedingAttention NOTIFY compatibilityChanged)

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

    // Evidence-gated Sound DNA. An empty list is a first-class state: QML
    // cannot expose a research candidate rejected by the knowledge model.
    Q_PROPERTY(bool soundDnaAvailable READ soundDnaAvailable NOTIFY soundDnaChanged)
    Q_PROPERTY(bool soundDnaBusy READ soundDnaBusy CONSTANT)
    Q_PROPERTY(QString soundDnaStatusText READ soundDnaStatusText NOTIFY soundDnaChanged)
    Q_PROPERTY(QString soundDnaModelVersion READ soundDnaModelVersion CONSTANT)
    Q_PROPERTY(QVariantList soundDnaDimensions READ soundDnaDimensions NOTIFY soundDnaChanged)
    Q_PROPERTY(QString soundDnaLastExplanation READ soundDnaLastExplanation NOTIFY soundDnaChanged)

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

    // The workspace must outlive the view model: it is the owner of the Patch
    // being worked on, shared with every other screen showing it.
    PatchEditorViewModel(services::DeviceSession& session, services::PatchWorkspace& workspace,
                         services::PatchTransfer* transfer = nullptr, QObject* parent = nullptr);
    // Explicit model injection keeps production evidence-gated while allowing
    // deterministic integration tests of a validated model.
    PatchEditorViewModel(services::DeviceSession& session, services::PatchWorkspace& workspace,
                         services::PatchTransfer* transfer, sounddna::SoundDnaKnowledgeModel dnaModel,
                         QObject* parent = nullptr);

    // Model access used by ToneViewModel -------------------------------------
    [[nodiscard]] bool hasPatch() const noexcept { return m_workspace.hasPatch(); }
    // The Patch on screen, which is the A side while comparing.
    [[nodiscard]] const xpmodel::Xp60Patch& patch() const
    {
        return m_comparing && m_workspace.baseline() ? *m_workspace.baseline() : m_workspace.working();
    }
    // The Patch being edited, never the A side. Mutations copy this.
    [[nodiscard]] const xpmodel::Xp60Patch& working() const { return m_workspace.working(); }
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
    [[nodiscard]] QString studioBadgeText() const;
    [[nodiscard]] QString studioBadgeTone() const;
    [[nodiscard]] QString deviceBadgeText() const;
    [[nodiscard]] QString deviceBadgeTone() const;
    [[nodiscard]] QString deviceMessage() const;
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

    // Points a Tone at a wave on a Wave Expansion Board. `displayNumber` is
    // 1-based as Roland prints it; the Tone carries one less.
    //
    // Refuses a number the board does not have whenever Roland's Waveform List
    // for it is held, rather than writing a reference that would sound as
    // nothing. Where no list is held the field limits are all that can be
    // enforced, and the caller is trusted — but nothing in XP60Studio's own UI
    // offers a wave from such a board.
    Q_INVOKABLE bool useExpansionWaveInTone(int toneNumber, int waveGroupId, int displayNumber);
    // The browser's current selection applied to the currently selected Tone.
    Q_INVOKABLE bool useSelectedWaveInTone();

    // ── Missing waves: three explicit ways out, and no fourth ───────────────
    //
    // A Tone pointing at a wave from a board this instrument does not have is
    // shown, never fixed. XP60Studio has no table mapping an expansion wave to
    // an internal one — such a mapping would be invented, and a Patch silently
    // re-pointed at a wave nobody chose is worse than one that plainly does not
    // sound. So there is no "auto-replace" anywhere in this class, and these
    // three are the whole of what the workflow offers.

    // Selects the Tone and asks the screen to open the Wave Browser, where the
    // musician picks. Chooses nothing itself and changes no data; false when
    // the Tone number is not 1..4.
    Q_INVOKABLE bool findReplacementFor(int toneNumber);
    // Turns the Tone's switch off — an ordinary, undoable edit, the same one
    // the Tone card's own switch makes. The wave it points at is untouched, so
    // this is reversible by turning it back on.
    Q_INVOKABLE bool disableTone(int toneNumber);
    // Dismisses the prompt for this Tone. Changes nothing at all: the Patch is
    // exactly as it was, and it will still not sound on this instrument. The
    // dismissal is per-Patch and is forgotten when another Patch is opened,
    // because a different Patch's Tone 2 is a different question.
    Q_INVOKABLE void keepToneAnyway(int toneNumber);
    // Undoes a dismissal, so a musician who changed their mind is not stuck.
    Q_INVOKABLE void reconsiderTone(int toneNumber);

    [[nodiscard]] QVariantList toneCompatibility() const;
    [[nodiscard]] int tonesNeedingAttention() const;

    // The instrument the Patch is judged against. Optional: without it every
    // expansion Tone reads as undecided.
    void setExpansionProfile(const library::ExpansionProfile* profile);
    Q_INVOKABLE void expansionProfileChanged();
    [[nodiscard]] bool canUseSelectedWave() const;

    [[nodiscard]] bool comparing() const noexcept { return m_comparing; }
    void setComparing(bool comparing);
    [[nodiscard]] bool canUndo() const noexcept { return !m_comparing && m_workspace.canUndo(); }
    [[nodiscard]] bool canRedo() const noexcept { return !m_comparing && m_workspace.canRedo(); }
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

    [[nodiscard]] bool soundDnaAvailable() const noexcept { return m_dnaProfile.available(); }
    [[nodiscard]] bool soundDnaBusy() const noexcept { return false; }
    [[nodiscard]] QString soundDnaStatusText() const;
    [[nodiscard]] QString soundDnaModelVersion() const;
    [[nodiscard]] QVariantList soundDnaDimensions() const;
    [[nodiscard]] QString soundDnaLastExplanation() const { return m_dnaLastExplanation; }
    Q_INVOKABLE void beginSoundDnaGesture();
    Q_INVOKABLE void previewSoundDnaTarget(const QString& dimensionId, int targetScore);
    Q_INVOKABLE void endSoundDnaGesture();

signals:
    void patchChanged();
    void canUseSelectedWaveChanged();
    void effectPageChanged();
    void sectionChanged();
    void disclosureChanged();
    void selectedToneChanged();
    void envelopeChanged();
    void rangeChanged();
    void writeChanged();
    void compatibilityChanged();
    void soundDnaChanged();
    // The screen's cue to open the Wave Browser for `toneNumber`. Emitted only
    // from findReplacementFor(); nothing in this class picks a wave.
    void replacementRequested(int toneNumber);

private:
    void adoptFetchedPatch();
    bool commitEdit(xpmodel::Xp60Patch edited, const QString& label);
    void emitAll();
    xpmodel::Xp60Patch auditionPatch() const;
    void queueAudition();
    void resetAuditionFlags();
    void refreshSoundDna();
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
    services::PatchWorkspace& m_workspace;
    const library::ExpansionProfile* m_expansionProfile = nullptr;
    // Tone numbers the musician chose to keep as they are. Cleared whenever the
    // Patch on screen changes: the dismissal is about this Patch, not the slot.
    std::set<int> m_keptTones;
    services::PatchTransfer* m_transfer = nullptr;
    services::PatchTransfer::State m_lastTransferState = services::PatchTransfer::State::Idle;
    std::vector<std::unique_ptr<ToneViewModel>> m_tones;
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

    sounddna::SoundDnaKnowledgeModel m_dnaModel;
    sounddna::PatchFeatureExtractor m_dnaExtractor;
    sounddna::SoundDnaAnalyzer m_dnaAnalyzer;
    sounddna::SoundDnaTransformationEngine m_dnaTransformer;
    sounddna::SoundDnaProfile m_dnaProfile;
    std::optional<xpmodel::Xp60Patch> m_dnaGestureBase;
    bool m_applyingDnaGesture = false;
    QString m_dnaLastExplanation;
};

} // namespace xp60studio::presentation
