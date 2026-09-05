#pragma once

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace xp60studio::library {

// What is actually installed in this musician's XP-60.
//
// The instrument has four Wave Expansion slots, EXP-A to EXP-D, each holding one
// SR-JV80 board (Owner's Manual p.45). A Patch that uses a wave from a board the
// user does not own will not sound as its author intended, and the point of this
// type is to be able to say so before the musician discovers it on stage.
//
// ── Why the user declares this rather than the application detecting it ──────
//
// Nothing in the XP-60's protocol reports which boards are fitted, so this is
// the musician's own knowledge, recorded.
//
// XP60Studio can *name* what a Patch is asking for: a Tone identifies an
// expansion wave by **Wave Group ID**, and that ID is inferred to be the
// SR-JV80 board's catalogue number (`ExpansionBoardCatalog`, with the evidence
// in `docs/protocol/ROLAND_XP60_PROTOCOL_FACTS.md` §7). That inference fills in
// a wave group when a musician picks a board from the list, and nothing more.
//
// It is never authority over this type. A board whose group is not yet known is
// a first-class state rather than a blank to fill with a plausible number, and a
// group *learned* from the musician's own instrument overrides the catalogue —
// if their board answers to a different number, that is the fact and the table
// is wrong.
struct ExpansionBoard
{
    // 1..4, displayed as EXP-A..EXP-D.
    int slot = 0;
    // Whatever the musician calls it — "SR-JV80-05 World", "the orchestral one".
    // Never parsed for meaning.
    std::string name;
    // The Wave Group ID this board's waves carry, when it is known. Nullopt
    // means "installed, but XP60Studio cannot yet tell which waves are its",
    // which is honestly different from "not installed".
    std::optional<int> waveGroupId;

    [[nodiscard]] bool empty() const noexcept { return name.empty() && !waveGroupId.has_value(); }
    friend bool operator==(const ExpansionBoard&, const ExpansionBoard&) noexcept = default;
};

// "EXP-A".."EXP-D" for slots 1..4, empty otherwise.
[[nodiscard]] std::string slotLabel(int slot);
[[nodiscard]] constexpr bool isValidSlot(int slot) noexcept { return slot >= 1 && slot <= 4; }
inline constexpr int kSlotCount = 4;

// The four slots together.
class ExpansionProfile
{
public:
    ExpansionProfile();

    [[nodiscard]] const std::vector<ExpansionBoard>& boards() const noexcept { return m_boards; }
    // Always four entries, slots 1..4 in order. An empty name means the slot is
    // declared empty.
    [[nodiscard]] const ExpansionBoard& board(int slot) const;
    [[nodiscard]] int installedCount() const;

    // Sets what is in a slot. An empty name clears it, which also forgets its
    // group: an empty slot cannot answer for any wave.
    bool setBoard(int slot, std::string name, std::optional<int> waveGroupId);
    bool clearSlot(int slot);
    // Records that the board in `slot` answers to `waveGroupId`. This is how a
    // group is *learned* rather than guessed — see PatchCompatibility.
    bool setWaveGroup(int slot, int waveGroupId);
    void setBoards(std::vector<ExpansionBoard> boards);

    // Which slot provides `waveGroupId`, or nullopt. Only a board whose group is
    // known can answer; a board with an unknown group answers for nothing, which
    // is why `anyGroupUnknown()` exists to explain a "cannot tell" verdict.
    [[nodiscard]] std::optional<int> slotProviding(int waveGroupId) const;
    [[nodiscard]] bool providesGroup(int waveGroupId) const { return slotProviding(waveGroupId).has_value(); }
    // Every group a declared board answers for, ascending. Boards whose group
    // is not known contribute nothing, so an empty set can mean either "no
    // boards" or "no groups learned yet" — `anyGroupUnknown()` and `isEmpty()`
    // separate the two.
    [[nodiscard]] std::set<int> providedGroups() const;
    // True when at least one installed board has no known wave group, so a
    // "missing" verdict cannot be trusted and must be reported as unknown.
    [[nodiscard]] bool anyGroupUnknown() const;
    // No board declared at all. The honest verdict for every expansion
    // reference is then "unknown", not "missing": a musician who has not filled
    // this in has not told us they lack the board.
    [[nodiscard]] bool isEmpty() const;

    friend bool operator==(const ExpansionProfile&, const ExpansionProfile&) noexcept = default;

private:
    std::vector<ExpansionBoard> m_boards;
};

} // namespace xp60studio::library
