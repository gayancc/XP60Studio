#pragma once

#include "xpmodel/Xp60Patch.h"
#include "xpmodel/Xp60PatchLayout.h"

#include <optional>
#include <string>
#include <vector>

namespace xp60studio::xpmodel {

// Parameter-level comparison of two Patches.
//
// Used to explain a read-back mismatch in exact terms ("Tone 2 Cutoff
// Frequency: sent 84, read back 83") instead of reporting that some bytes
// differ. Differences are never suppressed or normalised.
class Xp60PatchDiff
{
public:
    struct Difference
    {
        std::string block;             // "Patch Common", "Tone 1", ...
        std::optional<ToneIndex> tone; // nullopt for Common
        std::string parameterId;
        std::string parameterName;
        std::string category;
        std::uint32_t blockOffset = 0; // byte offset inside the block
        std::uint32_t patchOffset = 0; // byte offset from the Patch base
        int leftRaw = 0;
        int rightRaw = 0;
        std::string leftText;          // display text, e.g. "HALL1" or "+12"
        std::string rightText;
    };

    // `left` is conventionally what was sent / expected, `right` what came back.
    [[nodiscard]] static Xp60PatchDiff compare(const Xp60Patch& left, const Xp60Patch& right);

    [[nodiscard]] const std::vector<Difference>& differences() const noexcept { return m_differences; }
    [[nodiscard]] bool identical() const noexcept { return m_differences.empty(); }
    [[nodiscard]] std::size_t count() const noexcept { return m_differences.size(); }

    // Differences confined to one block, in block order.
    [[nodiscard]] std::vector<Difference> forBlock(std::string_view block) const;

    // "Tone 2 Cutoff Frequency: 84 -> 83" per line, at most `limit` lines.
    [[nodiscard]] std::string describe(std::size_t limit = 20) const;
    // One line: "3 parameters differ (Patch Common 1, Tone 2 2)".
    [[nodiscard]] std::string summary() const;

private:
    std::vector<Difference> m_differences;
};

} // namespace xp60studio::xpmodel
