#include "presentation/PerformanceViewModel.h"

#include "xpmodel/Xp60PerformanceCodec.h"

namespace xp60studio::presentation {

using xpmodel::PartIndex;
using xpmodel::PerformanceCommonParameter;
using xpmodel::PerformancePartParameter;
using xpmodel::Xp60PerformanceLayout;

namespace {

constexpr std::size_t kUndoLimit = 64;

QString toQt(const std::string& text)
{
    return QString::fromStdString(text);
}

QString toQt(std::string_view text)
{
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

// Performance Common holds one Voice Reserve per Part, in Part order.
PerformanceCommonParameter voiceReserveFor(PartIndex part) noexcept
{
    return static_cast<PerformanceCommonParameter>(
        static_cast<std::size_t>(PerformanceCommonParameter::VoiceReserve1) + part.index());
}

} // namespace

PerformanceViewModel::PerformanceViewModel(services::DeviceSession& session, QObject* parent)
    : QObject(parent)
    , m_session(session)
{
    connect(&m_session, &services::DeviceSession::patchFetchChanged, this, &PerformanceViewModel::onFetchChanged);
    connect(&m_session, &services::DeviceSession::connectionStateChanged, this,
            &PerformanceViewModel::transferChanged);
    connect(&m_session, &services::DeviceSession::dataSetBatchFinished, this,
            [this](quint64, bool ok, const QString& message) {
                if (!m_sending) {
                    return;
                }
                m_sending = false;
                setTransfer(ok ? tr("Sent to the XP-60's temporary Performance.") : message,
                            ok ? QStringLiteral("success") : QStringLiteral("error"));
            });
}

// ---------------------------------------------------------------------------
// Reading
// ---------------------------------------------------------------------------

QString PerformanceViewModel::name() const
{
    return hasPerformance() ? toQt(m_working->name().displayText()) : QString();
}

QString PerformanceViewModel::summary() const
{
    return hasPerformance() ? toQt(m_working->summary()) : QString();
}

int PerformanceViewModel::tempo() const
{
    return hasPerformance() ? m_working->display(PerformanceCommonParameter::PerformanceTempo) : 0;
}

QString PerformanceViewModel::keyboardMode() const
{
    return hasPerformance() ? toQt(m_working->displayText(PerformanceCommonParameter::KeyboardMode)) : QString();
}

int PerformanceViewModel::activePartCount() const
{
    return hasPerformance() ? m_working->activePartCount() : 0;
}

QVariantList PerformanceViewModel::parts() const
{
    QVariantList list;
    if (!hasPerformance()) {
        return list;
    }
    for (const auto part : PartIndex::all()) {
        const auto mix = m_working->mix(part);
        const auto range = m_working->range(part);
        const auto assignment = m_working->assignment(part);

        QVariantMap map;
        map.insert(QStringLiteral("partNumber"), part.number());
        // Part 10 is the Rhythm part, which the strip says rather than hides:
        // its Patch fields name a Rhythm Setup, not a Patch.
        map.insert(QStringLiteral("isRhythmPart"), part.isRhythmPart());
        map.insert(QStringLiteral("receives"), mix.receives);
        map.insert(QStringLiteral("midiChannel"), mix.midiChannel);
        map.insert(QStringLiteral("level"), mix.level);
        map.insert(QStringLiteral("pan"), mix.pan);
        map.insert(QStringLiteral("panText"), toQt(mix.panText));
        map.insert(QStringLiteral("outputAssign"), mix.outputAssignRaw);
        map.insert(QStringLiteral("outputAssignLabel"), toQt(mix.outputAssignLabel));
        map.insert(QStringLiteral("chorusSend"), mix.chorusSend);
        map.insert(QStringLiteral("reverbSend"), mix.reverbSend);
        map.insert(QStringLiteral("voiceReserve"), mix.voiceReserve);
        map.insert(QStringLiteral("patchGroupType"), assignment.groupTypeRaw);
        map.insert(QStringLiteral("patchGroupLabel"), toQt(assignment.groupTypeLabel));
        map.insert(QStringLiteral("patchGroupId"), assignment.groupId);
        map.insert(QStringLiteral("patchNumber"), assignment.numberDisplay);
        map.insert(QStringLiteral("keyLowerRaw"), range.lowerRaw);
        map.insert(QStringLiteral("keyUpperRaw"), range.upperRaw);
        map.insert(QStringLiteral("keyLowerNote"), toQt(range.lowerNote));
        map.insert(QStringLiteral("keyUpperNote"), toQt(range.upperNote));
        map.insert(QStringLiteral("octaveShift"), range.octaveShift);
        map.insert(QStringLiteral("coarseTune"), range.coarseTune);
        map.insert(QStringLiteral("fineTune"), range.fineTune);
        list.append(map);
    }
    return list;
}

bool PerformanceViewModel::modified() const
{
    return hasPerformance() && m_baseline && !(*m_working == *m_baseline);
}

QString PerformanceViewModel::undoLabel() const
{
    return m_undo.empty() ? QString() : m_undo.back().label;
}

// ---------------------------------------------------------------------------
// Editing
// ---------------------------------------------------------------------------

bool PerformanceViewModel::commit(const QString& label,
                                  const std::function<bool(xpmodel::Xp60Performance&)>& mutate)
{
    if (!hasPerformance()) {
        return false;
    }
    auto edited = *m_working;
    if (!mutate(edited)) {
        return false;
    }
    // An edit that changes nothing is not a step: it would make undo lie.
    if (edited == *m_working) {
        return true;
    }
    m_undo.push_back(Step{*m_working, label});
    if (m_undo.size() > kUndoLimit) {
        m_undo.pop_front();
    }
    m_redo.clear();
    m_working = std::move(edited);
    emit changed();
    return true;
}

bool PerformanceViewModel::setPartLevel(int partNumber, int level)
{
    const auto part = PartIndex::fromNumber(partNumber);
    if (!part) return false;
    return commit(tr("Part %1 level").arg(partNumber), [&](auto& p) {
        return p.setRaw(*part, PerformancePartParameter::PartLevel, level);
    });
}

bool PerformanceViewModel::setPartPan(int partNumber, int pan)
{
    const auto part = PartIndex::fromNumber(partNumber);
    if (!part) return false;
    return commit(tr("Part %1 pan").arg(partNumber), [&](auto& p) {
        return p.setRaw(*part, PerformancePartParameter::PartPan, pan);
    });
}

bool PerformanceViewModel::setPartReceives(int partNumber, bool receives)
{
    const auto part = PartIndex::fromNumber(partNumber);
    if (!part) return false;
    return commit(receives ? tr("Part %1 on").arg(partNumber) : tr("Part %1 off").arg(partNumber), [&](auto& p) {
        return p.setRaw(*part, PerformancePartParameter::ReceiveSwitch, receives ? 1 : 0);
    });
}

bool PerformanceViewModel::setPartMidiChannel(int partNumber, int channel)
{
    const auto part = PartIndex::fromNumber(partNumber);
    if (!part || channel < 1 || channel > 16) return false;
    return commit(tr("Part %1 MIDI channel").arg(partNumber), [&](auto& p) {
        return p.setRaw(*part, PerformancePartParameter::MidiChannel, channel - 1);
    });
}

bool PerformanceViewModel::setPartChorusSend(int partNumber, int level)
{
    const auto part = PartIndex::fromNumber(partNumber);
    if (!part) return false;
    return commit(tr("Part %1 chorus send").arg(partNumber), [&](auto& p) {
        return p.setRaw(*part, PerformancePartParameter::ChorusSendLevel, level);
    });
}

bool PerformanceViewModel::setPartReverbSend(int partNumber, int level)
{
    const auto part = PartIndex::fromNumber(partNumber);
    if (!part) return false;
    return commit(tr("Part %1 reverb send").arg(partNumber), [&](auto& p) {
        return p.setRaw(*part, PerformancePartParameter::ReverbSendLevel, level);
    });
}

bool PerformanceViewModel::setPartOutputAssign(int partNumber, int raw)
{
    const auto part = PartIndex::fromNumber(partNumber);
    if (!part) return false;
    return commit(tr("Part %1 output").arg(partNumber), [&](auto& p) {
        return p.setRaw(*part, PerformancePartParameter::OutputAssign, raw);
    });
}

bool PerformanceViewModel::setPartVoiceReserve(int partNumber, int voices)
{
    const auto part = PartIndex::fromNumber(partNumber);
    if (!part) return false;
    // Lives in Performance Common even though the mixer strip shows it.
    return commit(tr("Part %1 voice reserve").arg(partNumber),
                  [&](auto& p) { return p.setRaw(voiceReserveFor(*part), voices); });
}

bool PerformanceViewModel::setPartOctaveShift(int partNumber, int octaves)
{
    const auto part = PartIndex::fromNumber(partNumber);
    if (!part || octaves < -3 || octaves > 3) return false;
    return commit(tr("Part %1 octave").arg(partNumber), [&](auto& p) {
        return p.setRaw(*part, PerformancePartParameter::OctaveShift, octaves + 3);
    });
}

bool PerformanceViewModel::setPartKeyRange(int partNumber, int lowerRaw, int upperRaw)
{
    const auto part = PartIndex::fromNumber(partNumber);
    if (!part) return false;
    // Roland prints each bound in terms of the other. A crossing pair is
    // refused whole rather than half-applied or clamped to fit.
    if (lowerRaw > upperRaw) {
        return false;
    }
    return commit(tr("Part %1 key range").arg(partNumber), [&](auto& p) {
        return p.setRaw(*part, PerformancePartParameter::KeyboardRangeLower, lowerRaw)
            && p.setRaw(*part, PerformancePartParameter::KeyboardRangeUpper, upperRaw);
    });
}

bool PerformanceViewModel::setName(const QString& text)
{
    const auto renamed = xpmodel::PatchName::fromText(text.toStdString());
    if (!renamed) {
        return false;
    }
    return commit(tr("Rename Performance"), [&](auto& p) { return p.setName(*renamed); });
}

void PerformanceViewModel::undo()
{
    if (m_undo.empty()) {
        return;
    }
    m_redo.push_back(Step{*m_working, m_undo.back().label});
    m_working = m_undo.back().performance;
    m_undo.pop_back();
    emit changed();
}

void PerformanceViewModel::redo()
{
    if (m_redo.empty()) {
        return;
    }
    m_undo.push_back(Step{*m_working, m_redo.back().label});
    m_working = m_redo.back().performance;
    m_redo.pop_back();
    emit changed();
}

void PerformanceViewModel::revert()
{
    if (!m_baseline || !hasPerformance() || *m_working == *m_baseline) {
        return;
    }
    m_undo.push_back(Step{*m_working, tr("Revert")});
    m_redo.clear();
    m_working = *m_baseline;
    emit changed();
}

// ---------------------------------------------------------------------------
// The instrument
// ---------------------------------------------------------------------------

void PerformanceViewModel::adopt(const xpmodel::Xp60Performance& performance, const QString& sourceText)
{
    m_working = performance;
    m_baseline = performance;
    m_sourceText = sourceText;
    m_undo.clear();
    m_redo.clear();
    emit changed();
}

bool PerformanceViewModel::canFetch() const
{
    return m_session.connectionState() == services::DeviceSession::ConnectionState::Connected && !busy();
}

bool PerformanceViewModel::busy() const
{
    return m_sending
        || m_session.patchFetch().state == services::DeviceSession::PatchFetchState::InProgress;
}

bool PerformanceViewModel::canSend() const
{
    return hasPerformance() && canFetch();
}

bool PerformanceViewModel::fetchTemporary()
{
    if (!canFetch()) {
        return false;
    }
    setTransfer(tr("Reading the temporary Performance…"), QStringLiteral("info"));
    return m_session.fetchTemporaryPerformance();
}

bool PerformanceViewModel::fetchUser(int userNumber)
{
    const auto address = Xp60PerformanceLayout::userPerformanceAddress(userNumber);
    if (!address || !canFetch()) {
        return false;
    }
    setTransfer(tr("Reading USER:%1…").arg(userNumber, 2, 10, QLatin1Char('0')), QStringLiteral("info"));
    return m_session.fetchPerformance(*address);
}

void PerformanceViewModel::onFetchChanged()
{
    const auto& fetch = m_session.patchFetch();
    // A Patch fetch shares this signal and this slot; only a Performance one is
    // ours to adopt.
    if (fetch.kind != services::DeviceSession::FetchKind::Performance) {
        emit transferChanged();
        return;
    }
    switch (fetch.state) {
    case services::DeviceSession::PatchFetchState::InProgress:
        setTransfer(toQt(fetch.message), QStringLiteral("info"));
        break;
    case services::DeviceSession::PatchFetchState::Failed:
        setTransfer(toQt(fetch.message), QStringLiteral("error"));
        break;
    case services::DeviceSession::PatchFetchState::Completed:
        if (fetch.performance) {
            const bool temporary = fetch.base == Xp60PerformanceLayout::temporaryPerformanceAddress();
            adopt(*fetch.performance,
                  temporary ? tr("Read from the XP-60 temporary area")
                            : tr("Read from %1").arg(toQt(fetch.base.toHexString())));
            setTransfer(toQt(fetch.message), QStringLiteral("success"));
        }
        break;
    case services::DeviceSession::PatchFetchState::Idle:
        break;
    }
    emit transferChanged();
}

bool PerformanceViewModel::sendToTemporary()
{
    if (!canSend()) {
        return false;
    }
    // The temporary area only. A USER Performance is a persistent write and is
    // not offered here — see the class comment.
    const auto messages = xpmodel::Xp60PerformanceCodec::encodeToDataSets(
        *m_working, m_session.deviceId(), m_session.modelId(),
        Xp60PerformanceLayout::temporaryPerformanceAddress(),
        m_session.pacing().maxDataSetPayloadBytes);
    if (messages.empty()) {
        setTransfer(tr("The Performance could not be encoded for sending."), QStringLiteral("error"));
        return false;
    }
    const auto id = m_session.sendDataSets(messages);
    if (!id.isValid()) {
        setTransfer(tr("The XP-60 did not accept the transfer."), QStringLiteral("error"));
        return false;
    }
    m_sending = true;
    setTransfer(tr("Sending %n block(s) to the temporary Performance…", "", static_cast<int>(messages.size())),
                QStringLiteral("info"));
    return true;
}

void PerformanceViewModel::setTransfer(const QString& message, const QString& tone)
{
    m_transferMessage = message;
    m_transferTone = tone;
    emit transferChanged();
}

} // namespace xp60studio::presentation
