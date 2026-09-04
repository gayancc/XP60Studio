#include "presentation/PatchEditorViewModel.h"

#include "xpmodel/Xp60PatchLayout.h"

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

    connect(&m_session, &services::DeviceSession::patchFetchChanged, this, &PatchEditorViewModel::adoptFetchedPatch);
    if (m_transfer) {
        connect(m_transfer, &services::PatchTransfer::changed, this, &PatchEditorViewModel::writeChanged);
    }
    connect(&m_session, &services::DeviceSession::connectionStateChanged, this, &PatchEditorViewModel::writeChanged);
}

// ---------------------------------------------------------------------------
// Patch lifecycle
// ---------------------------------------------------------------------------

void PatchEditorViewModel::adoptFetchedPatch()
{
    const auto& fetch = m_session.patchFetch();
    if (fetch.state != services::DeviceSession::PatchFetchState::Completed || !fetch.patch) {
        return;
    }
    // A freshly read Patch becomes the new A side; local edits start over.
    m_original = fetch.patch;
    m_current = fetch.patch;
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
    if (m_current->raw(tone, parameter) == raw) {
        return;
    }
    pushUndo();
    if (!m_current->setRaw(tone, parameter, raw)) {
        m_undo.pop_back(); // refused: the value is outside the documented range
        return;
    }
    emitAll();
}

void PatchEditorViewModel::setCommonRaw(CommonParameter parameter, int raw)
{
    if (!m_current || m_comparing) {
        return;
    }
    if (m_current->raw(parameter) == raw) {
        return;
    }
    pushUndo();
    if (!m_current->setRaw(parameter, raw)) {
        m_undo.pop_back();
        return;
    }
    emitAll();
}

bool PatchEditorViewModel::anyToneSoloed() const
{
    return std::any_of(m_tones.begin(), m_tones.end(), [](const auto& tone) { return tone->solo(); });
}

void PatchEditorViewModel::notifyAuditionChanged()
{
    for (auto& tone : m_tones) {
        tone->notifyAuditionChanged();
    }
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
    if (m_comparing) {
        return QStringLiteral("A · ORIGINAL");
    }
    return modified() ? QStringLiteral("MODIFIED") : QStringLiteral("ON XP-60");
}

QString PatchEditorViewModel::stateBadgeTone() const
{
    if (!m_current) {
        return QStringLiteral("neutral");
    }
    if (m_comparing) {
        return QStringLiteral("info");
    }
    return modified() ? QStringLiteral("warning") : QStringLiteral("success");
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
    return m_current ? m_current->enabledToneCount() : 0;
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
        .arg(QString::fromStdString(m_current->displayText(CommonParameter::StructureType12)),
             QString::fromStdString(m_current->displayText(CommonParameter::StructureType34)));
}

QString PatchEditorViewModel::mfxText() const
{
    // The EFX type list is not transcribed; the map gives the raw index only.
    return m_current ? QStringLiteral("Type %1").arg(QString::fromStdString(m_current->displayText(CommonParameter::EfxType)))
                     : QString();
}

QString PatchEditorViewModel::chorusText() const
{
    return m_current ? QStringLiteral("Level %1").arg(m_current->raw(CommonParameter::ChorusLevel)) : QString();
}

QString PatchEditorViewModel::reverbText() const
{
    return m_current ? QString::fromStdString(m_current->displayText(CommonParameter::ReverbType)) : QString();
}

QString PatchEditorViewModel::outputText() const
{
    return m_current ? QStringLiteral("Level %1").arg(m_current->raw(CommonParameter::PatchLevel)) : QString();
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
        return m_current->pitchEnvelope(tone);
    case Filter:
        return m_current->filterEnvelope(tone);
    case Amp:
        return m_current->levelEnvelope(tone);
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
    const bool bipolar = m_section != Amp; // pitch and filter levels are -63..+63

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
                         QString::fromStdString(m_current->displayText(tone, parameters.levels[static_cast<std::size_t>(i)])));
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
        const double scale = m_section == Amp ? 127.0 : 126.0;
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
    return m_current ? m_current->raw(selectedToneIndex(), ToneParameter::KeyboardRangeLower) : 0;
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
    return m_current ? m_current->raw(selectedToneIndex(), ToneParameter::KeyboardRangeUpper) : 127;
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
    return m_current ? QString::fromStdString(m_current->displayText(selectedToneIndex(), ToneParameter::KeyboardRangeLower))
                     : QString();
}

QString PatchEditorViewModel::keyRangeUpperText() const
{
    return m_current ? QString::fromStdString(m_current->displayText(selectedToneIndex(), ToneParameter::KeyboardRangeUpper))
                     : QString();
}

int PatchEditorViewModel::velocityLower() const
{
    return m_current ? m_current->raw(selectedToneIndex(), ToneParameter::VelocityRangeLower) : 1;
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
    return m_current ? m_current->raw(selectedToneIndex(), ToneParameter::VelocityRangeUpper) : 127;
}

void PatchEditorViewModel::setVelocityUpper(int value)
{
    if (!m_current) {
        return;
    }
    setToneRaw(selectedToneIndex(), ToneParameter::VelocityRangeUpper, std::clamp(value, velocityLower(), 127));
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
        row.insert(QStringLiteral("raw"), m_current->raw(parameter));
        row.insert(QStringLiteral("valueText"), QString::fromStdString(m_current->displayText(parameter)));
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
    if (m_undo.empty() || !m_current) {
        return;
    }
    m_redo.push_back(*m_current);
    m_current = m_undo.back();
    m_undo.pop_back();
    emitAll();
}

void PatchEditorViewModel::redo()
{
    if (m_redo.empty() || !m_current) {
        return;
    }
    m_undo.push_back(*m_current);
    m_current = m_redo.back();
    m_redo.pop_back();
    emitAll();
}

void PatchEditorViewModel::revertToOriginal()
{
    if (!m_original || !m_current || *m_current == *m_original) {
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
    return m_transfer && m_current && m_transfer->isArmed() && !m_transfer->isBusy();
}

bool PatchEditorViewModel::canArmWrite() const
{
    return m_transfer && m_current && m_transfer->canArm();
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
    if (m_transfer) {
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
