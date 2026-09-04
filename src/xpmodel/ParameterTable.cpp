#include "xpmodel/ParameterTable.h"

#include <algorithm>
#include <set>

namespace xp60studio::xpmodel {

std::string_view tableCompletenessName(TableCompleteness completeness) noexcept
{
    return completeness == TableCompleteness::Complete ? "Complete" : "Partial";
}

std::string_view tableIssueKindName(ParameterTable::Issue::Kind kind) noexcept
{
    using Kind = ParameterTable::Issue::Kind;
    switch (kind) {
    case Kind::Descriptor:
        return "Descriptor";
    case Kind::OutOfBlock:
        return "OutOfBlock";
    case Kind::Overlap:
        return "Overlap";
    case Kind::DuplicateId:
        return "DuplicateId";
    case Kind::Unsorted:
        return "Unsorted";
    }
    return "Unknown";
}

ParameterTable::ParameterTable(std::string_view blockName, std::uint32_t blockSize,
                               std::span<const ParameterDescriptor> parameters, TableCompleteness completeness,
                               std::string_view sourceNote)
    : m_blockName(blockName)
    , m_blockSize(blockSize)
    , m_parameters(parameters)
    , m_completeness(completeness)
    , m_sourceNote(sourceNote)
{
}

const ParameterDescriptor* ParameterTable::find(std::string_view id) const noexcept
{
    for (const auto& parameter : m_parameters) {
        if (parameter.id == id) {
            return &parameter;
        }
    }
    return nullptr;
}

std::optional<std::size_t> ParameterTable::indexOf(std::string_view id) const noexcept
{
    for (std::size_t i = 0; i < m_parameters.size(); ++i) {
        if (m_parameters[i].id == id) {
            return i;
        }
    }
    return std::nullopt;
}

const ParameterDescriptor* ParameterTable::atOffset(std::uint32_t offset) const noexcept
{
    for (const auto& parameter : m_parameters) {
        if (offset >= parameter.offset && offset < parameter.endOffset()) {
            return &parameter;
        }
    }
    return nullptr;
}

std::vector<ParameterTable::Issue> ParameterTable::validate() const
{
    std::vector<Issue> issues;
    std::set<std::string_view> ids;
    std::uint32_t previousEnd = 0;
    bool first = true;
    for (const auto& parameter : m_parameters) {
        if (const auto problem = parameter.selfCheck()) {
            issues.push_back({Issue::Kind::Descriptor, *problem});
        }
        if (!ids.insert(parameter.id).second) {
            issues.push_back({Issue::Kind::DuplicateId, "duplicate id '" + std::string(parameter.id) + "'"});
        }
        if (parameter.endOffset() > m_blockSize) {
            issues.push_back({Issue::Kind::OutOfBlock, std::string(parameter.id) + " ends at byte "
                                                            + std::to_string(parameter.endOffset()) + " but the block has "
                                                            + std::to_string(m_blockSize) + " bytes"});
        }
        if (!first && parameter.offset < previousEnd) {
            const auto* other = atOffset(parameter.offset);
            const std::string otherId =
                other && other != &parameter ? std::string(other->id) : std::string("the previous parameter");
            issues.push_back({Issue::Kind::Overlap, std::string(parameter.id) + " overlaps " + otherId + " at byte "
                                                        + std::to_string(parameter.offset)});
        }
        previousEnd = std::max(previousEnd, parameter.endOffset());
        first = false;
    }
    // Ordering check (independent of overlap reporting).
    for (std::size_t i = 1; i < m_parameters.size(); ++i) {
        if (m_parameters[i].offset < m_parameters[i - 1].offset) {
            issues.push_back({Issue::Kind::Unsorted, std::string(m_parameters[i].id) + " is listed before "
                                                        + std::string(m_parameters[i - 1].id) + " but has a lower offset"});
        }
    }
    return issues;
}

std::vector<ParameterTable::ByteRange> ParameterTable::reservedRanges() const
{
    std::vector<ByteRange> gaps;
    std::uint32_t cursor = 0;
    for (const auto& parameter : m_parameters) {
        if (parameter.offset > cursor) {
            gaps.emplace_back(cursor, parameter.offset);
        }
        cursor = std::max(cursor, parameter.endOffset());
    }
    if (cursor < m_blockSize) {
        gaps.emplace_back(cursor, m_blockSize);
    }
    return gaps;
}

std::uint32_t ParameterTable::describedByteCount() const noexcept
{
    std::uint32_t count = 0;
    for (const auto& parameter : m_parameters) {
        count += parameter.byteCount;
    }
    return count;
}

} // namespace xp60studio::xpmodel
