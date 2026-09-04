#include "xpmodel/Xp60PatchDiff.h"

#include <map>

namespace xp60studio::xpmodel {

namespace {

void compareBlock(const BlockValues& left, const BlockValues& right, const Xp60PatchLayout::Block& block,
                  std::vector<Xp60PatchDiff::Difference>& out)
{
    const auto parameters = left.table().parameters();
    for (std::size_t i = 0; i < parameters.size(); ++i) {
        const int l = left.rawAt(i);
        const int r = right.rawAt(i);
        if (l == r) {
            continue;
        }
        const auto& p = parameters[i];
        out.push_back(Xp60PatchDiff::Difference{
            std::string(block.name),
            block.tone,
            std::string(p.id),
            std::string(p.name),
            std::string(p.category),
            p.offset,
            block.offset + p.offset,
            l,
            r,
            p.formatDisplay(l),
            p.formatDisplay(r),
        });
    }
}

} // namespace

Xp60PatchDiff Xp60PatchDiff::compare(const Xp60Patch& left, const Xp60Patch& right)
{
    Xp60PatchDiff diff;
    for (const auto& block : Xp60PatchLayout::blocks()) {
        if (block.tone) {
            compareBlock(left.tone(*block.tone), right.tone(*block.tone), block, diff.m_differences);
        } else {
            compareBlock(left.common(), right.common(), block, diff.m_differences);
        }
    }
    return diff;
}

std::vector<Xp60PatchDiff::Difference> Xp60PatchDiff::forBlock(std::string_view block) const
{
    std::vector<Difference> out;
    for (const auto& d : m_differences) {
        if (d.block == block) {
            out.push_back(d);
        }
    }
    return out;
}

std::string Xp60PatchDiff::describe(std::size_t limit) const
{
    if (m_differences.empty()) {
        return "identical";
    }
    std::string out;
    std::size_t shown = 0;
    for (const auto& d : m_differences) {
        if (shown == limit) {
            out += "... and " + std::to_string(m_differences.size() - shown) + " more\n";
            break;
        }
        out += d.block + " " + d.parameterName + ": " + d.leftText + " -> " + d.rightText + "  (raw "
            + std::to_string(d.leftRaw) + " -> " + std::to_string(d.rightRaw) + ", byte +"
            + std::to_string(d.blockOffset) + ")\n";
        ++shown;
    }
    return out;
}

std::string Xp60PatchDiff::summary() const
{
    if (m_differences.empty()) {
        return "identical";
    }
    // Counts per block, in layout order.
    std::map<std::string, std::size_t> counts;
    for (const auto& d : m_differences) {
        ++counts[d.block];
    }
    std::string out = std::to_string(m_differences.size());
    out += m_differences.size() == 1 ? " parameter differs (" : " parameters differ (";
    bool first = true;
    for (const auto& block : Xp60PatchLayout::blocks()) {
        const auto it = counts.find(std::string(block.name));
        if (it == counts.end()) {
            continue;
        }
        if (!first) {
            out += ", ";
        }
        out += it->first + " " + std::to_string(it->second);
        first = false;
    }
    out += ")";
    return out;
}

} // namespace xp60studio::xpmodel
