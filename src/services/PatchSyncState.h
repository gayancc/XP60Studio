#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace xp60studio::services {

// The two questions that can be asked about a Patch being worked on, kept
// deliberately apart.
//
// "Is my work kept?" and "does the instrument hold what I am looking at?" are
// independent: a Patch can be saved in the Studio and absent from the XP-60, or
// auditioning on the instrument and never saved. Collapsing them into one badge
// is how an editor ends up telling a musician their sound is "saved" when what
// it means is "sent".
//
// See docs/PATCH_SYNCHRONIZATION.md §4 for the evidence behind each state.

// ---------------------------------------------------------------------------
// Studio storage
// ---------------------------------------------------------------------------
enum class StudioState {
    // A working Patch with no library row behind it, e.g. a fresh device read.
    Untracked,
    // Byte-identical to the library row it came from.
    Saved,
    // Differs from its library row. Abandoning it loses the difference.
    Edited,
};

[[nodiscard]] std::string_view studioStateName(StudioState state) noexcept;
[[nodiscard]] std::string_view studioStateLabel(StudioState state) noexcept;

// ---------------------------------------------------------------------------
// XP-60 temporary memory
// ---------------------------------------------------------------------------
//
// Every state here is about the **temporary** area (`03 00 00 00`), which the
// Owner's Manual (p.45) says is what the instrument makes sound from and what it
// discards on a patch change or power-off. None of it says anything about
// permanent USER memory, which is not a live status at all — see
// PersistentWriteRecord below.
enum class DeviceState {
    // No connection. The question does not apply.
    Offline,
    // Connected, but this working Patch has never been transmitted.
    NotSent,
    // DT1 messages are in flight.
    Sending,
    // The bytes were transmitted and the transport accepted them, but nothing
    // has been read back. This is **not** a claim of synchronization: it is the
    // honest state for real-time editing, where verifying every parameter move
    // would cost a full five-block read (~265 ms measured, see §1.5 T6).
    Assumed,
    // A read-back proved the temporary area equals the working Patch. The only
    // state that claims synchronization, and only ever after a byte comparison.
    InSync,
    // The working Patch has changed since the last send or verification.
    Diverged,
    // The instrument's temporary area is believed replaced or unknowable, so
    // nothing may be inferred from an earlier verification. Reached when the
    // XP-60 announces its own Patch change (Bank Select / Program Change, which
    // by OM p.45 destroys the temporary area), on disconnect, on a device-ID
    // change, or on a transport error.
    Stale,
    // A transfer did not complete. The temporary area may be partly written.
    Failed,
};

[[nodiscard]] std::string_view deviceStateName(DeviceState state) noexcept;
// Short, LCD-style: "XP TEMP", "SENT", "STALE"...
[[nodiscard]] std::string_view deviceStateLabel(DeviceState state) noexcept;
// "neutral" | "info" | "success" | "warning" | "error", for StatusPill tones.
[[nodiscard]] std::string_view deviceStateTone(DeviceState state) noexcept;
// Whether this state asserts the instrument holds the working Patch. Only
// InSync does; Assumed deliberately does not.
[[nodiscard]] bool assertsSynchronized(DeviceState state) noexcept;

// ---------------------------------------------------------------------------
// Where a working Patch came from
// ---------------------------------------------------------------------------
//
// The origin is what makes cross-screen synchronization possible and what keeps
// it honest: a working Patch adopted from library id 42 overlays library row 42
// and every bank destination that references 42, and nothing else.
struct PatchOrigin
{
    enum class Kind {
        None,
        // A row in the XP60Studio library.
        LibraryEntry,
        // Read from the XP-60's temporary area (`03 00 00 00`).
        DeviceTemporary,
        // Read from a permanent USER Patch slot (`11 nn 00 00`).
        DeviceUserSlot,
    };

    Kind kind = Kind::None;
    std::int64_t libraryId = 0; // Kind::LibraryEntry
    int userNumber = 0;         // Kind::DeviceUserSlot, 1..128

    [[nodiscard]] bool isLibraryEntry(std::int64_t id) const noexcept
    {
        return kind == Kind::LibraryEntry && libraryId == id && id > 0;
    }
    [[nodiscard]] static PatchOrigin library(std::int64_t id)
    {
        return {Kind::LibraryEntry, id, 0};
    }
    [[nodiscard]] static PatchOrigin temporary() { return {Kind::DeviceTemporary, 0, 0}; }
    [[nodiscard]] static PatchOrigin userSlot(int userNumber)
    {
        return {Kind::DeviceUserSlot, 0, userNumber};
    }

    friend bool operator==(const PatchOrigin&, const PatchOrigin&) noexcept = default;
};

// ---------------------------------------------------------------------------
// Permanent USER memory
// ---------------------------------------------------------------------------
//
// Deliberately not a live status. XP60Studio cannot know what a USER slot holds
// without reading it, and a stale "written" badge is worse than none. What is
// recorded is an event: this working Patch was written to USER:nnn and the
// read-back verified. It is reported as "last written", never as "in sync".
struct PersistentWriteRecord
{
    bool everWritten = false;
    int userNumber = 0;    // 1..128
    bool verified = false; // the read-back compared equal
    std::string note;

    void clear() { *this = PersistentWriteRecord{}; }
};

} // namespace xp60studio::services
