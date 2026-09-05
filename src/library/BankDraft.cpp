#include "library/BankDraft.h"

#include <algorithm>
#include <utility>

namespace xp60studio::library {
namespace {

const BankSlotContent kEmptySlot{};

std::string panelLabelFor(int slotIndex)
{
    const auto location = xpmodel::Xp60BankLocation::fromSlotIndex(slotIndex);
    return location ? location->panelLabel() : std::string("---");
}

// "GrandPiano" when there is one, otherwise something that still names the
// destination rather than reading as an empty sentence.
std::string patchLabel(const BankSlotContent& content)
{
    return content.patchName.empty() ? std::string("Patch") : content.patchName;
}

} // namespace

BankDraft::BankDraft()
    : BankDraft(std::string("New User Bank"))
{
}

BankDraft::BankDraft(std::string name)
    : m_name(std::move(name))
    , m_slots(static_cast<std::size_t>(kSlotCount))
{
}

const BankSlotContent& BankDraft::slot(int slotIndex) const
{
    if (!xpmodel::Xp60BankLocation::isValidSlotIndex(slotIndex)) {
        return kEmptySlot;
    }
    return m_slots[static_cast<std::size_t>(slotIndex)];
}

bool BankDraft::isOccupied(int slotIndex) const
{
    return !slot(slotIndex).empty();
}

int BankDraft::occupiedInBank(int subgroup, int bank) const
{
    if (!xpmodel::Xp60BankLocation::isValidSubgroup(subgroup) || !xpmodel::Xp60BankLocation::isValidBank(bank)) {
        return -1;
    }
    int filled = 0;
    for (int number = 1; number <= xpmodel::Xp60BankLocation::kNumbersPerBank; ++number) {
        const auto location = xpmodel::Xp60BankLocation::fromPanel(subgroup, bank, number);
        if (location && isOccupied(location->slotIndex())) {
            ++filled;
        }
    }
    return filled;
}

bool BankDraft::setName(std::string name)
{
    if (name.empty() || name == m_name) {
        return false;
    }
    pushUndo("Rename bank to " + name);
    m_name = std::move(name);
    m_modified = true;
    m_lastActionLabel = "Renamed bank";
    return true;
}

bool BankDraft::assign(int slotIndex, BankSlotContent content)
{
    if (!xpmodel::Xp60BankLocation::isValidSlotIndex(slotIndex) || content.empty()) {
        return false;
    }
    auto& target = m_slots[static_cast<std::size_t>(slotIndex)];
    if (target == content) {
        return false;
    }
    const bool replacing = !target.empty();
    const std::string label = (replacing ? "Replace " + patchLabel(target) + " with " : "Place ")
        + patchLabel(content) + " at " + panelLabelFor(slotIndex);
    pushUndo(label);
    target = std::move(content);
    m_modified = true;
    m_lastActionLabel = label;
    recount();
    return true;
}

bool BankDraft::clear(int slotIndex)
{
    if (!isOccupied(slotIndex)) {
        return false;
    }
    auto& target = m_slots[static_cast<std::size_t>(slotIndex)];
    const std::string label = "Clear " + patchLabel(target) + " from " + panelLabelFor(slotIndex);
    pushUndo(label);
    target = BankSlotContent{};
    m_modified = true;
    m_lastActionLabel = label;
    recount();
    return true;
}

bool BankDraft::moveOrSwap(int from, int to)
{
    if (!xpmodel::Xp60BankLocation::isValidSlotIndex(from) || !xpmodel::Xp60BankLocation::isValidSlotIndex(to)
        || from == to || !isOccupied(from)) {
        return false;
    }
    auto& source = m_slots[static_cast<std::size_t>(from)];
    auto& target = m_slots[static_cast<std::size_t>(to)];
    const bool swapping = !target.empty();
    const std::string label = swapping
        ? "Swap " + panelLabelFor(from) + " and " + panelLabelFor(to)
        : "Move " + patchLabel(source) + " to " + panelLabelFor(to);
    pushUndo(label);
    std::swap(source, target);
    m_modified = true;
    m_lastActionLabel = label;
    // A swap moves two Patches and a move moves one; either way the total
    // stays the same, but recount keeps one rule instead of two.
    recount();
    return true;
}

bool BankDraft::assignAll(const std::vector<std::pair<int, BankSlotContent>>& placements, std::string label)
{
    if (placements.empty()) {
        return false;
    }
    // Validate everything before touching anything: a fill that turns out to
    // be half-legal must not leave half an arrangement behind.
    for (const auto& [slotIndex, content] : placements) {
        if (!xpmodel::Xp60BankLocation::isValidSlotIndex(slotIndex) || content.empty()) {
            return false;
        }
    }
    const bool changes = std::any_of(placements.begin(), placements.end(), [this](const auto& placement) {
        return !(m_slots[static_cast<std::size_t>(placement.first)] == placement.second);
    });
    if (!changes) {
        return false;
    }

    pushUndo(label);
    for (const auto& [slotIndex, content] : placements) {
        m_slots[static_cast<std::size_t>(slotIndex)] = content;
    }
    m_modified = true;
    m_lastActionLabel = std::move(label);
    recount();
    return true;
}

bool BankDraft::clearSlots(const std::vector<int>& slotIndices, std::string label)
{
    const bool anything = std::any_of(slotIndices.begin(), slotIndices.end(), [this](int slotIndex) {
        return xpmodel::Xp60BankLocation::isValidSlotIndex(slotIndex)
            && !m_slots[static_cast<std::size_t>(slotIndex)].empty();
    });
    if (!anything) {
        return false;
    }
    pushUndo(label);
    for (const int slotIndex : slotIndices) {
        if (xpmodel::Xp60BankLocation::isValidSlotIndex(slotIndex)) {
            m_slots[static_cast<std::size_t>(slotIndex)] = BankSlotContent{};
        }
    }
    m_modified = true;
    m_lastActionLabel = std::move(label);
    recount();
    return true;
}

bool BankDraft::clearAll()
{
    if (m_occupied == 0) {
        return false;
    }
    pushUndo("Clear every destination");
    std::fill(m_slots.begin(), m_slots.end(), BankSlotContent{});
    m_modified = true;
    m_lastActionLabel = "Cleared every destination";
    recount();
    return true;
}

const BankSection* BankDraft::sectionAt(int slotIndex) const
{
    const auto found = std::find_if(m_sections.begin(), m_sections.end(),
                                    [slotIndex](const BankSection& section) { return section.contains(slotIndex); });
    return found == m_sections.end() ? nullptr : &*found;
}

bool BankDraft::addSection(std::string name, int firstSlot, int lastSlot)
{
    if (name.empty() || !xpmodel::Xp60BankLocation::isValidSlotIndex(firstSlot)
        || !xpmodel::Xp60BankLocation::isValidSlotIndex(lastSlot) || lastSlot < firstSlot) {
        return false;
    }
    const BankSection candidate{std::move(name), firstSlot, lastSlot};
    // A destination in two sections would make the rail lie about where it is.
    if (std::any_of(m_sections.begin(), m_sections.end(),
                    [&candidate](const BankSection& existing) { return existing.overlaps(candidate); })) {
        return false;
    }
    const std::string label = "Add section " + candidate.name;
    pushUndo(label);
    m_sections.push_back(candidate);
    std::sort(m_sections.begin(), m_sections.end(),
              [](const BankSection& a, const BankSection& b) { return a.firstSlot < b.firstSlot; });
    m_modified = true;
    m_lastActionLabel = label;
    return true;
}

bool BankDraft::renameSection(int firstSlot, std::string name)
{
    if (name.empty()) {
        return false;
    }
    const auto found = std::find_if(m_sections.begin(), m_sections.end(),
                                    [firstSlot](const BankSection& s) { return s.firstSlot == firstSlot; });
    if (found == m_sections.end() || found->name == name) {
        return false;
    }
    const std::string label = "Rename section " + found->name + " to " + name;
    pushUndo(label);
    // pushUndo captured the old state, so the iterator is still valid here.
    std::find_if(m_sections.begin(), m_sections.end(),
                 [firstSlot](const BankSection& s) { return s.firstSlot == firstSlot; })
        ->name = std::move(name);
    m_modified = true;
    m_lastActionLabel = label;
    return true;
}

bool BankDraft::removeSection(int firstSlot)
{
    const auto found = std::find_if(m_sections.begin(), m_sections.end(),
                                    [firstSlot](const BankSection& s) { return s.firstSlot == firstSlot; });
    if (found == m_sections.end()) {
        return false;
    }
    const std::string label = "Remove section " + found->name;
    pushUndo(label);
    m_sections.erase(std::find_if(m_sections.begin(), m_sections.end(),
                                  [firstSlot](const BankSection& s) { return s.firstSlot == firstSlot; }));
    m_modified = true;
    m_lastActionLabel = label;
    return true;
}

void BankDraft::setSections(std::vector<BankSection> sections)
{
    // A stored bank must still open, so anything unusable is dropped rather
    // than refusing the whole load.
    std::sort(sections.begin(), sections.end(),
              [](const BankSection& a, const BankSection& b) { return a.firstSlot < b.firstSlot; });
    m_sections.clear();
    for (auto& section : sections) {
        if (section.name.empty() || !xpmodel::Xp60BankLocation::isValidSlotIndex(section.firstSlot)
            || !xpmodel::Xp60BankLocation::isValidSlotIndex(section.lastSlot)
            || section.lastSlot < section.firstSlot) {
            continue;
        }
        if (!m_sections.empty() && m_sections.back().overlaps(section)) {
            continue;
        }
        m_sections.push_back(std::move(section));
    }
}

void BankDraft::reset(std::string name, std::vector<BankSlotContent> contents)
{
    m_name = std::move(name);
    m_sections.clear();
    contents.resize(static_cast<std::size_t>(kSlotCount));
    m_slots = std::move(contents);
    m_undo.clear();
    m_redo.clear();
    m_modified = false;
    m_savedBankId.reset();
    m_lastActionLabel.clear();
    recount();
}

std::string BankDraft::undoLabel() const
{
    return m_undo.empty() ? std::string() : m_undo.back().label;
}

std::string BankDraft::redoLabel() const
{
    return m_redo.empty() ? std::string() : m_redo.back().label;
}

bool BankDraft::undo()
{
    if (m_undo.empty()) {
        return false;
    }
    Snapshot previous = std::move(m_undo.back());
    m_undo.pop_back();
    m_redo.push_back(capture(previous.label));
    m_lastActionLabel = "Undo " + previous.label;
    restore(std::move(previous));
    return true;
}

bool BankDraft::redo()
{
    if (m_redo.empty()) {
        return false;
    }
    Snapshot next = std::move(m_redo.back());
    m_redo.pop_back();
    m_undo.push_back(capture(next.label));
    m_lastActionLabel = "Redo " + next.label;
    restore(std::move(next));
    return true;
}

void BankDraft::markSaved(std::int64_t bankId)
{
    m_savedBankId = bankId;
    m_modified = false;
}

void BankDraft::detachFromSavedBank()
{
    m_savedBankId.reset();
}

BankDraft::Snapshot BankDraft::capture(std::string label) const
{
    return Snapshot{m_name, m_slots, m_sections, m_occupied, m_modified, std::move(label)};
}

void BankDraft::restore(Snapshot snapshot)
{
    m_name = std::move(snapshot.name);
    m_slots = std::move(snapshot.destinations);
    m_sections = std::move(snapshot.sections);
    m_occupied = snapshot.occupied;
    m_modified = snapshot.modified;
}

void BankDraft::pushUndo(std::string label)
{
    m_undo.push_back(capture(std::move(label)));
    while (m_undo.size() > kMaxHistory) {
        m_undo.pop_front();
    }
    // A new edit ends the redo branch: redoing into a history that no longer
    // leads anywhere is how an editor loses a user's work.
    m_redo.clear();
}

void BankDraft::recount()
{
    m_occupied = static_cast<int>(
        std::count_if(m_slots.begin(), m_slots.end(), [](const BankSlotContent& s) { return !s.empty(); }));
}

} // namespace xp60studio::library
