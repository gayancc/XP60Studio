#include "library/PatchSimilarity.h"

#include "xpmodel/Xp60PatchLayout.h"

#include <algorithm>
#include <cmath>

namespace xp60studio::library {

using xpmodel::Xp60PatchDiff;
using xpmodel::Xp60PatchLayout;

namespace {

// The twelve Patch Name bytes. Excluded from the sound score and reported
// separately; see the class comment.
bool isNameParameter(const xpmodel::ParameterDescriptor& parameter) noexcept
{
    return parameter.category == "Name";
}

} // namespace

double PatchSimilarity::Block::score() const noexcept
{
    return comparedParameters == 0 ? 1.0
                                   : static_cast<double>(equalParameters) / comparedParameters;
}

PatchSimilarity PatchSimilarity::compare(const xpmodel::Xp60Patch& left, const xpmodel::Xp60Patch& right)
{
    PatchSimilarity result;
    result.m_diff = Xp60PatchDiff::compare(left, right);
    result.m_nameEqual = left.name() == right.name();

    for (const auto& block : Xp60PatchLayout::blocks()) {
        Block summary;
        summary.block = std::string(block.name);

        // Count the block's own parameters rather than its bytes: a two-byte
        // nibble value is one thing a musician changed, not two.
        int compared = 0;
        for (const auto& parameter : block.table->parameters()) {
            if (!isNameParameter(parameter)) {
                ++compared;
            }
        }
        int differing = 0;
        for (const auto& difference : result.m_diff.forBlock(block.name)) {
            // The diff reports every difference including the name bytes; the
            // score is over sound parameters, so name differences are counted
            // by nameEqual() instead.
            if (difference.category != "Name") {
                ++differing;
            }
        }
        summary.comparedParameters = compared;
        summary.equalParameters = compared - differing;
        result.m_compared += compared;
        result.m_equal += summary.equalParameters;
        result.m_blocks.push_back(std::move(summary));
    }
    return result;
}

double PatchSimilarity::score() const noexcept
{
    return m_compared == 0 ? 1.0 : static_cast<double>(m_equal) / m_compared;
}

bool PatchSimilarity::identicalSound() const noexcept
{
    return m_equal == m_compared;
}

bool PatchSimilarity::identical() const noexcept
{
    return identicalSound() && m_nameEqual;
}

int PatchSimilarity::percent() const noexcept
{
    if (identicalSound()) {
        return 100;
    }
    // Never round up to 100 for Patches that actually differ: "100%" has to
    // mean identical, or the number stops being trustworthy exactly where a
    // musician is relying on it.
    const int rounded = static_cast<int>(std::lround(score() * 100.0));
    return std::min(rounded, 99);
}

std::string PatchSimilarity::summary() const
{
    if (identical()) {
        return "Identical.";
    }
    if (identicalSound()) {
        return "The same sound under a different name.";
    }

    std::string out = std::to_string(percent()) + "% alike \xE2\x80\x94 "
        + std::to_string(differingParameters())
        + (differingParameters() == 1 ? " parameter differs" : " parameters differ");

    // Name the blocks that actually moved, worst first: "Tone 3 4, Patch Common
    // 1" is what tells a musician where to look.
    auto ranked = m_blocks;
    std::stable_sort(ranked.begin(), ranked.end(), [](const Block& a, const Block& b) {
        return a.differingParameters() > b.differingParameters();
    });
    std::string parts;
    for (const auto& block : ranked) {
        if (block.differingParameters() == 0) {
            continue;
        }
        if (!parts.empty()) {
            parts += ", ";
        }
        parts += block.block + " " + std::to_string(block.differingParameters());
    }
    if (!parts.empty()) {
        out += " (" + parts + ")";
    }
    out += ".";
    if (!m_nameEqual) {
        out += " The names differ too.";
    }
    return out;
}

} // namespace xp60studio::library
