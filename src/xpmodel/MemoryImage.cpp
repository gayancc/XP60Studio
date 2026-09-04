#include "xpmodel/MemoryImage.h"

#include <algorithm>

namespace xp60studio::xpmodel {

bool MemoryImage::write(const roland::RolandAddress& address, roland::ByteSpan data)
{
    if (data.empty()) {
        return true;
    }
    const std::uint64_t begin = address.value();
    const std::uint64_t end = begin + data.size();
    if (end > static_cast<std::uint64_t>(roland::RolandSize::kMaxValue) + 1) {
        return false;
    }
    ++m_writes;

    // Collect every run touching [begin, end) (including adjacent runs so
    // they merge into one).
    auto first = m_runs.lower_bound(static_cast<std::uint32_t>(begin));
    if (first != m_runs.begin()) {
        auto previous = std::prev(first);
        if (previous->first + previous->second.size() >= begin) {
            first = previous;
        }
    }
    auto last = first;
    std::uint64_t mergedBegin = begin;
    std::uint64_t mergedEnd = end;
    while (last != m_runs.end() && last->first <= end) {
        mergedBegin = std::min<std::uint64_t>(mergedBegin, last->first);
        mergedEnd = std::max<std::uint64_t>(mergedEnd, last->first + last->second.size());
        ++last;
    }

    roland::ByteVector merged(static_cast<std::size_t>(mergedEnd - mergedBegin), 0);
    std::vector<bool> present(merged.size(), false);
    for (auto it = first; it != last; ++it) {
        const std::size_t at = static_cast<std::size_t>(it->first - mergedBegin);
        std::copy(it->second.begin(), it->second.end(), merged.begin() + static_cast<std::ptrdiff_t>(at));
        std::fill(present.begin() + static_cast<std::ptrdiff_t>(at),
                  present.begin() + static_cast<std::ptrdiff_t>(at + it->second.size()), true);
    }
    // Count bytes that were already present in the new range.
    const std::size_t newAt = static_cast<std::size_t>(begin - mergedBegin);
    const std::size_t overlapping = static_cast<std::size_t>(
        std::count(present.begin() + static_cast<std::ptrdiff_t>(newAt),
                   present.begin() + static_cast<std::ptrdiff_t>(newAt + data.size()), true));
    if (overlapping != 0) {
        ++m_overlaps;
    }
    std::copy(data.begin(), data.end(), merged.begin() + static_cast<std::ptrdiff_t>(newAt));
    m_byteCount += data.size() - overlapping;

    m_runs.erase(first, last);
    m_runs.emplace(static_cast<std::uint32_t>(mergedBegin), std::move(merged));
    return true;
}

bool MemoryImage::addDataSet(const roland::RolandSysExMessage& message)
{
    if (!message.isDataSet()) {
        return false;
    }
    return write(message.address(), message.data());
}

std::optional<roland::ByteVector> MemoryImage::read(const roland::RolandAddress& address, std::uint32_t byteCount) const
{
    if (byteCount == 0) {
        return roland::ByteVector{};
    }
    const std::uint64_t begin = address.value();
    const std::uint64_t end = begin + byteCount;
    auto it = m_runs.upper_bound(static_cast<std::uint32_t>(begin));
    if (it == m_runs.begin()) {
        return std::nullopt;
    }
    --it;
    const std::uint64_t runEnd = it->first + it->second.size();
    if (it->first > begin || runEnd < end) {
        return std::nullopt;
    }
    const auto start = it->second.begin() + static_cast<std::ptrdiff_t>(begin - it->first);
    return roland::ByteVector(start, start + byteCount);
}

std::optional<roland::Byte> MemoryImage::byteAt(const roland::RolandAddress& address) const noexcept
{
    const auto bytes = read(address, 1);
    if (!bytes) {
        return std::nullopt;
    }
    return (*bytes)[0];
}

bool MemoryImage::contains(const roland::RolandAddress& address, std::uint32_t byteCount) const noexcept
{
    return read(address, byteCount).has_value();
}

MemoryImage::Coverage MemoryImage::coverage(const roland::RolandAddress& address, std::uint32_t byteCount) const
{
    Coverage result;
    result.requested = byteCount;
    const std::uint64_t begin = address.value();
    const std::uint64_t end = begin + byteCount;
    std::uint64_t cursor = begin;
    auto it = m_runs.upper_bound(static_cast<std::uint32_t>(begin));
    if (it != m_runs.begin()) {
        --it;
    }
    for (; it != m_runs.end() && it->first < end; ++it) {
        const std::uint64_t runBegin = it->first;
        const std::uint64_t runEnd = runBegin + it->second.size();
        if (runEnd <= cursor) {
            continue;
        }
        if (runBegin > cursor && !result.firstMissing) {
            result.firstMissing = roland::RolandAddress::fromValue(cursor);
        }
        const std::uint64_t coveredBegin = std::max(cursor, runBegin);
        const std::uint64_t coveredEnd = std::min(end, runEnd);
        if (coveredEnd > coveredBegin) {
            result.covered += static_cast<std::uint32_t>(coveredEnd - coveredBegin);
        }
        cursor = std::max(cursor, coveredEnd);
        if (cursor >= end) {
            break;
        }
    }
    if (cursor < end && !result.firstMissing) {
        result.firstMissing = roland::RolandAddress::fromValue(cursor);
    }
    return result;
}

std::vector<MemoryImage::Range> MemoryImage::ranges() const
{
    std::vector<Range> out;
    out.reserve(m_runs.size());
    for (const auto& [start, bytes] : m_runs) {
        out.push_back(Range{roland::RolandAddress::fromValue(start).value(), static_cast<std::uint32_t>(bytes.size())});
    }
    return out;
}

void MemoryImage::clear()
{
    m_runs.clear();
    m_byteCount = 0;
    m_overlaps = 0;
    m_writes = 0;
}

} // namespace xp60studio::xpmodel
