#include "presentation/PatchEditorViewModel.h"

#include "xp60/Xp60Device.h"
#include "xpmodel/Xp60PatchLayout.h"
#include "xpmodel/Xp60Effects.h"

#include <QVariantMap>

#include <algorithm>

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

PatchEditorViewModel::PatchEditorViewModel(services::DeviceSession& session, services::PatchTransfer* transfer,
                                           QObject* parent)
    : QObject(parent)
    , m_session(session)
    , m_transfer(transfer)
{
    for (const auto tone : ToneIndex::all()) {
        m_tones.push_back(std::make_unique<ToneViewModel>(*this, tone, this));
    }
    m_sectionParameters = new EditorParameterModel(*this, false, this);
    m_expertParameters = new EditorParameterModel(*this, true, this);

    connect(&m_session, &services::DeviceSession::patchFetchChanged, this, &PatchEditorViewModel::adoptFetchedPatch);
    if (m_transfer) {
        connect(m_transfer, &services::PatchTransfer::changed, this, [this] {
            const auto state = m_transfer->state();
            if (state == services::PatchTransfer::State::Verified && state != m_lastTransferState) {
                m_hardware = m_transfer->readBack();
            } else if (m_transfer->isBusy()
                       || m_transfer->state() == services::PatchTransfer::State::Mismatch
                       || m_transfer->state() == services::PatchTransfer::State::Failed
                       || m_transfer->state() == services::PatchTransfer::State::Cancelled) {
                m_hardware.reset();
            }
            m_lastTransferState = state;
            emitAll();
        });
    }
    const auto invalidateHardware = [this] {
        m_hardware.reset();
        emitAll();
    };
    connect(&m_session, &services::DeviceSession::connectionStateChanged, this, invalidateHardware);
    connect(&m_session, &services::DeviceSession::deviceIdChanged, this, invalidateHardware);
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
    m_original = fetch.patch;
    m_current = fetch.patch;
    m_hardware = fetch.patch;
    m_undo.clear();
    m_redo.clear();
    m_comparing = false;
    m_sourceText = fetch.base == xpmodel::Xp60PatchLayout::temporaryPatchAddress()
        ? QStringLiteral("Read from the XP-60 temporary area")
        : QStringLiteral("Read from %1").arg(QString::fromStdString(fetch.base.toHexString()));
    emitAll();
}

void PatchEditorViewModel::pushUndo()
{
    if (!m_current) {
        return;
    }
    m_undo.push_back(*m_current);
    while (m_undo.size() > kUndoLimit) {
        m_undo.pop_front();
    }
    m_redo.clear();
}

void PatchEditorViewModel::emitAll()
{
    queueAudition();
    for (auto& tone : m_tones) {
        tone->notifyChanged();
    }
    emit patchChanged();
    emit envelopeChanged();
    emit rangeChanged();
    emit writeChanged();
}

void PatchEditorViewModel::setToneRaw(ToneIndex tone, ToneParameter parameter, int raw)
{
    if (!m_current || m_comparing) {
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
    auto edited = *m_current;
    if (!edited.setRaw(tone, parameter, raw)) {
        return;
    }
    pushUndo();
    m_current = std::move(edited);
    emitAll();
}

void PatchEditorViewModel::setCommonRaw(CommonParameter parameter, int raw)
{
    if (!m_current || m_comparing) {
        return;
    }
    if (patch().raw(parameter) == raw) {
        return;
    }
    auto edited = *m_current;
    if (!edited.setRaw(parameter, raw)) {
        return;
    }
    pushUndo();
    m_current = std::move(edited);
    emitAll();
}

bool PatchEditorViewModel::anyToneSoloed() const
{
    return std::any_of(m_tones.begin(), m_tones.end(), [](const auto& tone) { return tone->solo(); });
}

void PatchEditorViewModel::setDisclosure(int mode)
{
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
    if (liveAudition() && m_current) m_transfer->queueLivePreview(auditionPatch());
}

QString PatchEditorViewModel::auditionMessage() const
{
    if (liveStopping()) return tr("Finishing audition: syncing the final Patch and checking its read-back.");
    if (liveAudition()) return tr("LIVE: edits, Solo/Mute and A/B reach the temporary Patch. Stop & keep B removes audition masks; Restore before audition restores the captured Patch.");
    return tr("Local editing. Arm, then Start live audition to send edits, Solo/Mute and A/B to the temporary Patch. Hardware validation pending.");
}

void PatchEditorViewModel::startLiveAudition()
{
    if (canStartLiveAudition()) m_transfer->startLivePreview(auditionPatch());
}

void PatchEditorViewModel::resetAuditionFlags()
{
    m_comparing = false;
    for (const auto& tone : m_tones) { tone->setSolo(false); tone->setMute(false); }
    emitAll();
}

void PatchEditorViewModel::stopLiveAudition()
{
    if (!liveAudition() || liveStopping() || !m_current) return;
    m_transfer->stopLivePreview(*m_current);
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
    if (!m_current) {
        return {};
    }
    const auto& shown = m_comparing && m_original ? *m_original : *m_current;
    return QString::fromStdString(shown.name().displayText());
}

void PatchEditorViewModel::setPatchName(const QString& name)
{
    if (!m_current || m_comparing) {
        return;
    }
    const auto value = xpmodel::PatchName::fromText(name.toStdString());
    if (!value || *value == m_current->name()) {
        return;
    }
    pushUndo();
    m_current->setName(*value);
    emitAll();
}

QString PatchEditorViewModel::locationText() const
{
    return m_current ? QStringLiteral("TEMPORARY PATCH") : QString();
}

QString PatchEditorViewModel::sourceText() const
{
    return m_sourceText;
}

bool PatchEditorViewModel::modified() const
{
    return m_current && m_original && !(*m_current == *m_original);
}

QString PatchEditorViewModel::stateBadgeText() const
{
    if (!m_current) {
        return QStringLiteral("NO PATCH");
    }
    if (liveAudition()) {
        return m_hardware && *m_hardware == auditionPatch() && !writeBusy()
            ? tr("LIVE · VERIFIED") : tr("LIVE · PENDING");
    }
    if (m_comparing) {
        return QStringLiteral("A · ORIGINAL");
    }
    if (m_hardware && *m_hardware == *m_current) {
        return QStringLiteral("ON XP-60");
    }
    return modified() ? QStringLiteral("MODIFIED") : QStringLiteral("LOCAL");
}

QString PatchEditorViewModel::stateBadgeTone() const
{
    if (!m_current) {
        return QStringLiteral("neutral");
    }
    if (liveAudition()) return stateBadgeText() == tr("LIVE · VERIFIED") ? QStringLiteral("success") : QStringLiteral("warning");
    if (m_comparing) {
        return QStringLiteral("info");
    }
    return stateBadgeText() == QStringLiteral("ON XP-60") ? QStringLiteral("success")
        : modified() ? QStringLiteral("warning") : QStringLiteral("neutral");
}

QString PatchEditorViewModel::emptyStateMessage() const
{
    return QStringLiteral("No Patch loaded. Go to Devices, connect the XP-60 and fetch the temporary Patch; "
                          "it opens here for editing.");
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
    return m_current ? patch().enabledToneCount() : 0;
}

// ---------------------------------------------------------------------------
// Signal flow
// ---------------------------------------------------------------------------

QString PatchEditorViewModel::structureText() const
{
    if (!m_current) {
        return {};
    }
    return QStringLiteral("%1 / %2")
        .arg(QString::fromStdString(patch().displayText(CommonParameter::StructureType12)),
             QString::fromStdString(patch().displayText(CommonParameter::StructureType34)));
}

QString PatchEditorViewModel::mfxText() const
{
    if (m_current) {
        if (const auto name = xpmodel::efxTypeName(patch().raw(CommonParameter::EfxType))) return toQString(*name);
    }
    return m_current ? QStringLiteral("Type %1").arg(QString::fromStdString(patch().displayText(CommonParameter::EfxType)))
                     : QString();
}

QString PatchEditorViewModel::chorusText() const
{
    return m_current ? QStringLiteral("Level %1").arg(patch().raw(CommonParameter::ChorusLevel)) : QString();
}

QString PatchEditorViewModel::reverbText() const
{
    return m_current ? QString::fromStdString(patch().displayText(CommonParameter::ReverbType)) : QString();
}

QString PatchEditorViewModel::outputText() const
{
    return m_current ? QStringLiteral("Level %1").arg(patch().raw(CommonParameter::PatchLevel)) : QString();
}

QString PatchEditorViewModel::routingSummary() const
{
    if (!m_current) return {};
    const auto structure = m_selectedTone <= 2 ? CommonParameter::StructureType12 : CommonParameter::StructureType34;
    const int type = patch().raw(structure);
    if (type < 0 || type > 9) return tr("Structure routing is unknown for this raw value.");
    // Owner's Manual pp.60-61: structures 2-10 combine each pair into
    // Tone 2/4; the output settings of Tone 1/3 are ignored.
    const int outputTone = type == 0 ? m_selectedTone : (m_selectedTone <= 2 ? 2 : 4);
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
    if (!m_current) {
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
    if (!m_current) {
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
    if (!parameters.valid || !m_current || m_comparing) {
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
    return m_current ? patch().raw(selectedToneIndex(), ToneParameter::KeyboardRangeLower) : 0;
}

void PatchEditorViewModel::setKeyRangeLower(int value)
{
    if (!m_current) {
        return;
    }
    // Roland states Lower <= Upper; keep the pair coherent rather than
    // writing a range the instrument would reject.
    const int upper = keyRangeUpper();
    setToneRaw(selectedToneIndex(), ToneParameter::KeyboardRangeLower, std::clamp(value, 0, upper));
}

int PatchEditorViewModel::keyRangeUpper() const
{
    return m_current ? patch().raw(selectedToneIndex(), ToneParameter::KeyboardRangeUpper) : 127;
}

void PatchEditorViewModel::setKeyRangeUpper(int value)
{
    if (!m_current) {
        return;
    }
    setToneRaw(selectedToneIndex(), ToneParameter::KeyboardRangeUpper, std::clamp(value, keyRangeLower(), 127));
}

QString PatchEditorViewModel::keyRangeLowerText() const
{
    return m_current ? QString::fromStdString(patch().displayText(selectedToneIndex(), ToneParameter::KeyboardRangeLower))
                     : QString();
}

QString PatchEditorViewModel::keyRangeUpperText() const
{
    return m_current ? QString::fromStdString(patch().displayText(selectedToneIndex(), ToneParameter::KeyboardRangeUpper))
                     : QString();
}

int PatchEditorViewModel::velocityLower() const
{
    return m_current ? patch().raw(selectedToneIndex(), ToneParameter::VelocityRangeLower) : 1;
}

void PatchEditorViewModel::setVelocityLower(int value)
{
    if (!m_current) {
        return;
    }
    setToneRaw(selectedToneIndex(), ToneParameter::VelocityRangeLower, std::clamp(value, 1, velocityUpper()));
}

int PatchEditorViewModel::velocityUpper() const
{
    return m_current ? patch().raw(selectedToneIndex(), ToneParameter::VelocityRangeUpper) : 127;
}

void PatchEditorViewModel::setVelocityUpper(int value)
{
    if (!m_current) {
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
    if (!m_current) {
        return keys.lowestNote;
    }
    return std::min(keys.lowestNote, floorToOctave(keyRangeLower()));
}

int PatchEditorViewModel::keyboardWindowUpper() const
{
    const auto keys = xp60::keybed();
    if (!m_current) {
        return keys.highestNote;
    }
    return std::max(keys.highestNote, ceilToOctave(keyRangeUpper()));
}

bool PatchEditorViewModel::keyRangeExceedsKeybed() const
{
    if (!m_current) {
        return false;
    }
    const auto keys = xp60::keybed();
    return keyRangeLower() < keys.lowestNote || keyRangeUpper() > keys.highestNote;
}

QString PatchEditorViewModel::keyRangeNote() const
{
    if (!m_current) {
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
    if (!m_current) {
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
    if (!m_current) {
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
    if (m_comparing == comparing || !m_original) {
        return;
    }
    m_comparing = comparing;
    emitAll();
}

QString PatchEditorViewModel::differenceSummary() const
{
    if (!m_current || !m_original) {
        return {};
    }
    const auto diff = Xp60PatchDiff::compare(*m_original, *m_current);
    return diff.identical() ? QStringLiteral("No local changes") : QString::fromStdString(diff.summary());
}

void PatchEditorViewModel::undo()
{
    if (!canUndo() || !m_current) {
        return;
    }
    m_redo.push_back(*m_current);
    m_current = m_undo.back();
    m_undo.pop_back();
    emitAll();
}

void PatchEditorViewModel::redo()
{
    if (!canRedo() || !m_current) {
        return;
    }
    m_undo.push_back(*m_current);
    m_current = m_redo.back();
    m_redo.pop_back();
    emitAll();
}

void PatchEditorViewModel::revertToOriginal()
{
    if (m_comparing || !m_original || !m_current || *m_current == *m_original) {
        return;
    }
    pushUndo();
    m_current = m_original;
    emitAll();
}

// ---------------------------------------------------------------------------
// Write
// ---------------------------------------------------------------------------

bool PatchEditorViewModel::canWrite() const
{
    return m_transfer && m_current && !m_comparing && m_transfer->isArmed() && !m_transfer->isBusy()
        && !m_transfer->liveActive();
}

bool PatchEditorViewModel::canArmWrite() const
{
    return m_transfer && m_current && !m_comparing && m_transfer->canArm();
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
        m_transfer->writeAndVerifyTemporaryPatch(*m_current);
    }
}

} // namespace xp60studio::presentation
