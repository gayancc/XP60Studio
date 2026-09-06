#include "services/PatchWorkspace.h"

namespace xp60studio::services {

PatchWorkspace::PatchWorkspace(QObject* parent)
    : QObject(parent)
{
}

QString PatchWorkspace::displayName() const
{
    if (!m_working) {
        return {};
    }
    return QString::fromStdString(m_working->name().displayText());
}

void PatchWorkspace::adopt(const xpmodel::Xp60Patch& patch, PatchOrigin origin)
{
    m_working = patch;
    m_baseline = patch;
    m_origin = origin;
    m_undo.clear();
    m_redo.clear();
    m_gesture = false;
    m_gestureHasUndo = false;
    m_gestureBase.reset();
    m_gestureRedoBefore.clear();

    // A different Patch is a different question about the instrument. Whatever
    // was verified about the previous one says nothing about this one, so the
    // device state starts over rather than being inherited.
    m_deviceBaseline.reset();
    m_lastSent.reset();
    m_deviceState = m_connected ? DeviceState::NotSent : DeviceState::Offline;
    m_deviceMessage.clear();

    emit originChanged();
    emit syncChanged();
    emit changed();
}

void PatchWorkspace::clear()
{
    m_working.reset();
    m_baseline.reset();
    m_deviceBaseline.reset();
    m_lastSent.reset();
    m_origin = {};
    m_undo.clear();
    m_redo.clear();
    m_gesture = false;
    m_gestureHasUndo = false;
    m_gestureBase.reset();
    m_gestureRedoBefore.clear();
    m_deviceState = m_connected ? DeviceState::NotSent : DeviceState::Offline;
    m_deviceMessage.clear();

    emit originChanged();
    emit syncChanged();
    emit changed();
}

// ---------------------------------------------------------------------------
// Editing
// ---------------------------------------------------------------------------

bool PatchWorkspace::commit(xpmodel::Xp60Patch patch, const QString& label)
{
    if (!m_working) {
        return false;
    }
    if (patch == *m_working) {
        // An edit that changes nothing must not consume an undo step, mark the
        // Patch dirty, or diverge it from the instrument.
        return false;
    }
    if (m_gesture && m_gestureHasUndo) {
        m_working = std::move(patch);
        afterPatchChanged();
        return true;
    }
    pushUndo(label);
    if (m_gesture) {
        m_gestureHasUndo = true;
    }
    m_working = std::move(patch);
    afterPatchChanged();
    return true;
}

bool PatchWorkspace::amend(xpmodel::Xp60Patch patch)
{
    if (!m_working || patch == *m_working) {
        return false;
    }
    m_working = std::move(patch);
    afterPatchChanged();
    return true;
}

void PatchWorkspace::beginGesture()
{
    if (m_gesture) return;
    m_gesture = true;
    m_gestureHasUndo = false;
    m_gestureBase = m_working;
    m_gestureRedoBefore = m_redo;
}

void PatchWorkspace::endGesture()
{
    const bool returnedToOrigin = m_gesture && m_gestureHasUndo && m_working && m_gestureBase
        && *m_working == *m_gestureBase;
    if (returnedToOrigin) {
        // A drag that finishes where it started is not an edit. Remove the
        // provisional undo step and restore the redo future that the first
        // movement temporarily displaced.
        if (!m_undo.empty()) m_undo.pop_back();
        m_redo = m_gestureRedoBefore;
    }
    m_gesture = false;
    m_gestureHasUndo = false;
    m_gestureBase.reset();
    m_gestureRedoBefore.clear();
    if (returnedToOrigin) emit changed();
}

void PatchWorkspace::pushUndo(const QString& label)
{
    if (!m_working) {
        return;
    }
    m_undo.push_back(Step{*m_working, label});
    while (m_undo.size() > kUndoLimit) {
        m_undo.pop_front();
    }
    // A new edit ends the redo branch: the future it described no longer
    // follows from the present.
    m_redo.clear();
}

void PatchWorkspace::afterPatchChanged()
{
    // Any local change means the instrument no longer holds what is on screen.
    // Offline and Stale are left alone: neither becomes truer by editing, and
    // overwriting Stale would quietly discard the fact that the temporary area
    // is unknowable.
    if (m_deviceState == DeviceState::Assumed || m_deviceState == DeviceState::InSync
        || m_deviceState == DeviceState::Sending) {
        m_deviceState = DeviceState::Diverged;
        m_deviceMessage.clear();
        emit syncChanged();
    }
    emit changed();
}

QString PatchWorkspace::undoLabel() const
{
    return m_undo.empty() ? QString() : m_undo.back().label;
}

QString PatchWorkspace::redoLabel() const
{
    return m_redo.empty() ? QString() : m_redo.back().label;
}

bool PatchWorkspace::undo()
{
    if (m_undo.empty() || !m_working) {
        return false;
    }
    endGesture();
    auto step = std::move(m_undo.back());
    m_undo.pop_back();
    m_redo.push_back(Step{*m_working, step.label});
    m_working = std::move(step.patch);
    afterPatchChanged();
    return true;
}

bool PatchWorkspace::redo()
{
    if (m_redo.empty() || !m_working) {
        return false;
    }
    endGesture();
    auto step = std::move(m_redo.back());
    m_redo.pop_back();
    m_undo.push_back(Step{*m_working, step.label});
    m_working = std::move(step.patch);
    afterPatchChanged();
    return true;
}

bool PatchWorkspace::revert()
{
    if (!m_working || !m_baseline) {
        return false;
    }
    endGesture();
    return commit(*m_baseline, QObject::tr("Discard changes"));
}

// ---------------------------------------------------------------------------
// Studio storage
// ---------------------------------------------------------------------------

bool PatchWorkspace::modified() const
{
    if (!m_working || !m_baseline) {
        return false;
    }
    return !(*m_working == *m_baseline);
}

StudioState PatchWorkspace::studioState() const noexcept
{
    // Unkept work outranks provenance. A Patch read from the instrument has no
    // library row, but once it has been edited the urgent thing to say is that
    // the changes are held nowhere — not that it was never in the library.
    if (modified()) {
        return StudioState::Edited;
    }
    // Unmodified: now the question is whether anything backs it at all. Saying
    // "saved" for a Patch with no library row would name a row that does not
    // exist.
    return m_origin.kind == PatchOrigin::Kind::LibraryEntry ? StudioState::Saved : StudioState::Untracked;
}

void PatchWorkspace::markSaved(std::int64_t libraryId)
{
    if (!m_working || libraryId <= 0) {
        return;
    }
    m_baseline = m_working;
    m_origin = PatchOrigin::library(libraryId);
    emit originChanged();
    emit changed();
}

// ---------------------------------------------------------------------------
// XP-60 temporary memory
// ---------------------------------------------------------------------------

const std::optional<xpmodel::Xp60Patch>& PatchWorkspace::sendBaseline() const noexcept
{
    // Prefer what was verified; fall back to what was sent. Both are better
    // than sending 644 bytes for a one-byte change, and neither is used to
    // claim synchronization.
    if (m_deviceState == DeviceState::Stale || m_deviceState == DeviceState::Failed) {
        // The temporary area is unknowable, so a diff against it would be a
        // guess. The caller sends the whole Patch instead.
        static const std::optional<xpmodel::Xp60Patch> none;
        return none;
    }
    return m_deviceBaseline ? m_deviceBaseline : m_lastSent;
}

void PatchWorkspace::setConnected(bool connected)
{
    if (m_connected == connected) {
        return;
    }
    m_connected = connected;
    if (!connected) {
        m_deviceState = DeviceState::Offline;
        m_deviceMessage.clear();
        m_deviceBaseline.reset();
        m_lastSent.reset();
    } else {
        // A connection appearing proves nothing about the temporary area. It is
        // not pushed to, and it is not assumed to match — the user re-sends or
        // re-reads deliberately.
        m_deviceState = m_working ? DeviceState::NotSent : DeviceState::NotSent;
        m_deviceMessage.clear();
    }
    emit syncChanged();
}

void PatchWorkspace::noteSending()
{
    if (!m_connected) {
        return;
    }
    m_deviceState = DeviceState::Sending;
    m_deviceMessage.clear();
    emit syncChanged();
}

void PatchWorkspace::noteSent(const xpmodel::Xp60Patch& sent)
{
    if (!m_connected) {
        return;
    }
    m_lastSent = sent;
    // Only claim as much as is true: the bytes left, nothing was read back.
    m_deviceState = (m_working && *m_working == sent) ? DeviceState::Assumed : DeviceState::Diverged;
    m_deviceMessage.clear();
    emit syncChanged();
}

void PatchWorkspace::noteVerified(const xpmodel::Xp60Patch& verified)
{
    if (!m_connected) {
        return;
    }
    m_deviceBaseline = verified;
    m_lastSent = verified;
    m_deviceState = (m_working && *m_working == verified) ? DeviceState::InSync : DeviceState::Diverged;
    m_deviceMessage.clear();
    emit syncChanged();
}

void PatchWorkspace::noteTransferFailed(const QString& reason)
{
    m_deviceState = DeviceState::Failed;
    m_deviceMessage = reason;
    // A failed transfer may have written part of the temporary area, so what it
    // holds is no longer known. Dropping the baselines forces the next send to
    // be a whole Patch rather than a diff against a fiction.
    m_deviceBaseline.reset();
    m_lastSent.reset();
    emit syncChanged();
}

void PatchWorkspace::markStale(const QString& reason)
{
    if (!m_connected) {
        return;
    }
    m_deviceState = DeviceState::Stale;
    m_deviceMessage = reason;
    m_deviceBaseline.reset();
    m_lastSent.reset();
    emit syncChanged();
}

} // namespace xp60studio::services
