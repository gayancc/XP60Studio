#pragma once

#include "xpmodel/ParameterDescriptor.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace xp60studio::xpmodel {

// Whether a table describes every byte Roland documents for the block.
enum class TableCompleteness {
    // Every documented parameter is present; remaining bytes are reserved.
    Complete,
    // Only some parameters are transcribed. Decoding still preserves all
    // bytes, but callers must not present the block as fully understood.
    Partial,
};

[[nodiscard]] std::string_view tableCompletenessName(TableCompleteness completeness) noexcept;

// The parameter layout of one Roland data block (e.g. Patch Common).
class ParameterTable
{
public:
    struct Issue
    {
        enum class Kind {
            Descriptor,   // descriptor fails its own self-check
            OutOfBlock,   // parameter extends past blockSize
            Overlap,      // two parameters share a byte
            DuplicateId,
            Unsorted,     // parameters must be listed in ascending offset order
        };
        Kind kind;
        std::string detail;
    };

    using ByteRange = std::pair<std::uint32_t, std::uint32_t>; // [begin, end)

    ParameterTable(std::string_view blockName, std::uint32_t blockSize, std::span<const ParameterDescriptor> parameters,
                   TableCompleteness completeness, std::string_view sourceNote = {});

    [[nodiscard]] std::string_view blockName() const noexcept { return m_blockName; }
    [[nodiscard]] std::uint32_t blockSize() const noexcept { return m_blockSize; }
    [[nodiscard]] TableCompleteness completeness() const noexcept { return m_completeness; }
    [[nodiscard]] bool isComplete() const noexcept { return m_completeness == TableCompleteness::Complete; }
    [[nodiscard]] std::string_view sourceNote() const noexcept { return m_sourceNote; }
    [[nodiscard]] std::span<const ParameterDescriptor> parameters() const noexcept { return m_parameters; }
    [[nodiscard]] std::size_t size() const noexcept { return m_parameters.size(); }

    [[nodiscard]] const ParameterDescriptor* find(std::string_view id) const noexcept;
    [[nodiscard]] std::optional<std::size_t> indexOf(std::string_view id) const noexcept;
    [[nodiscard]] const ParameterDescriptor* atOffset(std::uint32_t offset) const noexcept;

    // Structural validation; an empty result means the table is usable.
    [[nodiscard]] std::vector<Issue> validate() const;

    // Byte ranges inside the block not covered by any parameter. They are
    // preserved verbatim by the codec.
    [[nodiscard]] std::vector<ByteRange> reservedRanges() const;
    [[nodiscard]] std::uint32_t describedByteCount() const noexcept;

private:
    std::string_view m_blockName;
    std::uint32_t m_blockSize;
    std::span<const ParameterDescriptor> m_parameters;
    TableCompleteness m_completeness;
    std::string_view m_sourceNote;
};

[[nodiscard]] std::string_view tableIssueKindName(ParameterTable::Issue::Kind kind) noexcept;

} // namespace xp60studio::xpmodel
