#include "library/ExpansionBoardCatalog.h"

#include "library/generated/SrJv80WaveCatalog.generated.h"

#include <algorithm>
#include <array>

namespace xp60studio::library {
namespace {

// Roland's SR-JV80 series. 01..19 are the main run; 96..99 are the
// Japanese-market and compilation boards, and their existence is the reason the
// Wave Group ID field is 0..127 rather than 0..19.
constexpr std::array<SrJv80Board, 23> kBoards{{
    {1, "Pop"},
    {2, "Orchestral"},
    {3, "Piano"},
    {4, "Vintage Synth"},
    {5, "World"},
    {6, "Dance"},
    {7, "Super Sound Set"},
    {8, "Keyboards of the 60s & 70s"},
    {9, "Session"},
    {10, "Bass & Drums"},
    {11, "Techno Collection"},
    {12, "Hip Hop Collection"},
    {13, "Vocal Collection"},
    {14, "Asia"},
    {15, "Special FX Collection"},
    {16, "Orchestral II"},
    {17, "Country Collection"},
    {18, "Latin Collection"},
    {19, "House Collection"},
    {96, "World Collection: Latin"},
    {97, "Experience III"},
    {98, "Experience II"},
    {99, "Experience"},
}};

const SrJv80Board* find(int boardNumber) noexcept
{
    const auto found = std::find_if(kBoards.begin(), kBoards.end(),
                                    [boardNumber](const SrJv80Board& board) { return board.number == boardNumber; });
    return found == kBoards.end() ? nullptr : &*found;
}

} // namespace

std::span<const SrJv80Board> srJv80Boards() noexcept
{
    return {kBoards.data(), kBoards.size()};
}

bool isKnownSrJv80Board(int boardNumber) noexcept
{
    return find(boardNumber) != nullptr;
}

std::optional<std::string> srJv80BoardName(int boardNumber)
{
    const auto* board = find(boardNumber);
    if (!board) {
        return std::nullopt;
    }
    // Two digits, as Roland prints them: SR-JV80-05, not SR-JV80-5.
    std::string number = std::to_string(board->number);
    if (number.size() < 2) {
        number.insert(number.begin(), '0');
    }
    return "SR-JV80-" + number + " " + std::string(board->title);
}

namespace {

const srjv80::BoardList* findList(int boardNumber) noexcept
{
    const auto found = std::find_if(srjv80::kBoardLists.begin(), srjv80::kBoardLists.end(),
                                    [boardNumber](const srjv80::BoardList& l) { return l.board == boardNumber; });
    return found == srjv80::kBoardLists.end() ? nullptr : &*found;
}

} // namespace

bool hasSrJv80WaveList(int boardNumber) noexcept
{
    return findList(boardNumber) != nullptr;
}

int srJv80WaveCount(int boardNumber) noexcept
{
    const auto* list = findList(boardNumber);
    return list ? list->count : 0;
}

std::optional<std::string_view> srJv80WaveName(int boardNumber, int displayNumber) noexcept
{
    const auto* list = findList(boardNumber);
    if (!list || displayNumber < 1 || displayNumber > list->count) {
        return std::nullopt;
    }
    return list->first[displayNumber - 1];
}

std::string describeExpansionWave(int waveGroupId, int rawWaveNumber)
{
    // Roland prints wave numbers from 1; the Tone carries one less.
    const int display = rawWaveNumber + 1;
    const auto board = srJv80BoardName(waveGroupId);
    if (!board) {
        return "wave " + std::to_string(display) + " of " + describeWaveGroup(waveGroupId);
    }
    if (const auto wave = srJv80WaveName(waveGroupId, display)) {
        return "wave " + std::to_string(display) + " \u201c" + std::string(*wave) + "\u201d on " + *board;
    }
    return "wave " + std::to_string(display) + " on " + *board;
}

std::string describeWaveGroup(int waveGroupId)
{
    const auto name = srJv80BoardName(waveGroupId);
    const std::string group = "wave group " + std::to_string(waveGroupId);
    // Fact first, inference in brackets. A group with no board of that number is
    // not an error: the musician's instrument is the authority, not this table.
    return name ? group + " (" + *name + ")" : group;
}

} // namespace xp60studio::library
