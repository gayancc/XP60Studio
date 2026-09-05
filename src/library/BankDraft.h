#pragma once

#include "xpmodel/Xp60BankLocation.h"

#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <utility>
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

// A named run of destinations inside a bank.
//
// A 128-slot bank is a lot of undifferentiated boxes. A musician building one
// thinks in groups — "pianos at the front, pads after them, the set list at the
// end" — and the instrument gives them no way to say so, because the XP-60 knows
// only USER:001..128. Sections are XP60Studio's own organisation, saved with the
// bank and carried nowhere near the instrument: they change no address and are
// never transmitted.
struct BankSection
{
    std::string name;
    int firstSlot = 0; // 0..127, inclusive
    int lastSlot = 0;  // 0..127, inclusive

    [[nodiscard]] bool contains(int slotIndex) const noexcept
    {
        return slotIndex >= firstSlot && slotIndex <= lastSlot;
    }
    [[nodiscard]] bool overlaps(const BankSection& other) const noexcept
    {
        return firstSlot <= other.lastSlot && other.firstSlot <= lastSlot;
    }
    [[nodiscard]] int slotCount() const noexcept { return lastSlot - firstSlot + 1; }

    friend bool operator==(const BankSection&, const BankSection&) noexcept = default;
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
    // Fills several destinations at once as **one** undo step, which is what
    // filling a bank from a source has to be: undoing it must put the whole
    // arrangement back, not remove one of 128 placements at a time. Each pair
    // is (slotIndex, content); an invalid index or empty content is refused
    // outright rather than silently skipped, so a caller cannot believe it
    // placed more than it did. Returns false and changes nothing when refused
    // or when nothing would change.
    bool assignAll(const std::vector<std::pair<int, BankSlotContent>>& placements, std::string label);
    bool clear(int slotIndex);
    // Empties several destinations as **one** undo step, for a multi-selection.
    // Indices that are out of range or already empty are ignored rather than
    // refusing the whole operation: clearing a selection that happens to
    // include an empty destination is a perfectly sensible thing to ask for.
    // Returns false and changes nothing when nothing would be emptied.
    bool clearSlots(const std::vector<int>& slotIndices, std::string label);
    // Moves within the bank: a swap when both are occupied, a move when the
    // destination is empty. Refused when `from` is empty, because there would
    // be nothing to move.
    bool moveOrSwap(int from, int to);
    bool clearAll();

    // Sections ----------------------------------------------------------------
    // Kept sorted by first destination and never overlapping, so "which section
    // is this destination in" has exactly one answer.
    [[nodiscard]] const std::vector<BankSection>& sections() const noexcept { return m_sections; }
    // The section containing `slotIndex`, or nullptr.
    [[nodiscard]] const BankSection* sectionAt(int slotIndex) const;
    // Adds a section. Refused for an empty name, an out-of-range or inverted
    // range, or a range that overlaps an existing section — a destination in two
    // sections would make the rail lie about where it is. One undo step.
    bool addSection(std::string name, int firstSlot, int lastSlot);
    bool renameSection(int firstSlot, std::string name);
    bool removeSection(int firstSlot);
    // Replaces the whole set, for loading a saved bank. Overlapping or invalid
    // entries are dropped rather than refused, because a stored bank must still
    // open. Does not touch history.
    void setSections(std::vector<BankSection> sections);
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
        std::vector<BankSection> sections;
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
    std::vector<BankSection> m_sections;
    int m_occupied = 0;
    bool m_modified = false;
    std::optional<std::int64_t> m_savedBankId;
    std::string m_lastActionLabel;

    std::deque<Snapshot> m_undo;
    std::deque<Snapshot> m_redo;
};

} // namespace xp60studio::library
