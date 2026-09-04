#pragma once

#include "roland/RolandAddress.h"
#include "roland/RolandSysExMessage.h"
#include "roland/RolandTypes.h"

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace xp60studio::xpmodel {

// Sparse image of Roland device memory assembled from DT1 messages.
//
// Addresses are the 28-bit linear form of RolandAddress. Bytes are stored in
// merged contiguous runs, so a Patch, a bank or a whole snapshot can be laid
// down in any chunk order and read back by address range. Overlapping writes
// overwrite (latest wins) and are counted so a caller can report them.
class MemoryImage
{
public:
    struct Range
    {
        roland::RolandAddress begin;
        std::uint32_t byteCount = 0;

        [[nodiscard]] std::optional<roland::RolandAddress> end() const noexcept { return begin.plus(byteCount); }
    };

    struct Coverage
    {
        std::uint32_t requested = 0;
        std::uint32_t covered = 0;
        std::optional<roland::RolandAddress> firstMissing;

        [[nodiscard]] bool complete() const noexcept { return requested == covered; }
    };

    MemoryImage() = default;

    // Returns false when the range would run past the address space.
    bool write(const roland::RolandAddress& address, roland::ByteSpan data);
    // Convenience: writes the payload of a DT1. Ignores RQ1 (returns false).
    bool addDataSet(const roland::RolandSysExMessage& message);

    [[nodiscard]] std::optional<roland::ByteVector> read(const roland::RolandAddress& address, std::uint32_t byteCount) const;
    [[nodiscard]] std::optional<roland::Byte> byteAt(const roland::RolandAddress& address) const noexcept;
    [[nodiscard]] bool contains(const roland::RolandAddress& address, std::uint32_t byteCount = 1) const noexcept;
    [[nodiscard]] Coverage coverage(const roland::RolandAddress& address, std::uint32_t byteCount) const;

    [[nodiscard]] std::vector<Range> ranges() const;
    [[nodiscard]] std::size_t byteCount() const noexcept { return m_byteCount; }
    [[nodiscard]] bool isEmpty() const noexcept { return m_runs.empty(); }
    [[nodiscard]] std::size_t overlappingWriteCount() const noexcept { return m_overlaps; }
    [[nodiscard]] std::size_t writeCount() const noexcept { return m_writes; }

    void clear();

private:
    // key: linear start address; value: bytes of the run.
    std::map<std::uint32_t, roland::ByteVector> m_runs;
    std::size_t m_byteCount = 0;
    std::size_t m_overlaps = 0;
    std::size_t m_writes = 0;
};

} // namespace xp60studio::xpmodel
