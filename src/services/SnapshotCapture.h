#pragma once

#include "library/InstrumentSnapshot.h"
#include "services/DeviceSession.h"
#include "services/RestorePlan.h"

#include <QObject>
#include <QString>

#include <cstddef>
#include <vector>

namespace xp60studio::services {

// Reads the instrument's user memory into an `InstrumentSnapshot`.
//
// The other half of the restore workflow, and the one that must exist before
// the destructive half is usable at all: `RestorePlan::requiresSafetySnapshot()`
// is true for every plan that writes anything, and this is what takes it.
//
// ── Read-only, and structurally so ──────────────────────────────────────────
//
// Every read goes through `DeviceSession::fetchPatch` / `fetchPerformance`,
// which issue RQ1 only. RQ1 cannot modify device memory, so nothing here can
// alter the instrument even if it is wrong. There is no send path.
//
// ── It preserves the instrument's own bytes ─────────────────────────────────
//
// Each slot's snapshot is the exact DT1 messages the XP-60 sent, taken from
// `PatchFetchStatus::originalSysEx`, not a re-encoding of the decoded model.
// That is what makes the result a backup rather than a recreation: it restores
// byte for byte, and a build that decodes more than this one does not
// invalidate it.
//
// ── What it can and cannot capture ──────────────────────────────────────────
//
// User Patches and User Performances have block layouts and fetch plans, so
// they are read slot by slot. **Rhythm Setups and System have neither yet**, so
// asking for them is refused by name rather than quietly skipped — a snapshot
// silently missing an area is exactly the false safety net the restore plan
// exists to prevent.
//
// ── Why it takes as long as it does ─────────────────────────────────────────
//
// A whole User Patch bank is 128 slots of five blocks read one at a time, which
// the XP-60's own behaviour requires (`ROLAND_XP60_PROTOCOL_FACTS.md` §2.3,
// hardware-verified): roughly 640 block reads at about 53 ms each, so a couple
// of minutes. That is MIDI at 31250 baud, not this code, which is why the
// operation reports progress and can be stopped.
class SnapshotCapture : public QObject
{
    Q_OBJECT

public:
    enum class State { Idle, Reading, Completed, Failed, Cancelled };

    // The areas this build can read. Asking for anything else is refused.
    [[nodiscard]] static bool isCapturable(RestoreArea area) noexcept;
    [[nodiscard]] static std::vector<RestoreArea> capturableAreas();
    // How many slots an area has, so a caller can size a progress bar before
    // the first read.
    [[nodiscard]] static int slotCount(RestoreArea area) noexcept;

    explicit SnapshotCapture(DeviceSession& session, QObject* parent = nullptr);

    [[nodiscard]] State state() const noexcept { return m_state; }
    [[nodiscard]] bool isBusy() const noexcept { return m_state == State::Reading; }
    [[nodiscard]] QString message() const { return m_message; }
    [[nodiscard]] std::string_view stateName() const noexcept;

    // Starts a read of the given areas, in the order given. False when refused:
    // not connected, already busy, no areas, or an area this build cannot read.
    bool capture(const std::vector<RestoreArea>& areas,
                 library::SnapshotMetadata metadata = library::SnapshotMetadata());
    void cancel();

    // Progress ----------------------------------------------------------------
    [[nodiscard]] std::size_t totalSlots() const noexcept { return m_slots.size(); }
    [[nodiscard]] std::size_t completedSlots() const noexcept { return m_completed; }
    [[nodiscard]] QString currentSlotLabel() const;

    // The capture, once the run has ended — completed, cancelled or failed. A
    // run that ended early holds the slots that did arrive, which is a true
    // partial capture and reports itself as partial through `RestorePlan`.
    // Empty while a run is still going: the snapshot is built when the run
    // ends, not after every slot, so a 160-slot capture does not re-parse
    // everything it holds 160 times.
    [[nodiscard]] const library::InstrumentSnapshot& snapshot() const noexcept { return m_snapshot; }

Q_SIGNALS:
    void changed();
    void progressed(std::size_t completed, std::size_t total);
    void finished(bool ok);

private:
    struct Slot
    {
        RestoreArea area;
        int userNumber = 0;
        roland::RolandAddress base;
    };

    void beginNextSlot();
    void finish(State state, QString message);
    void setState(State state, QString message);
    void onFetchChanged();
    void rebuildSnapshot();

    DeviceSession& m_session;
    State m_state = State::Idle;
    QString m_message;
    bool m_cancelRequested = false;
    bool m_awaitingFetch = false;

    std::vector<Slot> m_slots;
    std::size_t m_index = 0;
    std::size_t m_completed = 0;
    std::vector<roland::ByteVector> m_messages;
    library::SnapshotMetadata m_metadata;
    library::InstrumentSnapshot m_snapshot;
};

} // namespace xp60studio::services
