#pragma once

#include "services/PatchSyncState.h"
#include "xpmodel/Xp60Patch.h"

#include <QObject>
#include <QString>

#include <cstdint>
#include <deque>
#include <optional>
#include <string>

namespace xp60studio::services {

// The one Patch XP60Studio is working on, and everything that is true about it.
//
// Before this class existed the Patch Editor owned three copies of a Patch
// (original / current / hardware) plus its own undo history, the Library cached
// patch names in its rows, and the Bank Builder cached them again in its
// destinations. Nothing connected them, so a rename in the Editor could not
// reach a bank tile showing the same Patch — and the three screens only avoided
// disagreeing because there was no way to open the same Patch in more than one
// of them.
//
// The workspace is the fix: a single owner that every view projects. Views hold
// no Patch of their own. They read from here and re-read when `changed()` says
// to, and they know whether a row or a destination is *this* Patch by comparing
// `origin()`. That is the entire propagation mechanism — no signal graph between
// screens, no cache-invalidation protocol.
//
// It lives in services/ rather than presentation/ because it is orchestration,
// not projection: the rules it enforces are about the XP-60's memory
// architecture, and they must be testable without QML.
//
// ── What it deliberately does not do ────────────────────────────────────────
//
// It never transmits. Editing here changes nothing on the instrument; reaching
// the XP-60 is `PatchTransfer`'s job and is always an explicit action. It has no
// path to permanent USER memory at all — see docs/PATCH_SYNCHRONIZATION.md §7.
//
// ── The memory model it encodes ─────────────────────────────────────────────
//
// The XP-60 makes sound from its *temporary area*, and modifying a Patch on the
// instrument modifies that area rather than stored memory. The area is lost when
// the power goes off **or when another Patch is selected** (Owner's Manual
// p.45). Two consequences drive this class:
//
//   * auditioning by writing the temporary area is safe and repeatable — it
//     cannot damage a stored sound;
//   * no claim about the instrument survives the musician pressing a Patch
//     button, which is why `markStale()` exists and why `DeviceState::Assumed`
//     is not allowed to masquerade as synchronized.
class PatchWorkspace : public QObject
{
    Q_OBJECT

public:
    // How deep local editing history goes. A bounded deque of whole Patches:
    // a Patch is ~600 bytes, so keeping snapshots is simpler and safer than
    // replaying inverse operations, and makes a compound edit one step by
    // construction.
    static constexpr std::size_t kUndoLimit = 64;

    explicit PatchWorkspace(QObject* parent = nullptr);

    // ── The working Patch ───────────────────────────────────────────────────
    [[nodiscard]] bool hasPatch() const noexcept { return m_working.has_value(); }
    // Precondition: hasPatch(). Callers in the UI check first.
    [[nodiscard]] const xpmodel::Xp60Patch& working() const { return *m_working; }
    // The Patch as it was adopted — the A side of an A/B comparison, and what
    // `revert()` returns to.
    [[nodiscard]] const std::optional<xpmodel::Xp60Patch>& baseline() const noexcept { return m_baseline; }
    // What the XP-60's temporary area last held according to a **verified**
    // read-back, or nullopt when nothing has been verified this session.
    [[nodiscard]] const std::optional<xpmodel::Xp60Patch>& deviceBaseline() const noexcept
    {
        return m_deviceBaseline;
    }

    [[nodiscard]] PatchOrigin origin() const noexcept { return m_origin; }
    [[nodiscard]] QString displayName() const;

    // Adopts a Patch as the new working Patch, replacing whatever was there.
    // Editing history is cleared, because the history of a different Patch is
    // not history of this one. `baseline` becomes both the A side and, for a
    // library origin, the saved comparison.
    void adopt(const xpmodel::Xp60Patch& patch, PatchOrigin origin);
    // Forgets the working Patch entirely.
    void clear();

    // ── Editing ─────────────────────────────────────────────────────────────
    // Applies `mutate` to the working Patch as one undoable step. `label` is
    // what the UI shows for that step. Returns false when there is no working
    // Patch, or when the mutation left it unchanged — an edit that changes
    // nothing must not consume an undo step or mark the Patch dirty.
    //
    // Grouped so that a compound edit (Wave Group Type + Group ID + Number move
    // together, or a whole envelope drag) is one step: pass everything the
    // gesture changes in a single call.
    template <typename Mutation>
    bool edit(const QString& label, Mutation&& mutate)
    {
        if (!m_working) {
            return false;
        }
        auto candidate = *m_working;
        mutate(candidate);
        return commit(std::move(candidate), label);
    }

    // Replaces the working Patch wholesale as one undoable step. Same rules as
    // edit(): no change means no step.
    bool commit(xpmodel::Xp60Patch patch, const QString& label);
    // Applies `patch` without recording history and without marking the Patch
    // edited. For a gesture already in progress whose first move pushed the undo
    // step — see `beginGesture()`.
    bool amend(xpmodel::Xp60Patch patch);

    // A gesture is a run of edits the user experiences as one: a knob drag, an
    // envelope point being pulled. The first commit inside a gesture records an
    // undo step; the rest amend it, so undo takes the whole drag back rather
    // than one pixel of it.
    void beginGesture();
    void endGesture();
    [[nodiscard]] bool gestureActive() const noexcept { return m_gesture; }

    [[nodiscard]] bool canUndo() const noexcept { return !m_undo.empty(); }
    [[nodiscard]] bool canRedo() const noexcept { return !m_redo.empty(); }
    [[nodiscard]] QString undoLabel() const;
    [[nodiscard]] QString redoLabel() const;
    bool undo();
    bool redo();
    // Back to the adopted Patch, as one undoable step.
    bool revert();

    // ── Studio storage ──────────────────────────────────────────────────────
    [[nodiscard]] StudioState studioState() const noexcept;
    // True when the working Patch differs from what was adopted.
    [[nodiscard]] bool modified() const;
    // Records that the working Patch is now what library row `libraryId` holds:
    // the baseline moves to the working Patch and the origin becomes that row.
    // Called after the library write succeeds, never before.
    void markSaved(std::int64_t libraryId);

    // ── XP-60 temporary memory ──────────────────────────────────────────────
    [[nodiscard]] DeviceState deviceState() const noexcept { return m_deviceState; }
    [[nodiscard]] QString deviceMessage() const { return m_deviceMessage; }
    // What a send should diff against. While `Assumed` this is what was last
    // *sent* rather than last read: the transport reported the batch delivered,
    // which is enough to build the next diff on and is exactly why the state is
    // not called synchronized. Nullopt means send the whole Patch.
    [[nodiscard]] const std::optional<xpmodel::Xp60Patch>& sendBaseline() const noexcept;

    void setConnected(bool connected);
    [[nodiscard]] bool connected() const noexcept { return m_connected; }

    // Transfer reporting. These are the only ways the device state moves
    // forward; nothing infers it from UI selection.
    void noteSending();
    // The transport accepted the batch. Not a claim of synchronization.
    void noteSent(const xpmodel::Xp60Patch& sent);
    // A read-back compared equal to `verified`.
    void noteVerified(const xpmodel::Xp60Patch& verified);
    void noteTransferFailed(const QString& reason);
    // The instrument's temporary area is believed replaced or unknowable.
    // Absorbing until a fresh send or read re-establishes ground truth.
    void markStale(const QString& reason);

Q_SIGNALS:
    // The working Patch, its origin, or its history changed. One signal on
    // purpose: views re-read what they display rather than tracking which of a
    // dozen properties moved.
    void changed();
    // Only the sync status moved; the Patch itself did not.
    void syncChanged();
    // A different Patch is now being worked on (adopt/clear). Views that key off
    // identity — a Library row's highlight, a Bank tile's overlay — use this.
    void originChanged();

private:
    void pushUndo(const QString& label);
    void afterPatchChanged();

    struct Step
    {
        xpmodel::Xp60Patch patch;
        QString label;
    };

    std::optional<xpmodel::Xp60Patch> m_working;
    std::optional<xpmodel::Xp60Patch> m_baseline;
    std::optional<xpmodel::Xp60Patch> m_deviceBaseline; // last verified
    std::optional<xpmodel::Xp60Patch> m_lastSent;       // last transmitted, unverified
    PatchOrigin m_origin;

    std::deque<Step> m_undo;
    std::deque<Step> m_redo;
    bool m_gesture = false;
    bool m_gestureHasUndo = false;

    bool m_connected = false;
    DeviceState m_deviceState = DeviceState::Offline;
    QString m_deviceMessage;
};

} // namespace xp60studio::services
