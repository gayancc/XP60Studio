#include "library/ExpansionProfile.h"

#include <algorithm>

namespace xp60studio::library {

namespace {

const ExpansionBoard kNoBoard{};

} // namespace

std::string slotLabel(int slot)
{
    if (!isValidSlot(slot)) {
        return {};
    }
    return std::string("EXP-") + static_cast<char>('A' + slot - 1);
}

ExpansionProfile::ExpansionProfile()
{
    m_boards.resize(static_cast<std::size_t>(kSlotCount));
    for (int slot = 1; slot <= kSlotCount; ++slot) {
        m_boards[static_cast<std::size_t>(slot - 1)].slot = slot;
    }
}

const ExpansionBoard& ExpansionProfile::board(int slot) const
{
    if (!isValidSlot(slot)) {
        return kNoBoard;
    }
    return m_boards[static_cast<std::size_t>(slot - 1)];
}

int ExpansionProfile::installedCount() const
{
    return static_cast<int>(std::count_if(m_boards.begin(), m_boards.end(),
                                          [](const ExpansionBoard& b) { return !b.name.empty(); }));
}

bool ExpansionProfile::setBoard(int slot, std::string name, std::optional<int> waveGroupId)
{
    if (!isValidSlot(slot)) {
        return false;
    }
    if (waveGroupId && (*waveGroupId < 0 || *waveGroupId > 127)) {
        // The Parameter Address Map gives Wave Group ID as one 7-bit byte.
        // A number outside it could never appear in a Tone, so accepting it
        // would create a board that can never match anything.
        return false;
    }
    auto& board = m_boards[static_cast<std::size_t>(slot - 1)];
    if (name.empty()) {
        // Clearing the name clears the slot: an empty slot cannot answer for
        // any wave, so keeping its group would let it go on claiming one.
        board.name.clear();
        board.waveGroupId.reset();
        return true;
    }
    board.name = std::move(name);
    board.waveGroupId = waveGroupId;
    return true;
}

bool ExpansionProfile::clearSlot(int slot)
{
    return setBoard(slot, {}, std::nullopt);
}

bool ExpansionProfile::setWaveGroup(int slot, int waveGroupId)
{
    if (!isValidSlot(slot) || waveGroupId < 0 || waveGroupId > 127) {
        return false;
    }
    auto& board = m_boards[static_cast<std::size_t>(slot - 1)];
    if (board.name.empty()) {
        // Nothing is declared here, so there is no board for the group to
        // belong to.
        return false;
    }
    board.waveGroupId = waveGroupId;
    return true;
}

void ExpansionProfile::setBoards(std::vector<ExpansionBoard> boards)
{
    for (auto& incoming : boards) {
        if (!isValidSlot(incoming.slot)) {
            continue;
        }
        setBoard(incoming.slot, incoming.name, incoming.waveGroupId);
    }
}

std::optional<int> ExpansionProfile::slotProviding(int waveGroupId) const
{
    for (const auto& board : m_boards) {
        if (!board.name.empty() && board.waveGroupId && *board.waveGroupId == waveGroupId) {
            return board.slot;
        }
    }
    return std::nullopt;
}

bool ExpansionProfile::anyGroupUnknown() const
{
    return std::any_of(m_boards.begin(), m_boards.end(), [](const ExpansionBoard& b) {
        return !b.name.empty() && !b.waveGroupId.has_value();
    });
}

bool ExpansionProfile::isEmpty() const
{
    return installedCount() == 0;
}

} // namespace xp60studio::library
