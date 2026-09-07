#include "services/LiveSession.h"

#include "xpmodel/Xp60PatchLayout.h"

#include <utility>

namespace xp60studio::services {

using library::LiveTarget;
using library::LiveTargetKind;
using library::SetlistCue;

namespace {

// A Patch is five blocks, read one at a time (ROLAND_XP60_PROTOCOL_FACTS.md
// §2.3). Stated as a count rather than a duration: the seconds depend on the
// link, the count does not.
constexpr int kPatchBlocks = 5;

} // namespace

std::string_view liveBlockName(LiveBlock block) noexcept
{
    switch (block) {
    case LiveBlock::None:
        return "None";
    case LiveBlock::NothingToDo:
        return "NothingToDo";
    case LiveBlock::NoSetlist:
        return "NoSetlist";
    case LiveBlock::OutOfRange:
        return "OutOfRange";
    case LiveBlock::NotConnected:
        return "NotConnected";
    case LiveBlock::Busy:
        return "Busy";
    case LiveBlock::NotArmed:
        return "NotArmed";
    case LiveBlock::PatchMissing:
        return "PatchMissing";
    case LiveBlock::PatchUnreadable:
        return "PatchUnreadable";
    case LiveBlock::PerformanceNotSupported:
        return "PerformanceNotSupported";
    }
    return "Unknown";
}

LiveSession::LiveSession(DeviceSession& session, PatchTransfer& transfer, QObject* parent)
    : QObject(parent)
    , m_session(session)
    , m_transfer(transfer)
{
    connect(&m_transfer, &PatchTransfer::changed, this, &LiveSession::onTransferChanged);
    connect(&m_session, &DeviceSession::patchFetchChanged, this, &LiveSession::onFetchChanged);
}

void LiveSession::setLibrary(library::LibraryDatabase* library) { m_library = library; }

// ---------------------------------------------------------------------------
// The running order
// ---------------------------------------------------------------------------

void LiveSession::load(library::Setlist setlist)
{
    m_setlist = std::move(setlist);
    rebuild();
    // Loading does not start playing. The cursor sits before the first cue so
    // the instrument keeps whatever it is holding until somebody says go.
    m_position = -1;
    m_sounding = -1;
    m_message.clear();
    emit setlistChanged();
    emit positionChanged();
}

void LiveSession::clear()
{
    m_setlist.reset();
    m_cues.clear();
    m_position = -1;
    m_sounding = -1;
    m_message.clear();
    emit setlistChanged();
    emit positionChanged();
}

void LiveSession::rebuild() { m_cues = m_setlist ? m_setlist->cues() : std::vector<SetlistCue>{}; }

std::optional<SetlistCue> LiveSession::currentCue() const
{
    if (m_position < 0 || m_position >= cueCount()) {
        return std::nullopt;
    }
    return m_cues[static_cast<std::size_t>(m_position)];
}

std::optional<SetlistCue> LiveSession::nextCue() const
{
    const int next = m_position + 1;
    if (next < 0 || next >= cueCount()) {
        return std::nullopt;
    }
    return m_cues[static_cast<std::size_t>(next)];
}

bool LiveSession::canGoNext() const noexcept { return m_position + 1 < cueCount(); }
bool LiveSession::canGoPrevious() const noexcept { return m_position > 0; }

bool LiveSession::goToIndex(int index)
{
    if (index < 0 || index >= cueCount() || index == m_position) {
        return false;
    }
    m_position = index;
    emit positionChanged();
    return true;
}

bool LiveSession::goNext() { return canGoNext() && goToIndex(m_position + 1); }
bool LiveSession::goPrevious() { return canGoPrevious() && goToIndex(m_position - 1); }

void LiveSession::rewind()
{
    if (m_position == -1) {
        return;
    }
    m_position = -1;
    emit positionChanged();
}

// ---------------------------------------------------------------------------
// Switching
// ---------------------------------------------------------------------------

LiveSwitchPlan LiveSession::planFor(int index) const
{
    LiveSwitchPlan plan;
    auto refuse = [&plan](LiveBlock block, QString reason) {
        plan.block = block;
        plan.reason = std::move(reason);
        return plan;
    };

    if (!m_setlist) {
        return refuse(LiveBlock::NoSetlist, tr("No setlist is loaded."));
    }
    if (index < 0 || index >= cueCount()) {
        return refuse(LiveBlock::OutOfRange, tr("There is no cue at that position."));
    }
    const auto& cue = m_cues[static_cast<std::size_t>(index)];
    const auto& target = cue.target;

    if (!target.selectsSomething()) {
        // Not a failure. The section is written to keep the sound, so there is
        // nothing to send, and saying "cannot" about it would be wrong.
        return refuse(LiveBlock::NothingToDo,
                      tr("%1 keeps the previous sound.").arg(cue.label()));
    }

    switch (target.kind) {
    case LiveTargetKind::UserPerformanceSlot:
        return refuse(LiveBlock::PerformanceNotSupported,
                      tr("%1 calls for a Performance. Performance cues can be written down and "
                         "moved through, but this build cannot switch to one.")
                          .arg(cue.label()));
    case LiveTargetKind::LibraryPatch:
        if (target.isMissing()) {
            return refuse(LiveBlock::PatchMissing,
                          tr("\"%1\" is no longer in the library.")
                              .arg(QString::fromStdString(target.name)));
        }
        if (m_library == nullptr || !m_library->isOpen()) {
            return refuse(LiveBlock::PatchMissing, tr("The library is not open."));
        }
        plan.blocksToRead = 0;  // the bytes are already held
        break;
    case LiveTargetKind::UserPatchSlot:
        if (!xpmodel::Xp60PatchLayout::userPatchAddress(target.userNumber)) {
            return refuse(LiveBlock::OutOfRange,
                          tr("USER:%1 is not a Patch slot the XP-60 has.").arg(target.userNumber));
        }
        // Read the slot, then write what came back into the temporary area.
        plan.blocksToRead = kPatchBlocks;
        break;
    case LiveTargetKind::CarryPrevious:
        break;  // handled above
    }
    plan.blocksToWrite = kPatchBlocks;
    plan.headline = tr("%1 → temporary area").arg(target.describe());

    // Device conditions last, so a screen showing the plan while offline still
    // says what the cue *is* before it says why it cannot happen yet.
    if (m_switching || m_awaitingSlotRead
        || m_session.patchFetch().state == DeviceSession::PatchFetchState::InProgress) {
        return refuse(LiveBlock::Busy, tr("Another transfer is still running."));
    }
    if (m_session.connectionState() != DeviceSession::ConnectionState::Connected) {
        return refuse(LiveBlock::NotConnected, tr("The XP-60 is not connected."));
    }
    // Arming is single-use, which is right for a one-off write and wrong for a
    // show: re-arming between every bar is not an armed write, it is an
    // unguarded one with an extra tap. So Live Mode uses PatchTransfer's
    // session-scoped live preview instead — armed once when the session starts,
    // and open until it is stopped. Before that, the arming is what is missing.
    if (!m_transfer.liveActive() && !m_transfer.isArmed()) {
        return refuse(LiveBlock::NotArmed,
                      tr("Writing to the XP-60 is not armed. Arm once to start the live session."));
    }
    return plan;
}

std::optional<xpmodel::Xp60Patch> LiveSession::patchForLibraryCue(const LiveTarget& target,
                                                                  QString& error) const
{
    if (m_library == nullptr || !m_library->isOpen()) {
        error = tr("The library is not open.");
        return std::nullopt;
    }
    auto entry = m_library->loadEntry(target.libraryId);
    if (!entry) {
        error = tr("\"%1\" could not be read from the library.")
                    .arg(QString::fromStdString(target.name));
        return std::nullopt;
    }
    return entry->patch();
}

bool LiveSession::sendPatch(const xpmodel::Xp60Patch& patch)
{
    m_lastSent = patch;
    if (m_transfer.liveActive()) {
        // Already in a live session: this is the next cue. Coalescing is the
        // right behaviour here too — two advances in quick succession should
        // land on the second sound, not play the first on the way past.
        m_transfer.queueLivePreview(patch);
        return true;
    }
    if (!m_transfer.startLivePreview(patch)) {
        finishSwitch(false, tr("The XP-60 did not accept the transfer."));
        return false;
    }
    return true;
}

void LiveSession::endLiveWrites()
{
    // `m_lastSent` is always set when a live session is open: it is assigned
    // before the call that opens one.
    if (!m_transfer.liveActive() || !m_lastSent) {
        return;
    }
    // Leaving the session verifies what is actually on the instrument rather
    // than ending on an unproved send.
    m_transfer.stopLivePreview(*m_lastSent);
}

bool LiveSession::goToCurrent()
{
    const auto plan = planForCurrent();
    if (!plan.possible()) {
        m_message = plan.reason;
        if (plan.block == LiveBlock::NothingToDo) {
            // The cue is satisfied by doing nothing, so the sound that is
            // playing is now this cue's sound. Recording that keeps the display
            // honest instead of leaving it pointing at an earlier section.
            m_sounding = m_position;
            emit positionChanged();
        }
        return false;
    }

    const auto cue = currentCue();
    m_switching = true;
    m_switchingIndex = m_position;
    m_message = tr("Switching to %1…").arg(cue->target.describe());
    emit switchingChanged();

    if (cue->target.kind == LiveTargetKind::UserPatchSlot) {
        const auto address = xpmodel::Xp60PatchLayout::userPatchAddress(cue->target.userNumber);
        m_awaitingSlotRead = true;
        if (!m_session.fetchPatch(*address, DeviceSession::PatchFetchPurpose::Transfer)) {
            m_awaitingSlotRead = false;
            finishSwitch(false, tr("The XP-60 did not accept the read of USER:%1.")
                                    .arg(cue->target.userNumber));
            return false;
        }
        return true;
    }

    QString error;
    const auto patch = patchForLibraryCue(cue->target, error);
    if (!patch) {
        finishSwitch(false, error);
        return false;
    }
    return sendPatch(*patch);
}

bool LiveSession::advance()
{
    if (!goNext()) {
        return false;
    }
    return goToCurrent();
}

void LiveSession::onFetchChanged()
{
    if (!m_awaitingSlotRead) {
        return;
    }
    const auto& fetch = m_session.patchFetch();
    if (fetch.state == DeviceSession::PatchFetchState::InProgress) {
        return;
    }
    m_awaitingSlotRead = false;
    if (fetch.state != DeviceSession::PatchFetchState::Completed || !fetch.patch) {
        finishSwitch(false, fetch.message.empty()
                                ? tr("The USER slot could not be read.")
                                : QString::fromStdString(fetch.message));
        return;
    }
    // What came back from the slot goes into the temporary area unchanged. The
    // instrument's own bytes, not a re-encoding of a decode of them.
    sendPatch(*fetch.patch);
}

void LiveSession::onTransferChanged()
{
    if (!m_switching || m_awaitingSlotRead) {
        return;
    }
    const auto describeSwitching = [this] {
        return m_cues[static_cast<std::size_t>(m_switchingIndex)].target.describe();
    };
    switch (m_transfer.state()) {
    case PatchTransfer::State::Sent:
        // The bytes are out and the transport took them; the instrument is
        // making the new sound. Read-back happens once the musician stops
        // moving, so this is reported as sent, never as verified — the state
        // name is the honest one and the screen can show it.
        finishSwitch(true, tr("Playing %1 (sent, not yet read back).").arg(describeSwitching()));
        break;
    case PatchTransfer::State::Verified:
        finishSwitch(true, tr("Playing %1.").arg(describeSwitching()));
        break;
    case PatchTransfer::State::Mismatch:
        // The write did not take. Reported, never rounded up to success: on
        // stage the difference is audible.
        finishSwitch(false, tr("The XP-60 did not hold what was sent."));
        break;
    case PatchTransfer::State::Failed:
    case PatchTransfer::State::Cancelled:
        finishSwitch(false, QString::fromStdString(m_transfer.message()));
        break;
    default:
        break;
    }
}

void LiveSession::finishSwitch(bool ok, QString message)
{
    const int index = m_switchingIndex;
    m_switching = false;
    m_switchingIndex = -1;
    m_awaitingSlotRead = false;
    m_message = std::move(message);
    if (ok) {
        m_sounding = index;
    }
    emit switchingChanged();
    emit switchFinished(index, ok);
}

} // namespace xp60studio::services
