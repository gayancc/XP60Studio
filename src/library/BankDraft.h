#pragma once

#include "xpmodel/Xp60BankLocation.h"

#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace xp60studio::library {

// One of the 128 destinations in a User bank under construction.
//
// A slot holds a *reference* to a library Patch, never a copy of it. Building a
// bank is an arrangement, not a duplication: the source Patch and its original
// SysEx stay exactly where they were, and dropping the same Patch into four
// destinations still leaves one Patch in the library.
//
// The name and source are cached alongside the id so a saved bank still reads
// correctly — and can say what is missing — when the Patch it referenced has
// since been deleted from the library.
struct BankSlotContent
{
    // 0 means the destination is empty. Library ids are positive.
    std::int64_t patchId = 0;
    std::string patchName;
    std::string sourceName;   // the file or device the Patch came from
    std::string sourceSlotLabel; // where it sat in its source, e.g. "USER:007"
    // The referenced Patch is no longer in the library. Set when a saved bank
    // is loaded; never a reason to drop the slot silently.
    bool missing = false;

    // A destination holding a Patch that has since been deleted is *not*
    // empty: the user put something there, and the bank has to keep saying so
    // rather than quietly offering the position as free.
    [[nodiscard]] bool empty() const noexcept { return patchId == 0 && !missing; }

    friend bool operator==(const BankSlotContent&, const BankSlotContent&) noexcept = default;
};

// A 128-Patch User bank being built.
//
// The draft owns the arrangement and its history. It knows nothing about
// SysEx, addresses or Qt: assigning a destination is an edit to a vector, and
// what a destination *means* comes from xpmodel::Xp60BankLocation. That keeps
// every rule about A/B, BANK and NUMBER in one tested place, and lets the
// presentation layer stay a thin projection of this state.
//
// Undo is snapshot-based. A bank is 128 small records, so keeping whole
// snapshots is both simpler and safer than replaying inverse operations, and
// it makes a compound edit (a swap is two writes) one undo step by
// construction.
class BankDraft
{
public:
    static constexpr int kSlotCount = xpmodel::Xp60BankLocation::kUserPatchCount; // 128
    // How many steps back the user can go. Deep enough for a full bank build,
    // bounded so a long session cannot grow without limit.
    static constexpr std::size_t kMaxHistory = 128;

    BankDraft();
    explicit BankDraft(std::string name);

    [[nodiscard]] const std::string& name() const noexcept { return m_name; }
    // Renaming is an undoable edit like any other: it changes what will be
    // saved.
    bool setName(std::string name);

    [[nodiscard]] const std::vector<BankSlotContent>& destinations() const noexcept { return m_slots; }
    // Empty content for an out-of-range index, so a caller can never read past
    // the bank.
    [[nodiscard]] const BankSlotContent& slot(int slotIndex) const;
    [[nodiscard]] bool isOccupied(int slotIndex) const;
    [[nodiscard]] int occupiedCount() const noexcept { return m_occupied; }
    [[nodiscard]] int emptyCount() const noexcept { return kSlotCount - m_occupied; }
    // How many destinations in one subgroup/bank pair are filled. `bank` is
    // 1-8, `subgroup` 0-1; -1 for an invalid pair.
    [[nodiscard]] int occupiedInBank(int subgroup, int bank) const;

    // Edits ------------------------------------------------------------------
    // Each returns false and changes nothing when refused, and each pushes at
    // most one undo step. `label` is what the UI shows for that step.

    // Places a Patch. Replacing an occupied destination is allowed — the UI is
    // required to say REPLACE before the drop — and is one undo step.
    bool assign(int slotIndex, BankSlotContent content);
    bool clear(int slotIndex);
    // Moves within the bank: a swap when both are occupied, a move when the
    // destination is empty. Refused when `from` is empty, because there would
    // be nothing to move.
    bool moveOrSwap(int from, int to);
    bool clearAll();
    // Every destination filled by `contents` in slot order. Used when a saved
    // bank is loaded; clears history because the previous draft is gone.
    void reset(std::string name, std::vector<BankSlotContent> contents);

    // History ----------------------------------------------------------------
    [[nodiscard]] bool canUndo() const noexcept { return !m_undo.empty(); }
    [[nodiscard]] bool canRedo() const noexcept { return !m_redo.empty(); }
    // What undo/redo would do, e.g. "Place GrandPiano at A35". Empty when
    // there is nothing to undo/redo.
    [[nodiscard]] std::string undoLabel() const;
    [[nodiscard]] std::string redoLabel() const;
    bool undo();
    bool redo();

    // Saving -----------------------------------------------------------------
    // True when the draft differs from the last saved state. A brand-new empty
    // bank is not modified; placing anything makes it so.
    [[nodiscard]] bool modified() const noexcept { return m_modified; }
    // The stored bank this draft was loaded from or last saved as. Nullopt for
    // a draft that has never been saved.
    [[nodiscard]] std::optional<std::int64_t> savedBankId() const noexcept { return m_savedBankId; }
    // Records that the current arrangement is what `bankId` holds.
    void markSaved(std::int64_t bankId);
    // Detaches from the stored bank without changing the arrangement, so
    // "save as new" cannot overwrite the bank it was derived from.
    void detachFromSavedBank();

    // The label the last edit produced, for the surface's action feedback.
    [[nodiscard]] const std::string& lastActionLabel() const noexcept { return m_lastActionLabel; }

private:
    struct Snapshot
    {
        std::string name;
        std::vector<BankSlotContent> destinations;
        int occupied = 0;
        bool modified = false;
        // What the edit that produced the *next* state did.
        std::string label;
    };

    [[nodiscard]] Snapshot capture(std::string label) const;
    void restore(Snapshot snapshot);
    void pushUndo(std::string label);
    void recount();

    std::string m_name;
    std::vector<BankSlotContent> m_slots;
    int m_occupied = 0;
    bool m_modified = false;
    std::optional<std::int64_t> m_savedBankId;
    std::string m_lastActionLabel;

    std::deque<Snapshot> m_undo;
    std::deque<Snapshot> m_redo;
};

} // namespace xp60studio::library
