#pragma once

#include "services/DeviceSession.h"
#include "xpmodel/Xp60Patch.h"

#include <QObject>
#include <QString>

#include <cstddef>
#include <vector>

namespace xp60studio::services {

// Reads Patches out of the XP-60's permanent USER bank (`11 nn 00 00`).
//
// The counterpart to `UserMemoryWrite`, and the thing that makes writing safe to
// offer at all: before a musician replaces what is on their keyboard, they can
// take the whole 128-Patch bank into the library, where it can be searched,
// arranged, exported as a `.syx` and — if the write turns out to have been a
// mistake — written straight back.
//
// It answers two of the questions `AGENTS.md` sets as the product standard:
// *"Did all 128 patches actually reach the keyboard?"* and *"Can I restore
// exactly what was on the XP-60 before this change?"*
//
// ── Read-only, and structurally so ──────────────────────────────────────────
//
// This class issues RQ1 only. RQ1 cannot modify device memory, so nothing here
// can alter the instrument even if it is wrong. It has no send path at all.
//
// ── Why it reads one slot at a time ─────────────────────────────────────────
//
// The XP-60 drops requests that arrive while it is still transmitting a reply
// (`ROLAND_XP60_PROTOCOL_FACTS.md` §2.3, hardware-verified), so block reads are
// serialised by `DeviceSession::fetchPatch` and whole Patches are serialised
// here. A full bank is 128 Patches of five blocks: around 640 block reads at
// roughly 53 ms each, so a whole-bank read takes a couple of minutes. That is a
// property of MIDI at 31250 baud, not of this code, which is why the operation
// reports progress and can be stopped.
//
// ── What it preserves ───────────────────────────────────────────────────────
//
// Each slot keeps the exact DT1 bytes the instrument sent alongside the decoded
// Patch, so a Patch stored from a device read carries the bytes that actually
// arrived rather than a re-encoding of them.
class UserBankRead : public QObject
{
    Q_OBJECT

public:
    enum class State { Idle, Reading, Completed, Failed, Cancelled };

    struct Slot
    {
        int userNumber = 0; // 1..128
        xpmodel::Xp60Patch patch;
        // Exactly what the instrument sent for this Patch.
        roland::ByteVector originalSysEx;
    };

    explicit UserBankRead(DeviceSession& session, QObject* parent = nullptr);

    [[nodiscard]] State state() const noexcept { return m_state; }
    [[nodiscard]] bool isBusy() const noexcept { return m_state == State::Reading; }
    [[nodiscard]] QString message() const { return m_message; }

    // Reads the given USER slots in the order given. Refused when already busy,
    // not connected, the list is empty, or any number is outside 1..128.
    bool read(std::vector<int> userNumbers);
    // All 128, in order.
    bool readWholeBank();
    // Stops after the Patch being read. Slots already read are kept and
    // reported, because a partial backup is still worth having.
    void cancel();

    [[nodiscard]] std::size_t total() const noexcept { return m_wanted.size(); }
    [[nodiscard]] std::size_t completed() const noexcept { return m_slots.size(); }
    [[nodiscard]] int currentUserNumber() const noexcept;
    // What was read. Populated as the run proceeds, so a cancelled or failed run
    // still hands back everything it got.
    [[nodiscard]] const std::vector<Slot>& readPatches() const noexcept { return m_slots; }

Q_SIGNALS:
    void changed();
    void progressed(std::size_t completed, std::size_t total);
    void finished(bool ok);

private:
    void requestNext();
    void onPatchFetchChanged();
    void finish(State state, QString message);

    DeviceSession& m_session;
    State m_state = State::Idle;
    QString m_message;
    bool m_awaitingFetch = false;
    bool m_cancelRequested = false;

    std::vector<int> m_wanted;
    std::size_t m_index = 0;
    std::vector<Slot> m_slots;
};

} // namespace xp60studio::services
