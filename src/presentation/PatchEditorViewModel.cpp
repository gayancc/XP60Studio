#include "presentation/PatchEditorViewModel.h"

#include "library/ExpansionBoardCatalog.h"
#include "sounddna/generated/SoundDnaModel.generated.h"
#include "xpmodel/Xp60WaveIdentifier.h"

#include "xp60/Xp60Device.h"
#include "xpmodel/Xp60PatchLayout.h"
#include "xpmodel/Xp60Effects.h"
#include "xpmodel/Xp60PatchRouting.h"

#include <QVariantMap>
#include <QStringList>

#include <algorithm>
#include <utility>

namespace xp60studio::presentation {

using xpmodel::CommonParameter;
using xpmodel::ToneIndex;
using xpmodel::ToneParameter;
using xpmodel::Xp60PatchDiff;
namespace tables = xpmodel::xp60tables;

namespace {

constexpr std::size_t kUndoLimit = 64;

QString toQString(std::string_view text)
{
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

} // namespace

PatchEditorViewModel::PatchEditorViewModel(services::DeviceSession& session, services::PatchWorkspace& workspace,
                                           services::PatchTransfer* transfer, QObject* parent)
    : PatchEditorViewModel(session, workspace, transfer, sounddna::generated::model(), parent)
{
}

PatchEditorViewModel::PatchEditorViewModel(services::DeviceSession& session, services::PatchWorkspace& workspace,
                                           services::PatchTransfer* transfer,
                                           sounddna::SoundDnaKnowledgeModel dnaModel, QObject* parent)
    : QObject(parent)
    , m_session(session)
    , m_workspace(workspace)
    , m_transfer(transfer)
    , m_dnaModel(std::move(dnaModel))
    , m_dnaAnalyzer(m_dnaModel)
    , m_dnaTransformer(m_dnaModel)
{
    for (const auto tone : ToneIndex::all()) {
        m_tones.push_back(std::make_unique<ToneViewModel>(*this, tone, this));
    }
    m_sectionParameters = new EditorParameterModel(*this, false, this);
    m_expertParameters = new EditorParameterModel(*this, true, this);

    connect(&m_workspace, &services::PatchWorkspace::changed, this, &PatchEditorViewModel::emitAll);
    // A different Patch is a different set of questions, so nothing a musician
    // dismissed about the last one carries over to this one.
    connect(&m_workspace, &services::PatchWorkspace::originChanged, this, [this] {
        m_keptTones.clear();
        emit compatibilityChanged();
    });
    connect(&m_workspace, &services::PatchWorkspace::syncChanged, this, &PatchEditorViewModel::emitAll);
    connect(&m_session, &services::DeviceSession::patchFetchChanged, this, &PatchEditorViewModel::adoptFetchedPatch);
    if (m_transfer) {
        connect(m_transfer, &services::PatchTransfer::changed, this, [this] {
            // The transfer is the only thing that knows what actually reached
            // the instrument, so it is the only thing allowed to move the
            // device half of the sync state.
            const auto state = m_transfer->state();
            if (state != m_lastTransferState) {
                switch (state) {
                case services::PatchTransfer::State::Verified:
                    if (const auto& readBack = m_transfer->readBack()) {
                        m_workspace.noteVerified(*readBack);
                    }
                    break;
                case services::PatchTransfer::State::Sent:
                    // Transmitted, not proved. The workspace records exactly
                    // that; verification follows when the gesture settles.
                    if (const auto& sent = m_transfer->readBack()) {
                        m_workspace.noteSent(*sent);
                    }
                    break;
                case services::PatchTransfer::State::Sending:
                case services::PatchTransfer::State::ReadingBack:
                case services::PatchTransfer::State::Comparing:
                case services::PatchTransfer::State::CapturingSafetySnapshot:
                    m_workspace.noteSending();
                    break;
                case services::PatchTransfer::State::Mismatch:
                case services::PatchTransfer::State::Failed:
                case services::PatchTransfer::State::Cancelled:
                    m_workspace.noteTransferFailed(QString::fromStdString(m_transfer->message()));
                    break;
                case services::PatchTransfer::State::Idle:
                    break;
                }
            }
            m_lastTransferState = state;
            emitAll();
        });
    }
    // Connecting, disconnecting or re-addressing the instrument all mean the
    // same thing: nothing previously verified about the temporary area can be
    // relied on any more.
    connect(&m_session, &services::DeviceSession::connectionStateChanged, this, [this] {
        m_workspace.setConnected(m_session.connectionState() == services::DeviceSession::ConnectionState::Connected);
        emitAll();
    });
    connect(&m_session, &services::DeviceSession::deviceIdChanged, this, [this] {
        m_workspace.markStale(tr("The device ID changed, so the XP-60's temporary Patch is unknown."));
        emitAll();
    });
}

// ---------------------------------------------------------------------------
// Patch lifecycle
// ---------------------------------------------------------------------------

void PatchEditorViewModel::adoptFetchedPatch()
{
    const auto& fetch = m_session.patchFetch();
    if (fetch.state != services::DeviceSession::PatchFetchState::Completed || !fetch.patch
        || fetch.purpose != services::DeviceSession::PatchFetchPurpose::Editing) {
        return;
    }
    // A freshly read Patch becomes the new A side; local edits start over.
    endEffectGesture();
    if (m_dnaGestureBase) m_workspace.endGesture();
    m_dnaGestureBase.reset();
    m_applyingDnaGesture = false;
    m_dnaLastExplanation.clear();
    const bool temporary = fetch.base == xpmodel::Xp60PatchLayout::temporaryPatchAddress();
    m_workspace.adopt(*fetch.patch, temporary ? services::PatchOrigin::temporary()
                                              : services::PatchOrigin{});
    // A read *is* a verification of the temporary area: these are the bytes the
    // instrument just gave us.
    if (temporary) {
        m_workspace.noteVerified(*fetch.patch);
    }
    m_comparing = false;
    m_sourceText = fetch.base == xpmodel::Xp60PatchLayout::temporaryPatchAddress()
        ? QStringLiteral("Read from the XP-60 temporary area")
        : QStringLiteral("Read from %1").arg(QString::fromStdString(fetch.base.toHexString()));
    emitAll();
}

// Applies `edited` as one undoable step under `label`, and re-renders. The
// workspace decides whether it is a step at all: an edit that changes nothing
// records no history and does not mark the Patch out of sync with the
// instrument.
bool PatchEditorViewModel::commitEdit(xpmodel::Xp60Patch edited, const QString& label)
{
    if (!m_applyingDnaGesture) {
        endSoundDnaGesture();
    }
    if (!m_applyingEffectGesture) {
        endEffectGesture();
    }
    if (!m_workspace.commit(std::move(edited), label)) {
        return false;
    }
    if (!m_applyingDnaGesture) m_dnaLastExplanation.clear();
    emitAll();
    return true;
}

void PatchEditorViewModel::emitAll()
{
    refreshSoundDna();
    queueAudition();
    for (auto& tone : m_tones) {
        tone->notifyChanged();
    }
    emit patchChanged();
    emit envelopeChanged();
    emit rangeChanged();
    emit writeChanged();
    emit compatibilityChanged();
    // Replacing a QVariantList model destroys Slider delegates. Hold this
    // notification until release so a DNA gesture remains continuous.
    if (!m_dnaGestureBase) emit soundDnaChanged();
}

QString transformationExplanation(const sounddna::SoundDnaTransformationResult& result,
                                  std::string_view dimensionId)
{
    QStringList parts;
    const auto* before = result.before.find(dimensionId);
    const auto* after = result.after.find(dimensionId);
    if (before && after) {
        parts << QStringLiteral("%1 %2 → %3")
                     .arg(toQString(before->label).toUpper())
                     .arg(before->score)
                     .arg(after->score);
    }
    for (const auto& change : result.parameterChanges) {
        const QString scope = change.toneNumber > 0
            ? QStringLiteral("Tone %1").arg(change.toneNumber) : QStringLiteral("Patch");
        parts << QStringLiteral("%1: %2 %3 → %4")
                     .arg(scope, toQString(change.parameterName))
                     .arg(change.beforeRaw).arg(change.afterRaw);
    }
    if (!result.secondaryEffects.empty()) {
        QStringList secondary;
        for (const auto& effect : result.secondaryEffects) {
            const int delta = effect.afterScore - effect.beforeScore;
            secondary << QStringLiteral("%1 %2%3").arg(toQString(effect.label))
                             .arg(delta >= 0 ? QStringLiteral("+") : QString()).arg(delta);
        }
        parts << QStringLiteral("Expected secondary: %1").arg(secondary.join(QStringLiteral(", ")));
    }
    if (!result.limitation.empty()) parts << toQString(result.limitation);
    return parts.join(QStringLiteral(" · "));
}

void PatchEditorViewModel::refreshSoundDna()
{
    if (!hasPatch()) {
        m_dnaProfile = {};
        m_dnaProfile.modelVersion = m_dnaModel.version();
        m_dnaProfile.unavailableReason = "Load a Patch to analyze Sound DNA.";
        return;
    }
    m_dnaProfile = m_dnaAnalyzer.analyze(m_dnaExtractor.extract(patch()));
}

QString PatchEditorViewModel::soundDnaStatusText() const
{
    if (m_dnaProfile.available()) return tr("Validated against comparable XP-60 patches");
    return tr("Evidence gate: %1").arg(QString::fromStdString(m_dnaProfile.unavailableReason.empty()
        ? m_dnaModel.unavailableReason() : m_dnaProfile.unavailableReason));
}

QString PatchEditorViewModel::soundDnaModelVersion() const
{
    return QString::fromStdString(m_dnaModel.version());
}

QVariantList PatchEditorViewModel::soundDnaDimensions() const
{
    QVariantList dimensions;
    for (const auto& value : m_dnaProfile.values) {
        QVariantList tones;
        for (const auto& contribution : value.toneContributions) {
            QVariantMap tone;
            tone.insert(QStringLiteral("toneNumber"), contribution.toneNumber);
            tone.insert(QStringLiteral("signedContribution"), contribution.signedContribution);
            tone.insert(QStringLiteral("magnitudePercent"), contribution.magnitudePercent);
            tones.push_back(tone);
        }
        QVariantMap row;
        row.insert(QStringLiteral("id"), QString::fromStdString(value.id));
        row.insert(QStringLiteral("label"), QString::fromStdString(value.label));
        row.insert(QStringLiteral("score"), value.score);
        row.insert(QStringLiteral("intervalLow"), value.intervalLow);
        row.insert(QStringLiteral("intervalHigh"), value.intervalHigh);
        row.insert(QStringLiteral("confidence"), value.confidence == sounddna::Confidence::High ? QStringLiteral("High")
                  : value.confidence == sounddna::Confidence::Medium ? QStringLiteral("Medium") : QStringLiteral("Low"));
        row.insert(QStringLiteral("cohort"), QString::fromStdString(value.cohort));
        row.insert(QStringLiteral("referenceText"), QString::fromStdString(value.referenceText));
        row.insert(QStringLiteral("confidenceReason"), QString::fromStdString(value.confidenceReason));
        row.insert(QStringLiteral("inDistributionSupport"), value.inDistributionSupport);
        row.insert(QStringLiteral("patchWideContributionPercent"), value.patchWideContributionPercent);
        row.insert(QStringLiteral("editable"), value.editable);
        row.insert(QStringLiteral("tones"), tones);
        dimensions.push_back(row);
    }
    return dimensions;
}

void PatchEditorViewModel::beginSoundDnaGesture()
{
    if (!hasPatch() || m_comparing || !m_dnaProfile.available() || m_dnaGestureBase) return;
    m_dnaGestureBase = working();
    m_dnaLastExplanation.clear();
    m_workspace.beginGesture();
}

void PatchEditorViewModel::previewSoundDnaTarget(const QString& dimensionId, int targetScore)
{
    if (!hasPatch() || m_comparing || !m_dnaProfile.available()) return;
    const auto& base = m_dnaGestureBase ? *m_dnaGestureBase : working();
    const auto result = m_dnaTransformer.transform(base, {}, {dimensionId.toStdString(), targetScore});
    if (!result.patch) {
        m_dnaLastExplanation = QString::fromStdString(result.limitation);
        if (!m_dnaGestureBase) emit soundDnaChanged();
        return;
    }
    m_dnaLastExplanation = transformationExplanation(result, dimensionId.toStdString());
    if (*result.patch == working()) {
        if (!m_dnaGestureBase) emit soundDnaChanged();
        return;
    }
    m_applyingDnaGesture = true;
    commitEdit(*result.patch, tr("Shape %1").arg(toQString(dimensionId.toStdString())));
    m_applyingDnaGesture = false;
}

void PatchEditorViewModel::endSoundDnaGesture()
{
    const bool wasActive = m_dnaGestureBase.has_value();
    if (wasActive) m_workspace.endGesture();
    m_dnaGestureBase.reset();
    m_applyingDnaGesture = false;
    if (wasActive) emit soundDnaChanged();
}

void PatchEditorViewModel::setToneRaw(ToneIndex tone, ToneParameter parameter, int raw)
{
    if (!hasPatch() || m_comparing) {
        return;
    }
    if (patch().raw(tone, parameter) == raw) {
        return;
    }
    // All editor entry points, including Expert, obey the documented paired
    // range invariant. Reject a crossing edit without disturbing history.
    if ((parameter == ToneParameter::KeyboardRangeLower && raw > patch().raw(tone, ToneParameter::KeyboardRangeUpper))
        || (parameter == ToneParameter::KeyboardRangeUpper && raw < patch().raw(tone, ToneParameter::KeyboardRangeLower))
        || (parameter == ToneParameter::VelocityRangeLower && raw > patch().raw(tone, ToneParameter::VelocityRangeUpper))
        || (parameter == ToneParameter::VelocityRangeUpper && raw < patch().raw(tone, ToneParameter::VelocityRangeLower))) {
        return;
    }
    auto edited = working();
    if (!edited.setRaw(tone, parameter, raw)) {
        return;
    }
    commitEdit(std::move(edited), tr("Edit Tone %1").arg(tone.number()));
}

void PatchEditorViewModel::setCommonRaw(CommonParameter parameter, int raw)
{
    if (!hasPatch() || m_comparing) {
        return;
    }
    if (patch().raw(parameter) == raw) {
        return;
    }
    auto edited = working();
    if (!edited.setRaw(parameter, raw)) {
        return;
    }
    commitEdit(std::move(edited), tr("Edit Patch Common"));
}

bool PatchEditorViewModel::useWaveInTone(int toneNumber, const QString& bank, int displayNumber)
{
    if (!hasPatch() || m_comparing) {
        return false;
    }
    const auto tone = ToneIndex::fromNumber(toneNumber);
    if (!tone) {
        return false;
    }
    const auto selectedBank = xpmodel::internalWaveBankFromLabel(bank.toStdString());
    if (!selectedBank) {
        return false; // EXP or an unknown bank: never guessed at
    }
    const auto identifier = xpmodel::encodeWave({*selectedBank, displayNumber});
    if (!identifier) {
        return false; // outside the bank; not clamped to fit
    }

    // Build the whole change on a copy first. If any byte is refused the patch
    // and the undo history are left exactly as they were, rather than applying
    // a partial wave reference the instrument never had.
    auto edited = working();
    if (!edited.setRaw(*tone, ToneParameter::WaveGroupType, identifier->groupTypeRaw)
        || !edited.setRaw(*tone, ToneParameter::WaveGroupId, identifier->groupIdRaw)
        || !edited.setRaw(*tone, ToneParameter::WaveNumber, identifier->numberRaw)) {
        return false;
    }
    // Three parameters move together or not at all, as one undo step. An edit
    // that changes nothing is still a success: the Tone already points there.
    commitEdit(std::move(edited), tr("Use %1 %2 in Tone %3").arg(bank).arg(displayNumber).arg(toneNumber));
    return true;
}

bool PatchEditorViewModel::useExpansionWaveInTone(int toneNumber, int waveGroupId, int displayNumber)
{
    if (!hasPatch() || m_comparing) {
        return false;
    }
    const auto tone = ToneIndex::fromNumber(toneNumber);
    if (!tone) {
        return false;
    }
    // Where Roland's list for the board is held, a number outside it is a
    // caller error and is refused. Clamping would write a wave nobody chose.
    if (library::hasSrJv80WaveList(waveGroupId)
        && (displayNumber < 1 || displayNumber > library::srJv80WaveCount(waveGroupId))) {
        return false;
    }
    const auto identifier = xpmodel::encodeExpansionWave(waveGroupId, displayNumber);
    if (!identifier) {
        return false;
    }

    auto edited = working();
    if (!edited.setRaw(*tone, ToneParameter::WaveGroupType, identifier->groupTypeRaw)
        || !edited.setRaw(*tone, ToneParameter::WaveGroupId, identifier->groupIdRaw)
        || !edited.setRaw(*tone, ToneParameter::WaveNumber, identifier->numberRaw)) {
        return false;
    }
    // Named in the undo label the way the musician chose it, so the history
    // reads as what they did rather than as three raw bytes.
    commitEdit(std::move(edited),
               tr("Use %1 in Tone %2")
                   .arg(toQString(library::describeExpansionWave(waveGroupId, displayNumber - 1)))
                   .arg(toneNumber));
    return true;
}

bool PatchEditorViewModel::canUseSelectedWave() const
{
    if (!hasPatch() || m_comparing) {
        return false;
    }
    const auto selected = m_waves.selected();
    if (selected.isEmpty()) {
        return false;
    }
    if (selected.value(QStringLiteral("expansion")).toBool()) {
        // The browser only ever offers boards the musician declared, so an
        // expansion selection is assignable by construction.
        return selected.value(QStringLiteral("waveGroupId")).toInt() >= 0;
    }
    return xpmodel::internalWaveBankFromLabel(selected.value("bank").toString().toStdString()).has_value();
}

bool PatchEditorViewModel::useSelectedWaveInTone()
{
    const auto selected = m_waves.selected();
    if (selected.isEmpty()) {
        return false;
    }
    if (selected.value(QStringLiteral("expansion")).toBool()) {
        return useExpansionWaveInTone(m_selectedTone, selected.value(QStringLiteral("waveGroupId")).toInt(),
                                      selected.value(QStringLiteral("number")).toInt());
    }
    return useWaveInTone(m_selectedTone, selected.value("bank").toString(), selected.value("number").toInt());
}

bool PatchEditorViewModel::anyToneSoloed() const
{
    return std::any_of(m_tones.begin(), m_tones.end(), [](const auto& tone) { return tone->solo(); });
}

void PatchEditorViewModel::setDisclosure(int mode)
{
    endEffectGesture();
    if (mode < Play || mode > Expert || mode == m_disclosure) return;
    m_disclosure = mode;
    emit disclosureChanged();
}

void PatchEditorViewModel::notifyAuditionChanged()
{
    for (auto& tone : m_tones) {
        tone->notifyAuditionChanged();
    }
    queueAudition();
    emit patchChanged();
    emit writeChanged();
}

xpmodel::Xp60Patch PatchEditorViewModel::auditionPatch() const
{
    auto result = patch();
    const bool soloed = anyToneSoloed();
    for (const auto& tone : m_tones) {
        const auto index = *ToneIndex::fromNumber(tone->toneNumber());
        // Only mask voices; never force a disabled Tone on or normalize an
        // undocumented raw switch value simply by entering audition.
        if (tone->mute() || (soloed && !tone->solo())) result.setRaw(index, ToneParameter::ToneSwitch, 0);
    }
    return result;
}

void PatchEditorViewModel::queueAudition()
{
    if (liveAudition() && hasPatch()) m_transfer->queueLivePreview(auditionPatch());
}

QString PatchEditorViewModel::auditionMessage() const
{
    if (liveStopping()) return tr("Finishing audition: syncing the final Patch and checking its read-back.");
    if (liveAudition()) return tr("LIVE: edits, Solo/Mute and A/B reach the temporary Patch. Stop & keep B removes audition masks; Restore before audition restores the captured Patch.");
    return tr("Local editing. Arm, then Start live audition to send edits, Solo/Mute and A/B to the temporary Patch. Hardware validation pending.");
}

void PatchEditorViewModel::startLiveAudition()
{
    if (!canStartLiveAudition()) {
        return;
    }
    // While the musician is moving a control, prove the update after they stop
    // rather than between every two values: a full read-back costs about
    // 265 ms on the XP-60, which is what made live editing feel disconnected.
    // Stopping the audition still verifies, so it never ends unproved.
    m_transfer->setVerification(services::PatchTransfer::Verification::WhenSettled);
    m_transfer->startLivePreview(auditionPatch());
}

void PatchEditorViewModel::resetAuditionFlags()
{
    m_comparing = false;
    for (const auto& tone : m_tones) { tone->setSolo(false); tone->setMute(false); }
    emitAll();
}

void PatchEditorViewModel::stopLiveAudition()
{
    if (!liveAudition() || liveStopping() || !hasPatch()) return;
    m_transfer->stopLivePreview(working());
    resetAuditionFlags();
}

void PatchEditorViewModel::restoreBeforeAudition()
{
    if (!liveAudition() || liveStopping() || !m_transfer->safetySnapshot()) return;
    m_transfer->stopLivePreview(*m_transfer->safetySnapshot());
    resetAuditionFlags();
}

// ---------------------------------------------------------------------------
// Identity
// ---------------------------------------------------------------------------

QString PatchEditorViewModel::patchName() const
{
    if (!hasPatch()) {
        return {};
    }
    const auto& shown = patch();
    return QString::fromStdString(shown.name().displayText());
}

void PatchEditorViewModel::setPatchName(const QString& name)
{
    if (!hasPatch() || m_comparing) {
        return;
    }
    const auto value = xpmodel::PatchName::fromText(name.toStdString());
    if (!value || *value == working().name()) {
        return;
    }
    auto edited = working();
    edited.setName(*value);
    // The rename reaches the Library row and every Bank destination showing this
    // Patch through the workspace, not through a signal between screens.
    commitEdit(std::move(edited), tr("Rename to %1").arg(name));
}

QString PatchEditorViewModel::locationText() const
{
    if (!hasPatch()) {
        return {};
    }
    // A Patch can now arrive from the Library or a bank destination as well as
    // from the instrument, and the header has to say which — "TEMPORARY PATCH"
    // over a library Patch would claim it came off the XP-60.
    switch (m_workspace.origin().kind) {
    case services::PatchOrigin::Kind::LibraryEntry:
        return QStringLiteral("LIBRARY PATCH");
    case services::PatchOrigin::Kind::DeviceUserSlot:
        return QStringLiteral("USER:%1").arg(m_workspace.origin().userNumber, 3, 10, QLatin1Char('0'));
    case services::PatchOrigin::Kind::DeviceTemporary:
        return QStringLiteral("TEMPORARY PATCH");
    case services::PatchOrigin::Kind::None:
        break;
    }
    return QStringLiteral("WORKING PATCH");
}

QString PatchEditorViewModel::sourceText() const
{
    return m_sourceText;
}

bool PatchEditorViewModel::modified() const
{
    return m_workspace.modified();
}

// Two indicators, never one. "Is my work kept?" and "does the instrument hold
// what I am looking at?" are independent questions, and a single badge answering
// both is how an editor ends up telling a musician their sound is saved when
// what it means is sent. See docs/PATCH_SYNCHRONIZATION.md §4.
QString PatchEditorViewModel::studioBadgeText() const
{
    if (!hasPatch()) {
        return {};
    }
    if (m_comparing) {
        return QStringLiteral("A · ORIGINAL");
    }
    return QString::fromLatin1(services::studioStateLabel(m_workspace.studioState()).data(),
                               static_cast<int>(services::studioStateLabel(m_workspace.studioState()).size()));
}

QString PatchEditorViewModel::studioBadgeTone() const
{
    if (m_comparing) {
        return QStringLiteral("info");
    }
    switch (m_workspace.studioState()) {
    case services::StudioState::Edited:
        return QStringLiteral("warning");
    case services::StudioState::Saved:
        return QStringLiteral("success");
    case services::StudioState::Untracked:
        return QStringLiteral("neutral");
    }
    return QStringLiteral("neutral");
}

QString PatchEditorViewModel::deviceBadgeText() const
{
    if (!hasPatch()) {
        return {};
    }
    const auto label = services::deviceStateLabel(m_workspace.deviceState());
    return QString::fromLatin1(label.data(), static_cast<int>(label.size()));
}

QString PatchEditorViewModel::deviceBadgeTone() const
{
    const auto tone = services::deviceStateTone(m_workspace.deviceState());
    return QString::fromLatin1(tone.data(), static_cast<int>(tone.size()));
}

QString PatchEditorViewModel::deviceMessage() const
{
    return m_workspace.deviceMessage();
}

QString PatchEditorViewModel::emptyStateMessage() const
{
    return tr("No Patch loaded. Open one from the Library, from a Bank Builder destination, or connect the "
              "XP-60 on the Devices screen and fetch its temporary Patch.");
}

// ---------------------------------------------------------------------------
// Sections and tones
// ---------------------------------------------------------------------------

QStringList PatchEditorViewModel::sectionNames() const
{
    return {QStringLiteral("Sound"), QStringLiteral("Filter"), QStringLiteral("Amp"), QStringLiteral("Motion"),
            QStringLiteral("Effects")};
}

void PatchEditorViewModel::setSection(int section)
{
    endEffectGesture();
    if (section < 0 || section > Effects || section == m_section) {
        return;
    }
    m_section = section;
    emit sectionChanged();
    emit envelopeChanged();
    emit patchChanged();
}

QVariantList PatchEditorViewModel::tones() const
{
    QVariantList list;
    for (const auto& tone : m_tones) {
        list.append(QVariant::fromValue(static_cast<QObject*>(tone.get())));
    }
    return list;
}

void PatchEditorViewModel::setSelectedTone(int toneNumber)
{
    endEffectGesture();
    if (!ToneIndex::fromNumber(toneNumber) || toneNumber == m_selectedTone) {
        return;
    }
    m_selectedTone = toneNumber;
    emit selectedToneChanged();
    emit envelopeChanged();
    emit rangeChanged();
    emit patchChanged();
}

ToneIndex PatchEditorViewModel::selectedToneIndex() const
{
    return ToneIndex::fromNumber(m_selectedTone).value_or(ToneIndex::tone1());
}

int PatchEditorViewModel::enabledToneCount() const
{
    return hasPatch() ? patch().enabledToneCount() : 0;
}

// ---------------------------------------------------------------------------
// Missing waves
//
// The rule this section exists to hold: XP60Studio never replaces a wave the
// musician did not choose. There is no mapping from an expansion wave to an
// internal one that this project could write honestly, and a Patch quietly
// re-pointed at a substitute is worse than one that plainly does not sound —
// the musician would have no way to know their sound had been changed. So the
// three actions below are the whole workflow, and none of them picks a wave.
// ---------------------------------------------------------------------------

QVariantList PatchEditorViewModel::toneCompatibility() const
{
    QVariantList list;
    if (!hasPatch()) {
        return list;
    }
    const auto profile = m_expansionProfile ? *m_expansionProfile : library::ExpansionProfile{};
    const auto report = library::analysePatch(patch(), profile);
    for (const auto& tone : report.tones) {
        // A switched-off Tone keeps its expansion verdict — turning it back on
        // is an ordinary edit and the board would still be missing — but it has
        // nothing to sound either way, so it is not something to act on now.
        // Disabling the Tone *is* one of the three resolutions.
        const bool unresolved = tone.enabled
            && (tone.status == library::ToneCompatibility::ExpansionMissing
                || tone.status == library::ToneCompatibility::ExpansionUnknown);
        const bool kept = m_keptTones.count(tone.toneNumber) > 0;
        QVariantMap map;
        map.insert(QStringLiteral("toneNumber"), tone.toneNumber);
        map.insert(QStringLiteral("status"),
                   QString::fromUtf8(library::toneCompatibilityName(tone.status).data(),
                                     static_cast<qsizetype>(library::toneCompatibilityName(tone.status).size())));
        map.insert(QStringLiteral("label"),
                   QString::fromUtf8(library::toneCompatibilityLabel(tone.status).data(),
                                     static_cast<qsizetype>(library::toneCompatibilityLabel(tone.status).size())));
        map.insert(QStringLiteral("tone"),
                   tone.status == library::ToneCompatibility::ExpansionMissing ? QStringLiteral("error")
                   : tone.status == library::ToneCompatibility::ExpansionUnknown ? QStringLiteral("warning")
                   : tone.status == library::ToneCompatibility::ExpansionAvailable ? QStringLiteral("success")
                                                                                  : QStringLiteral("neutral"));
        map.insert(QStringLiteral("groupId"), tone.waveGroupId ? *tone.waveGroupId : -1);
        map.insert(QStringLiteral("enabled"), tone.enabled);
        map.insert(QStringLiteral("kept"), kept);
        map.insert(QStringLiteral("needsAttention"), unresolved && !kept);
        list.append(map);
    }
    return list;
}

int PatchEditorViewModel::tonesNeedingAttention() const
{
    int count = 0;
    for (const auto& entry : toneCompatibility()) {
        if (entry.toMap().value(QStringLiteral("needsAttention")).toBool()) {
            ++count;
        }
    }
    return count;
}

bool PatchEditorViewModel::findReplacementFor(int toneNumber)
{
    if (!ToneIndex::fromNumber(toneNumber)) {
        return false;
    }
    setSelectedTone(toneNumber);
    // The screen opens the browser; the musician picks. Nothing is chosen here
    // and no parameter moves until they use one.
    emit replacementRequested(toneNumber);
    return true;
}

bool PatchEditorViewModel::disableTone(int toneNumber)
{
    const auto tone = ToneIndex::fromNumber(toneNumber);
    if (!tone || !hasPatch() || m_comparing) {
        return false;
    }
    setToneRaw(*tone, ToneParameter::ToneSwitch, 0);
    emit compatibilityChanged();
    return true;
}

void PatchEditorViewModel::keepToneAnyway(int toneNumber)
{
    if (!ToneIndex::fromNumber(toneNumber) || !m_keptTones.insert(toneNumber).second) {
        return;
    }
    emit compatibilityChanged();
}

void PatchEditorViewModel::reconsiderTone(int toneNumber)
{
    if (m_keptTones.erase(toneNumber) > 0) {
        emit compatibilityChanged();
    }
}

void PatchEditorViewModel::setExpansionProfile(const library::ExpansionProfile* profile)
{
    if (m_expansionProfile == profile) {
        return;
    }
    m_expansionProfile = profile;
    // The browser offers the boards this instrument has, so it follows the same
    // profile the compatibility verdicts do.
    m_waves.setExpansionProfile(profile);
    emit compatibilityChanged();
}

void PatchEditorViewModel::expansionProfileChanged()
{
    m_waves.expansionProfileChanged();
    emit compatibilityChanged();
}

// ---------------------------------------------------------------------------
// Signal flow
// ---------------------------------------------------------------------------

QString PatchEditorViewModel::structureText() const
{
    if (!hasPatch()) {
        return {};
    }
    return QStringLiteral("%1 / %2")
        .arg(QString::fromStdString(patch().displayText(CommonParameter::StructureType12)),
             QString::fromStdString(patch().displayText(CommonParameter::StructureType34)));
}

QString PatchEditorViewModel::mfxText() const
{
    if (hasPatch()) {
        if (const auto name = xpmodel::efxTypeName(patch().raw(CommonParameter::EfxType))) return toQString(*name);
    }
    return hasPatch() ? QStringLiteral("Type %1").arg(QString::fromStdString(patch().displayText(CommonParameter::EfxType)))
                     : QString();
}

QString PatchEditorViewModel::chorusText() const
{
    return hasPatch() ? QStringLiteral("Level %1").arg(patch().raw(CommonParameter::ChorusLevel)) : QString();
}

QString PatchEditorViewModel::reverbText() const
{
    return hasPatch() ? QString::fromStdString(patch().displayText(CommonParameter::ReverbType)) : QString();
}

QString PatchEditorViewModel::outputText() const
{
    return hasPatch() ? QStringLiteral("Level %1").arg(patch().raw(CommonParameter::PatchLevel)) : QString();
}

QString PatchEditorViewModel::routingSummary() const
{
    if (!hasPatch()) return {};
    const auto route = xpmodel::patchRouting(patch(), selectedToneIndex());
    if (!route.outputTone) return tr("Structure routing is unknown for this raw value.");
    const int type = route.structureType - 1;
    const int outputTone = route.outputTone;
    const auto tone = *ToneIndex::fromNumber(outputTone);
    const int output = patch().raw(tone, ToneParameter::OutputAssign);
    QString text = type == 0 ? tr("Tone %1 routing: ").arg(outputTone)
        : tr("Structure %1: Tones %2+%3 share Tone %3 output settings. ").arg(type + 1).arg(outputTone - 1).arg(outputTone);
    if (output == 2) return text + tr("DIRECT output; Chorus and Reverb sends are ignored.");
    if (output != 0 && output != 1) return text + tr("Output routing is undocumented for this value.");
    text += output == 0 ? tr("Dry sound to MIX. ") : tr("Dry sound through EFX. ");
    text += tr("Tone sends: Chorus %1, Reverb %2. ").arg(patch().raw(tone, ToneParameter::ChorusSendLevel))
        .arg(patch().raw(tone, ToneParameter::ReverbSendLevel));
    if (output == 1) {
        const int efxOutput = patch().raw(CommonParameter::EfxOutputAssign);
        text += efxOutput == 0 ? tr("EFX to MIX; its sends can add Chorus/Reverb. ")
            : efxOutput == 1 ? tr("EFX to DIRECT; EFX Chorus/Reverb sends are ignored. ")
                            : tr("EFX output routing is undocumented. ");
    }
    text += tr("Chorus output: %1.").arg(QString::fromStdString(patch().displayText(CommonParameter::ChorusOutput)));
    return text;
}

QVariantMap PatchEditorViewModel::routing() const
{
    if (!hasPatch()) return {};
    const auto route = xpmodel::patchRouting(patch(), selectedToneIndex());
    const auto nodeName = [](xpmodel::RoutingNode node) -> QString {
        switch (node) {
        case xpmodel::RoutingNode::Source: return QStringLiteral("source");
        case xpmodel::RoutingNode::Efx: return QStringLiteral("efx");
        case xpmodel::RoutingNode::Chorus: return QStringLiteral("chorus");
        case xpmodel::RoutingNode::Reverb: return QStringLiteral("reverb");
        case xpmodel::RoutingNode::Mix: return QStringLiteral("mix");
        case xpmodel::RoutingNode::Direct: return QStringLiteral("direct");
        case xpmodel::RoutingNode::Unknown: return QStringLiteral("unknown");
        }
        return {};
    };
    QVariantList edges;
    QStringList descriptions;
    for (const auto& edge : route.edges) {
        const auto from = nodeName(edge.from);
        const auto to = nodeName(edge.to);
        const auto level = edge.level < 0 ? tr("Unknown") : QString::number(edge.level);
        QString parameter;
        using xpmodel::RoutingNode;
        if (edge.from == RoutingNode::Source) parameter = edge.to == RoutingNode::Chorus ? "tone.chorus_send_level"
            : edge.to == RoutingNode::Reverb ? "tone.reverb_send_level" : "tone.mix_efx_send_level";
        else if (edge.from == RoutingNode::Efx) parameter = edge.to == RoutingNode::Chorus ? "common.efx_chorus_send_level"
            : edge.to == RoutingNode::Reverb ? "common.efx_reverb_send_level" : "common.efx_mix_out_send_level";
        else if (edge.from == RoutingNode::Chorus) parameter = "common.chorus_level";
        else if (edge.from == RoutingNode::Reverb) parameter = "common.reverb_level";
        if (edge.level < 0) parameter.clear();
        edges.append(QVariantMap{{QStringLiteral("from"), from}, {QStringLiteral("to"), to},
                                 {QStringLiteral("level"), level}, {QStringLiteral("open"), edge.open},
                                 {QStringLiteral("parameterId"), parameter}});
        descriptions.append(tr("%1 to %2: %3%4").arg(from, to, level,
                            edge.level < 0 ? QString() : edge.open ? QString() : tr(" (zero path)")));
    }
    const auto source = !route.outputTone ? tr("Unknown structure") : route.combined
        ? tr("Tones %1 + %2").arg(route.outputTone - 1).arg(route.outputTone)
        : tr("Tone %1").arg(route.outputTone);
    QVariantList sourceTones;
    if (route.combined) sourceTones.append(route.outputTone - 1);
    if (route.outputTone) sourceTones.append(route.outputTone);
    return {{QStringLiteral("source"), source}, {QStringLiteral("outputTone"), route.outputTone},
            {QStringLiteral("sourceTones"), sourceTones},
            {QStringLiteral("structure"), route.structureType ? tr("Type %1").arg(route.structureType) : tr("Unknown")},
            {QStringLiteral("unknown"), route.unknown}, {QStringLiteral("edges"), edges},
            {QStringLiteral("description"), descriptions.join(QStringLiteral("; "))}};
}

// ---------------------------------------------------------------------------
// Envelope
// ---------------------------------------------------------------------------

PatchEditorViewModel::EnvelopeParameters PatchEditorViewModel::envelopeParameters() const
{
    EnvelopeParameters p;
    switch (m_section) {
    case Sound:
        p = {{ToneParameter::PitchEnvelopeTime1, ToneParameter::PitchEnvelopeTime2, ToneParameter::PitchEnvelopeTime3,
              ToneParameter::PitchEnvelopeTime4},
             {ToneParameter::PitchEnvelopeLevel1, ToneParameter::PitchEnvelopeLevel2, ToneParameter::PitchEnvelopeLevel3,
              ToneParameter::PitchEnvelopeLevel4},
             4,
             true};
        break;
    case Filter:
        p = {{ToneParameter::FilterEnvelopeTime1, ToneParameter::FilterEnvelopeTime2, ToneParameter::FilterEnvelopeTime3,
              ToneParameter::FilterEnvelopeTime4},
             {ToneParameter::FilterEnvelopeLevel1, ToneParameter::FilterEnvelopeLevel2,
              ToneParameter::FilterEnvelopeLevel3, ToneParameter::FilterEnvelopeLevel4},
             4,
             true};
        break;
    case Amp:
        // Roland's Level envelope has four times but only three levels; the
        // fourth stage falls to silence.
        p = {{ToneParameter::LevelEnvelopeTime1, ToneParameter::LevelEnvelopeTime2, ToneParameter::LevelEnvelopeTime3,
              ToneParameter::LevelEnvelopeTime4},
             {ToneParameter::LevelEnvelopeLevel1, ToneParameter::LevelEnvelopeLevel2, ToneParameter::LevelEnvelopeLevel3,
              ToneParameter::LevelEnvelopeLevel3},
             3,
             true};
        break;
    default:
        break;
    }
    return p;
}

std::optional<xpmodel::Xp60Patch::Envelope> PatchEditorViewModel::currentEnvelope() const
{
    if (!hasPatch()) {
        return std::nullopt;
    }
    const auto tone = selectedToneIndex();
    switch (m_section) {
    case Sound:
        return patch().pitchEnvelope(tone);
    case Filter:
        return patch().filterEnvelope(tone);
    case Amp:
        return patch().levelEnvelope(tone);
    default:
        return std::nullopt;
    }
}

bool PatchEditorViewModel::envelopeAvailable() const
{
    return currentEnvelope().has_value();
}

QString PatchEditorViewModel::envelopeTitle() const
{
    if (!hasPatch()) {
        return {};
    }
    const auto envelope = currentEnvelope();
    if (!envelope) {
        return m_section == Motion ? QStringLiteral("LFO — no envelope in this section")
                                   : QStringLiteral("Effects — no envelope in this section");
    }
    return QStringLiteral("%1 (Tone %2)")
        .arg(toQString(envelope->name))
        .arg(m_selectedTone)
        .toUpper();
}

QString PatchEditorViewModel::envelopeUnitNote() const
{
    // The Parameter Address Map gives envelope times and levels as raw 0..127
    // with no conversion to seconds or decibels, so none is invented here.
    return QStringLiteral("Times and levels are Roland raw values (0-127). The manual gives no conversion to "
                          "seconds or dB, so none is shown.");
}

QVariantList PatchEditorViewModel::envelopePoints() const
{
    QVariantList points;
    const auto envelope = currentEnvelope();
    if (!envelope) {
        return points;
    }
    const auto parameters = envelopeParameters();
    int total = 0;
    for (const int t : envelope->timeRaw) {
        total += t;
    }
    const double span = total > 0 ? total : 1.0;
    const bool bipolar = m_section == Sound; // only Pitch has signed levels

    double elapsed = 0.0;
    QVariantMap origin;
    origin.insert(QStringLiteral("x"), 0.0);
    origin.insert(QStringLiteral("y"), bipolar ? 0.5 : 0.0);
    origin.insert(QStringLiteral("draggable"), false);
    origin.insert(QStringLiteral("label"), QStringLiteral("Start"));
    points.append(origin);

    for (int i = 0; i < 4; ++i) {
        elapsed += envelope->timeRaw[static_cast<std::size_t>(i)];
        const bool hasLevel = i < parameters.levelCount;
        const int levelRaw = hasLevel ? envelope->levelRaw[static_cast<std::size_t>(i)] : 0;
        double y = 0.0;
        if (hasLevel) {
            y = bipolar ? levelRaw / 126.0 : levelRaw / 127.0;
        }
        QVariantMap point;
        point.insert(QStringLiteral("x"), elapsed / span);
        point.insert(QStringLiteral("y"), y);
        point.insert(QStringLiteral("draggable"), true);
        point.insert(QStringLiteral("hasLevel"), hasLevel);
        point.insert(QStringLiteral("timeRaw"), envelope->timeRaw[static_cast<std::size_t>(i)]);
        point.insert(QStringLiteral("levelRaw"), levelRaw);
        point.insert(QStringLiteral("label"), QStringLiteral("T%1").arg(i + 1));
        points.append(point);
    }
    return points;
}

QVariantList PatchEditorViewModel::envelopeStages() const
{
    QVariantList stages;
    const auto envelope = currentEnvelope();
    if (!envelope) {
        return stages;
    }
    const auto parameters = envelopeParameters();
    const auto tone = selectedToneIndex();
    for (int i = 0; i < 4; ++i) {
        QVariantMap stage;
        stage.insert(QStringLiteral("index"), i);
        stage.insert(QStringLiteral("timeLabel"), QStringLiteral("Time %1").arg(i + 1));
        stage.insert(QStringLiteral("timeRaw"), envelope->timeRaw[static_cast<std::size_t>(i)]);
        const bool hasLevel = i < parameters.levelCount;
        stage.insert(QStringLiteral("hasLevel"), hasLevel);
        if (hasLevel) {
            stage.insert(QStringLiteral("levelLabel"), QStringLiteral("Level %1").arg(i + 1));
            stage.insert(QStringLiteral("levelRaw"), envelope->levelRaw[static_cast<std::size_t>(i)]);
            stage.insert(QStringLiteral("levelText"),
                         QString::fromStdString(patch().displayText(tone, parameters.levels[static_cast<std::size_t>(i)])));
        }
        stages.append(stage);
    }
    return stages;
}

void PatchEditorViewModel::moveEnvelopePoint(int index, double x, double y)
{
    const auto parameters = envelopeParameters();
    if (!parameters.valid || !hasPatch() || m_comparing) {
        return;
    }
    // Point 0 is the fixed origin; points 1..4 map to stages 0..3.
    const int stage = index - 1;
    if (stage < 0 || stage > 3) {
        return;
    }
    const auto tone = selectedToneIndex();

    // X drives the stage time: the fraction of the whole envelope this stage
    // occupies, scaled back to a 0..127 raw time.
    const auto envelope = currentEnvelope();
    int total = 0;
    for (const int t : envelope->timeRaw) {
        total += t;
    }
    double before = 0.0;
    for (int i = 0; i < stage; ++i) {
        before += envelope->timeRaw[static_cast<std::size_t>(i)];
    }
    const double span = total > 0 ? total : 1.0;
    const double desired = std::clamp(x, 0.0, 1.0) * span - before;
    const int timeRaw = std::clamp(static_cast<int>(std::lround(desired)), 0, 127);
    setToneRaw(tone, parameters.times[static_cast<std::size_t>(stage)], timeRaw);

    if (stage < parameters.levelCount) {
        const auto& descriptor = tables::descriptor(parameters.levels[static_cast<std::size_t>(stage)]);
        const double scale = descriptor.rawMax;
        const int levelRaw = std::clamp(static_cast<int>(std::lround(std::clamp(y, 0.0, 1.0) * scale)),
                                        descriptor.rawMin, descriptor.rawMax);
        setToneRaw(tone, parameters.levels[static_cast<std::size_t>(stage)], levelRaw);
    }
}

void PatchEditorViewModel::setEnvelopeStageRaw(int stageIndex, bool isLevel, int raw)
{
    const auto parameters = envelopeParameters();
    if (!parameters.valid || stageIndex < 0 || stageIndex > 3) {
        return;
    }
    if (isLevel && stageIndex >= parameters.levelCount) {
        return;
    }
    const auto tone = selectedToneIndex();
    setToneRaw(tone, isLevel ? parameters.levels[static_cast<std::size_t>(stageIndex)]
                             : parameters.times[static_cast<std::size_t>(stageIndex)],
               raw);
}

// ---------------------------------------------------------------------------
// Ranges
// ---------------------------------------------------------------------------

int PatchEditorViewModel::keyRangeLower() const
{
    return hasPatch() ? patch().raw(selectedToneIndex(), ToneParameter::KeyboardRangeLower) : 0;
}

void PatchEditorViewModel::setKeyRangeLower(int value)
{
    if (!hasPatch()) {
        return;
    }
    // Roland states Lower <= Upper; keep the pair coherent rather than
    // writing a range the instrument would reject.
    const int upper = keyRangeUpper();
    setToneRaw(selectedToneIndex(), ToneParameter::KeyboardRangeLower, std::clamp(value, 0, upper));
}

int PatchEditorViewModel::keyRangeUpper() const
{
    return hasPatch() ? patch().raw(selectedToneIndex(), ToneParameter::KeyboardRangeUpper) : 127;
}

void PatchEditorViewModel::setKeyRangeUpper(int value)
{
    if (!hasPatch()) {
        return;
    }
    setToneRaw(selectedToneIndex(), ToneParameter::KeyboardRangeUpper, std::clamp(value, keyRangeLower(), 127));
}

QString PatchEditorViewModel::keyRangeLowerText() const
{
    return hasPatch() ? QString::fromStdString(patch().displayText(selectedToneIndex(), ToneParameter::KeyboardRangeLower))
                     : QString();
}

QString PatchEditorViewModel::keyRangeUpperText() const
{
    return hasPatch() ? QString::fromStdString(patch().displayText(selectedToneIndex(), ToneParameter::KeyboardRangeUpper))
                     : QString();
}

int PatchEditorViewModel::velocityLower() const
{
    return hasPatch() ? patch().raw(selectedToneIndex(), ToneParameter::VelocityRangeLower) : 1;
}

void PatchEditorViewModel::setVelocityLower(int value)
{
    if (!hasPatch()) {
        return;
    }
    setToneRaw(selectedToneIndex(), ToneParameter::VelocityRangeLower, std::clamp(value, 1, velocityUpper()));
}

int PatchEditorViewModel::velocityUpper() const
{
    return hasPatch() ? patch().raw(selectedToneIndex(), ToneParameter::VelocityRangeUpper) : 127;
}

void PatchEditorViewModel::setVelocityUpper(int value)
{
    if (!hasPatch()) {
        return;
    }
    setToneRaw(selectedToneIndex(), ToneParameter::VelocityRangeUpper, std::clamp(value, velocityLower(), 127));
}

// The keybed the control draws: the XP-60's own 61 keys, widened to whole
// octaves whenever the Patch's range reaches past them. Key Range itself stays
// 0..127 — the window only decides what is worth drawing at a readable size.
namespace {

constexpr int kOctave = 12;

int floorToOctave(int note)
{
    return std::max(0, (note / kOctave) * kOctave);
}

int ceilToOctave(int note)
{
    return std::min(127, ((note + kOctave - 1) / kOctave) * kOctave);
}

} // namespace

int PatchEditorViewModel::keyboardWindowLower() const
{
    const auto keys = xp60::keybed();
    if (!hasPatch()) {
        return keys.lowestNote;
    }
    return std::min(keys.lowestNote, floorToOctave(keyRangeLower()));
}

int PatchEditorViewModel::keyboardWindowUpper() const
{
    const auto keys = xp60::keybed();
    if (!hasPatch()) {
        return keys.highestNote;
    }
    return std::max(keys.highestNote, ceilToOctave(keyRangeUpper()));
}

bool PatchEditorViewModel::keyRangeExceedsKeybed() const
{
    if (!hasPatch()) {
        return false;
    }
    const auto keys = xp60::keybed();
    return keyRangeLower() < keys.lowestNote || keyRangeUpper() > keys.highestNote;
}

QString PatchEditorViewModel::keyRangeNote() const
{
    if (!hasPatch()) {
        return {};
    }
    const auto keys = xp60::keybed();
    if (!keyRangeExceedsKeybed()) {
        return QStringLiteral("Within the XP-60's %1 keys").arg(keys.noteCount);

    }
    // Not an error: the parameter is 0..127 and a sequencer can reach these
    // notes. Say so rather than hiding or clamping it.
    return QStringLiteral("Past the XP-60's %1 keys — MIDI only").arg(keys.noteCount);
}

// ---------------------------------------------------------------------------
// Contextual Tone settings
// ---------------------------------------------------------------------------

QVariantList PatchEditorViewModel::toneSettings() const
{
    QVariantList rows;
    if (!hasPatch()) {
        return rows;
    }
    const auto add = [&](CommonParameter parameter) {
        const auto& descriptor = tables::descriptor(parameter);
        QVariantMap row;
        row.insert(QStringLiteral("parameterId"), toQString(descriptor.id));
        row.insert(QStringLiteral("name"), toQString(descriptor.name));
        row.insert(QStringLiteral("raw"), patch().raw(parameter));
        row.insert(QStringLiteral("valueText"), QString::fromStdString(patch().displayText(parameter)));
        row.insert(QStringLiteral("minimum"), descriptor.rawMin);
        row.insert(QStringLiteral("maximum"), descriptor.rawMax);
        row.insert(QStringLiteral("isEnum"), descriptor.isEnumeration());
        rows.append(row);
    };
    add(CommonParameter::BendRangeUp);
    add(CommonParameter::BendRangeDown);
    add(CommonParameter::PortamentoSwitch);
    add(CommonParameter::PortamentoTime);
    add(CommonParameter::KeyAssignMode);
    return rows;
}

void PatchEditorViewModel::setToneSetting(const QString& parameterId, int raw)
{
    if (!hasPatch()) {
        return;
    }
    const auto& table = xpmodel::Xp60PatchLayout::patchCommonTable();
    const auto index = table.indexOf(parameterId.toStdString());
    if (!index) {
        return;
    }
    setCommonRaw(static_cast<CommonParameter>(*index), raw);
}

// ---------------------------------------------------------------------------
// A/B and history
// ---------------------------------------------------------------------------

void PatchEditorViewModel::setComparing(bool comparing)
{
    endEffectGesture();
    if (m_comparing == comparing || !m_workspace.baseline()) {
        return;
    }
    m_comparing = comparing;
    emitAll();
}

QString PatchEditorViewModel::differenceSummary() const
{
    if (!hasPatch() || !m_workspace.baseline()) {
        return {};
    }
    const auto diff = Xp60PatchDiff::compare(*m_workspace.baseline(), working());
    return diff.identical() ? QStringLiteral("No local changes") : QString::fromStdString(diff.summary());
}

void PatchEditorViewModel::undo()
{
    m_dnaLastExplanation.clear();
    endSoundDnaGesture();
    endEffectGesture();
    if (m_comparing) {
        return;
    }
    if (m_workspace.undo()) {
        emitAll();
    }
}

void PatchEditorViewModel::redo()
{
    m_dnaLastExplanation.clear();
    endSoundDnaGesture();
    endEffectGesture();
    if (m_comparing) {
        return;
    }
    if (m_workspace.redo()) {
        emitAll();
    }
}

void PatchEditorViewModel::revertToOriginal()
{
    if (m_comparing) {
        return;
    }
    m_dnaLastExplanation.clear();
    endSoundDnaGesture();
    endEffectGesture();
    if (m_workspace.revert()) {
        emitAll();
    }
}

// ---------------------------------------------------------------------------
// Write
// ---------------------------------------------------------------------------

bool PatchEditorViewModel::canWrite() const
{
    return m_transfer && hasPatch() && !m_comparing && m_transfer->isArmed() && !m_transfer->isBusy()
        && !m_transfer->liveActive();
}

bool PatchEditorViewModel::canArmWrite() const
{
    return m_transfer && hasPatch() && !m_comparing && m_transfer->canArm();
}

bool PatchEditorViewModel::writeArmed() const
{
    return m_transfer && m_transfer->isArmed();
}

QString PatchEditorViewModel::writeStateText() const
{
    return m_transfer ? QString::fromStdString(m_transfer->stateLabel()) : QString();
}

QString PatchEditorViewModel::writeTone() const
{
    if (!m_transfer) {
        return QStringLiteral("neutral");
    }
    switch (m_transfer->state()) {
    case services::PatchTransfer::State::Verified:
        return QStringLiteral("success");
    case services::PatchTransfer::State::Mismatch:
    case services::PatchTransfer::State::Failed:
        return QStringLiteral("error");
    case services::PatchTransfer::State::Idle:
        return QStringLiteral("neutral");
    default:
        return QStringLiteral("warning");
    }
}

QString PatchEditorViewModel::writeMessage() const
{
    if (!m_transfer) {
        return {};
    }
    if (m_transfer->state() == services::PatchTransfer::State::Idle) {
        return QStringLiteral("Writing sends this Patch to the XP-60's temporary area and reads it back to verify "
                              "every parameter. Arm it first.");
    }
    return QString::fromStdString(m_transfer->message());
}

void PatchEditorViewModel::armWrite()
{
    if (canArmWrite()) {
        m_transfer->arm();
    }
}

void PatchEditorViewModel::disarmWrite()
{
    if (m_transfer) {
        m_transfer->disarm();
    }
}

void PatchEditorViewModel::writeToDevice()
{
    if (canWrite()) {
        m_transfer->writeAndVerifyTemporaryPatch(working());
    }
}

} // namespace xp60studio::presentation
