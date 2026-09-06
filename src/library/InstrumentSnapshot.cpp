#include "library/InstrumentSnapshot.h"

#include <QCryptographicHash>

#include <algorithm>

namespace xp60studio::library {

using roland::RolandAddress;

bool SnapshotRegion::contains(const RolandAddress& address, std::uint32_t bytes) const noexcept
{
    if (address < begin) {
        return false;
    }
    const auto offset = begin.distanceTo(address);
    if (!offset) {
        return false;
    }
    return static_cast<std::uint64_t>(*offset) + bytes <= byteCount;
}

std::string SnapshotRegion::describe() const
{
    std::string out = begin.toHexString();
    if (const auto last = begin.plus(byteCount == 0 ? 0 : byteCount - 1)) {
        out += "\xE2\x80\x93" + last->toHexString();
    }
    return out + " (" + std::to_string(byteCount) + " bytes)";
}

// ---------------------------------------------------------------------------

InstrumentSnapshot InstrumentSnapshot::fromSysEx(roland::ByteSpan bytes,
                                                 std::span<const roland::RolandModelId> models,
                                                 Metadata metadata)
{
    InstrumentSnapshot snapshot;
    snapshot.m_metadata = std::move(metadata);
    snapshot.m_models.assign(models.begin(), models.end());

    const auto stream = xpmodel::parseSysExStream(bytes, models);
    snapshot.m_strayBytes = stream.strayBytes + stream.abortedSysEx;
    snapshot.m_rejected = stream.rejectedRolandCount;
    for (const auto& item : stream.items) {
        // Only Data Sets carry memory contents. A request in the file is not a
        // capture of anything, and neither is another manufacturer's SysEx.
        if (item.roland && item.roland->isDataSet()) {
            snapshot.m_messages.push_back(item.raw);
        }
    }
    snapshot.rebuildRegions();
    return snapshot;
}

InstrumentSnapshot InstrumentSnapshot::fromDataSets(std::vector<roland::ByteVector> messages,
                                                    std::span<const roland::RolandModelId> models,
                                                    Metadata metadata)
{
    InstrumentSnapshot snapshot;
    snapshot.m_metadata = std::move(metadata);
    snapshot.m_models.assign(models.begin(), models.end());
    for (auto& message : messages) {
        const auto stream = xpmodel::parseSysExStream(message, models);
        const bool isDataSet = stream.items.size() == 1 && stream.items.front().roland
            && stream.items.front().roland->isDataSet();
        if (isDataSet) {
            snapshot.m_messages.push_back(std::move(message));
        } else {
            // Counted, never dropped silently: a snapshot that quietly lost a
            // message would restore an instrument to a state it was never in.
            ++snapshot.m_rejected;
        }
    }
    snapshot.rebuildRegions();
    return snapshot;
}

void InstrumentSnapshot::rebuildRegions()
{
    m_regions.clear();
    if (m_messages.empty()) {
        return;
    }

    struct Span
    {
        std::uint32_t begin = 0;
        std::uint32_t end = 0;  // exclusive
    };
    std::vector<Span> spans;
    spans.reserve(m_messages.size());
    for (const auto& message : m_messages) {
        const auto stream = xpmodel::parseSysExStream(message, m_models);
        for (const auto& item : stream.items) {
            if (!item.roland || !item.roland->isDataSet()) {
                continue;
            }
            const auto begin = item.roland->address().value();
            spans.push_back(Span{begin, begin + static_cast<std::uint32_t>(item.roland->data().size())});
        }
    }
    if (spans.empty()) {
        return;
    }

    std::sort(spans.begin(), spans.end(),
              [](const Span& a, const Span& b) { return a.begin < b.begin; });

    // Coalesce touching and overlapping spans, so a bank read one Patch at a
    // time describes itself as the one region it is rather than as 128.
    std::vector<Span> merged;
    std::vector<int> counts;
    for (const auto& span : spans) {
        if (!merged.empty() && span.begin <= merged.back().end) {
            merged.back().end = std::max(merged.back().end, span.end);
            ++counts.back();
            continue;
        }
        merged.push_back(span);
        counts.push_back(1);
    }

    m_regions.reserve(merged.size());
    for (std::size_t i = 0; i < merged.size(); ++i) {
        const auto begin = RolandAddress::fromValue(merged[i].begin);
        if (!begin) {
            continue;
        }
        m_regions.push_back(
            SnapshotRegion{*begin, merged[i].end - merged[i].begin, counts[i]});
    }
}

std::size_t InstrumentSnapshot::byteCount() const noexcept
{
    std::size_t total = 0;
    for (const auto& message : m_messages) {
        total += message.size();
    }
    return total;
}

bool InstrumentSnapshot::covers(const RolandAddress& address, std::uint32_t byteCount) const noexcept
{
    // Whole-range or nothing. Restoring part of a Patch would leave the
    // instrument holding a Patch that never existed.
    return std::any_of(m_regions.begin(), m_regions.end(),
                       [&](const SnapshotRegion& region) { return region.contains(address, byteCount); });
}

std::string InstrumentSnapshot::digest() const
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    for (const auto& message : m_messages) {
        hash.addData(QByteArrayView(reinterpret_cast<const char*>(message.data()),
                                    static_cast<qsizetype>(message.size())));
    }
    return hash.result().toHex().toStdString();
}

roland::ByteVector InstrumentSnapshot::toSysEx() const
{
    roland::ByteVector out;
    out.reserve(byteCount());
    for (const auto& message : m_messages) {
        out.insert(out.end(), message.begin(), message.end());
    }
    return out;
}

std::string InstrumentSnapshot::summary() const
{
    if (isEmpty()) {
        return "Empty snapshot: nothing was captured.";
    }
    std::uint64_t covered = 0;
    for (const auto& region : m_regions) {
        covered += region.byteCount;
    }
    std::string out = std::to_string(covered) + " bytes in " + std::to_string(m_regions.size())
        + (m_regions.size() == 1 ? " region" : " regions") + ", " + std::to_string(m_messages.size())
        + (m_messages.size() == 1 ? " message" : " messages") + ".";
    if (!isClean()) {
        out += " " + std::to_string(m_rejected) + " message"
            + (m_rejected == 1 ? "" : "s") + " could not be read";
        if (m_strayBytes > 0) {
            out += " and " + std::to_string(m_strayBytes) + " stray bytes were found";
        }
        out += "; this snapshot is not a complete capture.";
    }
    return out;
}

} // namespace xp60studio::library
