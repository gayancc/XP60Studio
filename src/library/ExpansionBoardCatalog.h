#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace xp60studio::library {

// The SR-JV80 Wave Expansion Board catalogue, and the inferred relationship
// between a board and the Wave Group ID its waves carry.
//
// ── What is documented, and what is inferred ─────────────────────────────────
//
// A Tone names an expansion wave by Wave Group ID: one 7-bit field, 0..127,
// which Roland's Parameter Address Map defines the width of and nothing else.
// The mapping below — **group ID is the SR-JV80 board number** — is an
// inference, not a documented fact, and the reasoning is set out in full in
// `docs/protocol/ROLAND_XP60_PROTOCOL_FACTS.md` §7.
//
// In short: every group the golden fixture uses (1, 5, 7, 14, 97) is a real
// board number, and the Patches using them match those boards' contents — group
// 5's are world instruments, group 14's is a sitar. The series runs 01..19 and
// 96..99, which is also why the field is 0..127 wide rather than 0..19.
//
// ── What this catalogue is allowed to do ─────────────────────────────────────
//
// Name things, and save typing. It lets XP60Studio say "wave group 14 —
// SR-JV80-14 Asia" instead of a bare number, and lets the Expansion Manager
// offer real boards to choose from.
//
// It is **not** authority. What is installed in an instrument is only ever what
// the musician declared (`ExpansionProfile`), a group learned from their own
// instrument outranks this table, and a musician whose board answers to a
// different number can say so and be believed. Nothing here concludes that an
// instrument has a board, and no compatibility verdict rests on it.
struct SrJv80Board
{
    // The catalogue number: 1 for SR-JV80-01, 97 for SR-JV80-97.
    int number = 0;
    // Roland's title for it, without the "SR-JV80-nn" prefix.
    std::string_view title;
};

// Every board this project knows of, in catalogue order.
[[nodiscard]] std::span<const SrJv80Board> srJv80Boards() noexcept;

// "SR-JV80-14 Asia" for 14, or nullopt for a number no board carries. A
// musician's own board may still answer to such a number — this says only that
// XP60Studio cannot name it.
[[nodiscard]] std::optional<std::string> srJv80BoardName(int boardNumber);

// "wave group 14 (SR-JV80-14 Asia)", or "wave group 42" when nothing is known.
// The group number always comes first: it is the fact, and the name is the
// inference resting on it.
[[nodiscard]] std::string describeWaveGroup(int waveGroupId);

[[nodiscard]] bool isKnownSrJv80Board(int boardNumber) noexcept;

} // namespace xp60studio::library
