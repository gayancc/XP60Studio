#include "services/SnapshotCapture.h"

#include "xp60/Xp60Device.h"
#include "xpmodel/SysExStream.h"
#include "xpmodel/Xp60PatchLayout.h"
#include "xpmodel/Xp60PerformanceLayout.h"

namespace xp60studio::services {

using xpmodel::Xp60PatchLayout;
using xpmodel::Xp60PerformanceLayout;

namespace {

QString areaName(RestoreArea area)
{
    const auto name = restoreAreaName(area);
    return QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size()));
}

} // namespace

bool SnapshotCapture::isCapturable(RestoreArea area) noexcept
{
    // Only the two areas with a documented block layout and a fetch plan. The
    // others are refused by name rather than skipped; see the class comment.
    return area == RestoreArea::UserPatches || area == RestoreArea::UserPerformances;
}

std::vector<RestoreArea> SnapshotCapture::capturableAreas()
{
    return {RestoreArea::UserPerformances, RestoreArea::UserPatches};
}

int SnapshotCapture::slotCount(RestoreArea area) noexcept
{
    switch (area) {
    case RestoreArea::UserPatches:
        return 128;
    case RestoreArea::UserPerformances:
        return 32;
    case RestoreArea::UserRhythmSetups:
        return 2;
    case RestoreArea::System:
        return 1;
    }
    return 0;
}

SnapshotCapture::SnapshotCapture(DeviceSession& session, QObject* parent)
    : QObject(parent)
    , m_session(session)
{
    connect(&m_session, &DeviceSession::patchFetchChanged, this, &SnapshotCapture::onFetchChanged);
    connect(&m_session, &DeviceSession::connectionStateChanged, this, [this] {
        if (isBusy() && m_session.connectionState() != DeviceSession::ConnectionState::Connected) {
            // What arrived is kept: a partial capture is a true statement about
            // the slots it holds, and the restore plan will say it is partial.
            rebuildSnapshot();
            finish(State::Failed,
                   tr("Disconnected after reading %1 of %2 slot(s). What was read is kept, but this "
                      "is not a complete backup.")
                       .arg(m_completed)
                       .arg(totalSlots()));
        }
    });
}

std::string_view SnapshotCapture::stateName() const noexcept
{
    switch (m_state) {
    case State::Idle:
        return "Idle";
    case State::Reading:
        return "Reading";
    case State::Completed:
        return "Completed";
    case State::Failed:
        return "Failed";
    case State::Cancelled:
        return "Cancelled";
    }
    return "Unknown";
}

QString SnapshotCapture::currentSlotLabel() const
{
    if (!isBusy() || m_index >= m_slots.size()) {
        return {};
    }
    const auto& slot = m_slots[m_index];
    const int width = slot.area == RestoreArea::UserPatches ? 3 : 2;
    return QStringLiteral("%1 USER:%2")
        .arg(areaName(slot.area))
        .arg(slot.userNumber, width, 10, QLatin1Char('0'));
}

bool SnapshotCapture::capture(const std::vector<RestoreArea>& areas,
                              library::SnapshotMetadata metadata)
{
    if (isBusy() || areas.empty()) {
        return false;
    }
    if (m_session.connectionState() != DeviceSession::ConnectionState::Connected) {
        setState(State::Failed, tr("Not connected to an XP-60."));
        return false;
    }
    // Refuse the whole request if any part of it cannot be read. A snapshot
    // that quietly omits an area the user asked for is the false safety net
    // this whole workflow exists to avoid.
    for (const auto area : areas) {
        if (!isCapturable(area)) {
            setState(State::Failed,
                     tr("This build cannot read %1 — there is no block layout for that area yet — "
                        "so nothing was captured. Ask for the areas it can read instead.")
                         .arg(areaName(area)));
            return false;
        }
    }

    m_slots.clear();
    for (const auto area : areas) {
        for (int number = 1; number <= slotCount(area); ++number) {
            const auto base = area == RestoreArea::UserPatches
                ? Xp60PatchLayout::userPatchAddress(number)
                : Xp60PerformanceLayout::userPerformanceAddress(number);
            if (!base) {
                setState(State::Failed,
                         tr("%1 USER:%2 has no address; nothing was captured.")
                             .arg(areaName(area))
                             .arg(number));
                return false;
            }
            m_slots.push_back(Slot{area, number, *base});
        }
    }

    m_index = 0;
    m_completed = 0;
    m_cancelRequested = false;
    m_awaitingFetch = false;
    m_messages.clear();
    m_metadata = std::move(metadata);
    m_snapshot = {};
    beginNextSlot();
    return true;
}

void SnapshotCapture::beginNextSlot()
{
    if (m_cancelRequested) {
        rebuildSnapshot();
        finish(State::Cancelled,
               tr("Stopped after %1 of %2 slot(s). What was read is kept, but this is not a "
                  "complete backup.")
                   .arg(m_completed)
                   .arg(totalSlots()));
        return;
    }
    if (m_index >= m_slots.size()) {
        rebuildSnapshot();
        finish(State::Completed,
               tr("Read %1 slot(s); %2 message(s) captured.")
                   .arg(m_completed)
                   .arg(m_snapshot.messageCount()));
        return;
    }

    const auto& slot = m_slots[m_index];
    m_awaitingFetch = true;
    setState(State::Reading, tr("Reading %1").arg(currentSlotLabel()));
    if (!m_awaitingFetch) {
        return;
    }
    const bool started = slot.area == RestoreArea::UserPatches
        ? m_session.fetchPatch(slot.base, DeviceSession::PatchFetchPurpose::Transfer)
        : m_session.fetchPerformance(slot.base, DeviceSession::PatchFetchPurpose::Transfer);
    if (!started) {
        rebuildSnapshot();
        finish(State::Failed,
               tr("Could not start reading %1 after %2 of %3 slot(s).")
                   .arg(currentSlotLabel())
                   .arg(m_completed)
                   .arg(totalSlots()));
    }
}

void SnapshotCapture::onFetchChanged()
{
    if (!m_awaitingFetch) {
        return;
    }
    const auto& fetch = m_session.patchFetch();
    const auto& slot = m_slots[m_index];
    const auto expectedKind = slot.area == RestoreArea::UserPatches
        ? DeviceSession::FetchKind::Patch
        : DeviceSession::FetchKind::Performance;
    if (fetch.kind != expectedKind) {
        // Somebody else's fetch on the shared slot; not this run's business.
        return;
    }

    if (fetch.state == DeviceSession::PatchFetchState::Completed) {
        if (fetch.originalSysEx.empty()) {
            rebuildSnapshot();
            finish(State::Failed,
                   tr("%1 was read but the instrument's own bytes were not preserved, so it cannot "
                      "go into a snapshot.")
                       .arg(currentSlotLabel()));
            return;
        }
        // The bytes the instrument actually sent, not a re-encoding of the
        // decoded model. This is what makes the result a backup.
        m_messages.push_back(fetch.originalSysEx);
        m_awaitingFetch = false;
        ++m_completed;
        ++m_index;
        emit progressed(m_completed, totalSlots());
        beginNextSlot();
        return;
    }
    if (fetch.state == DeviceSession::PatchFetchState::Failed) {
        m_awaitingFetch = false;
        rebuildSnapshot();
        finish(State::Failed,
               tr("Could not read %1 after %2 of %3 slot(s): %4. What was read is kept, but this is "
                  "not a complete backup.")
                   .arg(currentSlotLabel())
                   .arg(m_completed)
                   .arg(totalSlots())
                   .arg(QString::fromStdString(fetch.message)));
    }
}

void SnapshotCapture::rebuildSnapshot()
{
    // Each fetch preserved a whole slot's worth of DT1s as one byte run, so the
    // snapshot is built by parsing the stream rather than by message.
    roland::ByteVector bytes;
    std::size_t total = 0;
    for (const auto& run : m_messages) {
        total += run.size();
    }
    bytes.reserve(total);
    for (const auto& run : m_messages) {
        bytes.insert(bytes.end(), run.begin(), run.end());
    }
    const std::vector<roland::RolandModelId> models{xp60::modelId()};
    m_snapshot = library::InstrumentSnapshot::fromSysEx(bytes, models, m_metadata);
}

void SnapshotCapture::cancel()
{
    if (!isBusy()) {
        return;
    }
    // Honoured between slots, never inside one: half a Patch in a snapshot is
    // a Patch that cannot be restored.
    m_cancelRequested = true;
    setState(m_state, tr("Stopping after the slot being read."));
}

void SnapshotCapture::setState(State state, QString message)
{
    m_state = state;
    m_message = std::move(message);
    emit changed();
}

void SnapshotCapture::finish(State state, QString message)
{
    m_awaitingFetch = false;
    setState(state, std::move(message));
    emit finished(state == State::Completed);
}

} // namespace xp60studio::services
