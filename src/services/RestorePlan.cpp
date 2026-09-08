#include "services/RestorePlan.h"

#include "xp60/Xp60Device.h"
#include "xpmodel/SysExStream.h"
#include "xpmodel/Xp60PatchLayout.h"
#include "xpmodel/Xp60PerformanceLayout.h"

#include <algorithm>
#include <array>

namespace xp60studio::services {

using roland::RolandAddress;

namespace {

// The bounds of each area, from the documented Parameter Address Map
// (`ROLAND_XP60_PROTOCOL_FACTS.md` §3). An area runs from its base to the byte
// before the next region begins, so a restore covers the whole bank including
// the address space Roland leaves between its entries.
struct AreaBounds
{
    std::uint32_t begin = 0;
    std::uint32_t end = 0;  // exclusive
};

AreaBounds boundsOf(RestoreArea area)
{
    // Linear 28-bit value of `b0 b1 00 00`, packed the way SevenBitQuad does:
    // seven significant bits per byte. Computed rather than constructed —
    // `RolandAddress(b0, b1, 0x00, 0x00)` throws on a byte with bit 7 set, and
    // a throwing expression has no business in a bounds table.
    const auto base = [](std::uint32_t b0, std::uint32_t b1) {
        return ((b0 & 0x7Fu) << 21) | ((b1 & 0x7Fu) << 14);
    };
    // Every multi-slot area in this map uses the same stride, 00 01 00 00.
    // The end is computed by arithmetic rather than by naming an address: the
    // byte after User Patch 128 is 11 80 00 00, and 0x80 is not a legal Roland
    // address byte.
    constexpr std::uint32_t kStride = 0x01u << 14;
    switch (area) {
    case RestoreArea::System:
        // System Common plus the seventeen Scale Tune blocks, up to the
        // Temporary Performance at 01 00 00 00.
        return AreaBounds{base(0x00, 0x00), base(0x01, 0x00)};
    case RestoreArea::UserPerformances:
        return AreaBounds{base(0x10, 0x00), base(0x10, 0x00) + 32 * kStride};
    case RestoreArea::UserRhythmSetups:
        return AreaBounds{base(0x10, 0x40), base(0x10, 0x40) + 2 * kStride};
    case RestoreArea::UserPatches:
        return AreaBounds{base(0x11, 0x00), base(0x11, 0x00) + 128 * kStride};
    }
    return AreaBounds{};
}

// How many slots the area has, per the Parameter Address Map.
int expectedEntriesOf(RestoreArea area) noexcept
{
    switch (area) {
    case RestoreArea::UserPatches:
        return 128;
    case RestoreArea::UserPerformances:
        return 32;
    case RestoreArea::UserRhythmSetups:
        return 2;
    case RestoreArea::System:
        return 1;
    }
    return 0;
}

// Where each slot of the area starts, in ascending order.
std::vector<std::uint32_t> entryBasesOf(RestoreArea area)
{
    const auto bounds = boundsOf(area);
    std::vector<std::uint32_t> bases;
    const int count = expectedEntriesOf(area);
    if (count <= 1) {
        bases.push_back(bounds.begin);
        return bases;
    }
    // Every multi-slot area in this map uses the same stride, 00 01 00 00.
    constexpr std::uint32_t kStride = 0x01u << 14;
    bases.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        bases.push_back(bounds.begin + static_cast<std::uint32_t>(i) * kStride);
    }
    return bases;
}

constexpr std::array<RestoreArea, 4> kAllAreas{RestoreArea::System, RestoreArea::UserPerformances,
                                               RestoreArea::UserRhythmSetups,
                                               RestoreArea::UserPatches};

struct Placed
{
    std::uint32_t begin = 0;
    std::uint32_t end = 0;
    const roland::ByteVector* message = nullptr;
};

std::vector<Placed> placeMessages(const library::InstrumentSnapshot& snapshot)
{
    std::vector<Placed> placed;
    placed.reserve(snapshot.messageCount());
    const std::vector<roland::RolandModelId> models{xp60::modelId()};
    for (const auto& message : snapshot.messages()) {
        const auto stream = xpmodel::parseSysExStream(message, models);
        for (const auto& item : stream.items) {
            if (!item.roland || !item.roland->isDataSet()) {
                continue;
            }
            const auto begin = item.roland->address().value();
            placed.push_back(Placed{begin,
                                    begin + static_cast<std::uint32_t>(item.roland->data().size()),
                                    &message});
        }
    }
    return placed;
}

// The documented blocks one slot of the area is made of, as (address, size).
// Empty when this build has no layout for the area, which is how "cannot
// check" is expressed rather than "nothing required".
std::vector<std::pair<std::uint32_t, std::uint32_t>> requiredBlocksOf(RestoreArea area,
                                                                      std::uint32_t base)
{
    std::vector<std::pair<std::uint32_t, std::uint32_t>> blocks;
    const auto address = RolandAddress::fromValue(base);
    if (!address) {
        return blocks;
    }
    if (area == RestoreArea::UserPatches) {
        for (const auto& request : xpmodel::Xp60PatchLayout::fetchPlan(*address)) {
            blocks.emplace_back(request.address.value(), request.size.value());
        }
    } else if (area == RestoreArea::UserPerformances) {
        for (const auto& request : xpmodel::Xp60PerformanceLayout::fetchPlan(*address)) {
            blocks.emplace_back(request.address.value(), request.size.value());
        }
    }
    return blocks;
}

bool spansCover(const std::vector<std::pair<std::uint32_t, std::uint32_t>>& sortedSpans,
                std::uint32_t begin, std::uint32_t end)
{
    std::uint32_t reach = begin;
    for (const auto& [spanBegin, spanEnd] : sortedSpans) {
        if (spanBegin > reach) {
            break;
        }
        reach = std::max(reach, spanEnd);
        if (reach >= end) {
            return true;
        }
    }
    return reach >= end;
}

} // namespace

std::string_view restoreAreaName(RestoreArea area) noexcept
{
    switch (area) {
    case RestoreArea::UserPatches:
        return "User Patches";
    case RestoreArea::UserPerformances:
        return "User Performances";
    case RestoreArea::UserRhythmSetups:
        return "User Rhythm Setups";
    case RestoreArea::System:
        return "System settings";
    }
    return "Unknown area";
}

QString RestoreStep::describe() const
{
    const auto name = restoreAreaName(area);
    QString out = QStringLiteral("%1: %2 message(s), %3 of %4 slot(s)")
                      .arg(QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size())))
                      .arg(messages.size())
                      .arg(entriesWithData)
                      .arg(expectedEntries);
    if (completenessChecked()) {
        out += QStringLiteral(", %1 complete").arg(completeEntries);
    } else {
        out += QStringLiteral(", completeness not checked");
    }
    return out;
}

RestorePlan RestorePlan::build(const library::InstrumentSnapshot& snapshot,
                               const std::vector<RestoreArea>& areas)
{
    RestorePlan plan;
    const auto placed = placeMessages(snapshot);

    for (const auto area : areas) {
        const auto bounds = boundsOf(area);
        RestoreStep step;
        step.area = area;
        if (const auto begin = RolandAddress::fromValue(bounds.begin)) {
            step.begin = *begin;
        }
        step.expectedEntries = expectedEntriesOf(area);

        // Wholly inside. A message straddling an area boundary belongs to
        // neither: writing part of it would be writing outside what was asked
        // for, which rule 2 forbids.
        std::vector<std::pair<std::uint32_t, std::uint32_t>> spans;
        for (const auto& entry : placed) {
            if (entry.begin >= bounds.begin && entry.end <= bounds.end) {
                spans.emplace_back(entry.begin, entry.end);
                step.messages.push_back(*entry.message);
            }
        }
        if (spans.empty()) {
            plan.m_unavailable.push_back(area);
            continue;
        }
        std::sort(spans.begin(), spans.end());

        const auto bases = entryBasesOf(area);
        constexpr std::uint32_t kStride = 0x01u << 14;
        int complete = 0;
        bool checkable = false;
        for (std::size_t i = 0; i < bases.size(); ++i) {
            const std::uint32_t slotBegin = bases[i];
            const std::uint32_t slotEnd
                = bases.size() == 1 ? bounds.end : std::min(slotBegin + kStride, bounds.end);
            const bool anyData = std::any_of(spans.begin(), spans.end(), [&](const auto& span) {
                return span.first < slotEnd && span.second > slotBegin;
            });
            if (anyData) {
                ++step.entriesWithData;
            }
            const auto required = requiredBlocksOf(area, slotBegin);
            if (required.empty()) {
                continue;
            }
            checkable = true;
            const bool whole = std::all_of(required.begin(), required.end(), [&](const auto& block) {
                return spansCover(spans, block.first, block.first + block.second);
            });
            if (whole) {
                ++complete;
            }
        }
        step.completeEntries = checkable ? complete : -1;
        step.covered = step.entriesWithData == step.expectedEntries
            && (!checkable || complete == step.expectedEntries);

        if (!step.covered) {
            plan.m_partial.push_back(area);
        }
        plan.m_steps.push_back(std::move(step));
    }
    return plan;
}

RestorePlan RestorePlan::buildForEverythingCovered(const library::InstrumentSnapshot& snapshot)
{
    std::vector<RestoreArea> areas;
    const auto placed = placeMessages(snapshot);
    for (const auto area : kAllAreas) {
        const auto bounds = boundsOf(area);
        const bool any = std::any_of(placed.begin(), placed.end(), [&bounds](const Placed& entry) {
            return entry.begin >= bounds.begin && entry.end <= bounds.end;
        });
        if (any) {
            areas.push_back(area);
        }
    }
    return build(snapshot, areas);
}

std::size_t RestorePlan::messageCount() const noexcept
{
    std::size_t total = 0;
    for (const auto& step : m_steps) {
        total += step.messages.size();
    }
    return total;
}

std::size_t RestorePlan::byteCount() const noexcept
{
    std::size_t total = 0;
    for (const auto& step : m_steps) {
        for (const auto& message : step.messages) {
            total += message.size();
        }
    }
    return total;
}

std::vector<roland::ByteVector> RestorePlan::messages() const
{
    std::vector<roland::ByteVector> out;
    out.reserve(messageCount());
    for (const auto& step : m_steps) {
        out.insert(out.end(), step.messages.begin(), step.messages.end());
    }
    return out;
}

QStringList RestorePlan::warnings() const
{
    const auto nameOf = [](RestoreArea area) {
        const auto name = restoreAreaName(area);
        return QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size()));
    };
    QStringList out;
    for (const auto area : m_unavailable) {
        out << QStringLiteral("This snapshot contains no %1, so they cannot be restored.")
                   .arg(nameOf(area));
    }
    for (const auto& step : m_steps) {
        if (step.covered) {
            if (!step.completenessChecked()) {
                out << QStringLiteral("This build has no block layout for %1, so whether the "
                                      "snapshot holds all of each one could not be checked. What "
                                      "it holds will be written.")
                           .arg(nameOf(step.area));
            }
            continue;
        }
        if (step.entriesWithData < step.expectedEntries) {
            out << QStringLiteral("This snapshot holds %1 of the %2 %3. Only those will be "
                                  "written; the rest will be left as they are.")
                       .arg(step.entriesWithData)
                       .arg(step.expectedEntries)
                       .arg(nameOf(step.area));
        }
        if (step.completenessChecked() && step.completeEntries < step.entriesWithData) {
            out << QStringLiteral("%1 of the %2 %3 in this snapshot are missing some of their "
                                  "data and would be restored incomplete.")
                       .arg(step.entriesWithData - step.completeEntries)
                       .arg(step.entriesWithData)
                       .arg(nameOf(step.area));
        }
    }
    if (writesAnything()) {
        out << QStringLiteral("Restoring overwrites what is on the instrument now. Take a snapshot "
                              "of the current contents first.");
    }
    return out;
}

QString RestorePlan::summary() const
{
    if (!writesAnything()) {
        return QStringLiteral("Nothing to restore: this snapshot holds none of the areas asked for.");
    }
    QStringList parts;
    for (const auto& step : m_steps) {
        parts << step.describe();
    }
    return QStringLiteral("%1 message(s) across %2 area(s) \xE2\x80\x94 %3")
        .arg(messageCount())
        .arg(m_steps.size())
        .arg(parts.join(QStringLiteral("; ")));
}

} // namespace xp60studio::services
