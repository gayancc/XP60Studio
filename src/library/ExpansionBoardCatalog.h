#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace xp60studio::library {

// The SR-JV80 Wave Expansion Board catalogue, and the relationship
// between a board and the Wave Group ID its waves carry.
//
// ── What is documented, and what is corroborated ────────────────────────────
//
// A Tone names an expansion wave by Wave Group ID: one 7-bit field, 0..127,
// which Roland's Parameter Address Map defines the width of and nothing else.
// The mapping below — **group ID is the SR-JV80 board number** — is therefore
// not documented by Roland, but it is corroborated twice over, and
// `docs/protocol/ROLAND_XP60_PROTOCOL_FACTS.md` §7 sets out the evidence.
//
// In short: an independent implementation (JV PatchEd. — JV-XP, a Ctrlr panel
// for this family of instruments) transmits 14 for Asia and 97 for Experience
// III; and resolving the golden fixture's 192 expansion references as
// (board = group ID, wave = wave number) lands every one of them on a wave that
// exists on that board, with the names matching the Patches — a Patch called
// `*Tenor Solo` on group 97 wave 4 is SR-JV80-97's `*Tenor Solo`.
//
// The series runs 01..19 and 96..99, which is also why the field is 0..127 wide
// rather than 0..19.
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

// ── Waveform names, where Roland's own list is held ──────────────────────────
//
// `docs/XP60-References/SR-JV80/` holds Roland's per-board Waveform Lists for
// some boards. Where one is present, XP60Studio can name the wave a Tone
// actually points at rather than only the board it lives on.
//
// The distinction the API keeps is between **no list** and **no such wave**.
// A board this project has no list for is not a board with no waves, and a
// musician whose Tone points at wave 200 of a board whose list stops at 154 has
// been told something worth knowing. Both come back as nullopt from
// `srJv80WaveName`, so callers that care use `hasSrJv80WaveList` first.

// True when this project holds Roland's Waveform List for the board.
[[nodiscard]] bool hasSrJv80WaveList(int boardNumber) noexcept;
// How many waves that list has, or 0 when there is no list.
[[nodiscard]] int srJv80WaveCount(int boardNumber) noexcept;
// The name Roland prints for `displayNumber` (1-based, as the instrument shows
// it — a Tone's raw byte is one less). Nullopt when there is no list for the
// board, or the number is outside it.
[[nodiscard]] std::optional<std::string_view> srJv80WaveName(int boardNumber, int displayNumber) noexcept;

// "wave 17 “Clav 2A” on SR-JV80-01 Pop" — the fullest honest description of an
// expansion reference, degrading a piece at a time as knowledge runs out:
// without a wave list, "wave 17 on SR-JV80-01 Pop"; without even a board of
// that number, "wave 17 of wave group 42".
//
// `rawWaveNumber` is the Tone's byte; the printed number is one more.
[[nodiscard]] std::string describeExpansionWave(int waveGroupId, int rawWaveNumber);

} // namespace xp60studio::library
