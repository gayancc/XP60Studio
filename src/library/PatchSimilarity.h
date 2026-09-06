#pragma once

#include "xpmodel/Xp60Patch.h"
#include "xpmodel/Xp60PatchDiff.h"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace xp60studio::library {

// A Patch reduced to the raw values of its documented sound parameters, in
// layout order (Patch Common, then Tone 1..4), with the twelve name bytes left
// out — exactly the parameters `PatchSimilarity` scores over.
//
// This exists because sweeping a library is quadratic. `PatchSimilarity`
// answers one pair richly: it builds a full diff with parameter names and
// display text, which costs about a millisecond. That is the right price for
// the one comparison a Compare screen shows, and the wrong price for the
// 500,000 comparisons a thousand-Patch library needs — half an hour of them.
//
// A signature costs one decode and then compares as an integer scan. Same
// definition of similarity, same numbers, no strings. `atLeast()` stops as soon
// as too many positions have differed to reach the caller's threshold, which is
// what makes a full sweep finish: most pairs of unrelated sounds fail within a
// few dozen parameters.
class PatchSignature
{
public:
    [[nodiscard]] static PatchSignature of(const xpmodel::Xp60Patch& patch);

    // Positions where both Patches carry the same raw value.
    [[nodiscard]] static int equalCount(const PatchSignature& left,
                                        const PatchSignature& right) noexcept;
    // The same 0.0..1.0 number `PatchSimilarity::score()` reports.
    [[nodiscard]] static double score(const PatchSignature& left,
                                      const PatchSignature& right) noexcept;
    // True when at least `minimumEqual` positions match, abandoning the scan as
    // soon as the answer can no longer be yes.
    [[nodiscard]] static bool atLeast(const PatchSignature& left,
                                      const PatchSignature& right,
                                      int minimumEqual) noexcept;

    [[nodiscard]] int parameterCount() const noexcept { return static_cast<int>(m_values.size()); }
    [[nodiscard]] std::span<const int> values() const noexcept { return m_values; }

private:
    std::vector<int> m_values;
};

// How alike two Patches are, and exactly where they differ.
//
// `PatchFingerprint` answers "are these the same sound?" — one bit, and no help
// when the answer is no. This answers "how nearly the same, and in what?", which
// is the question a musician asks of a library that has accumulated a decade of
// edits, resaves and near-misses.
//
// ── How similarity is defined, and why this way ──────────────────────────────
//
// **The fraction of documented parameters that are equal.** Every parameter in
// the Roland Parameter Address Map counts once, unweighted.
//
// Unweighted is a deliberate refusal, not an oversight. Weighting would be a
// claim that some parameters matter more to how a Patch sounds — that Cutoff
// outranks Chorus Send, say. Roland documents no such ranking, this project
// cannot measure one, and a score built on invented weights would be a number
// that looks objective while encoding one person's guess. An unweighted count
// is a fact about the data: *this many of the instrument's own parameters
// agree*.
//
// Counted per **parameter**, not per byte, so a two-byte nibble value such as
// Wave Number counts once rather than twice.
//
// ── The name is not part of the sound ────────────────────────────────────────
//
// `score()` excludes the twelve name bytes. Two Patches that sound identical but
// are called `Strings 1` and `Strings 1b` are the near-duplicates a librarian is
// hunting; letting a rename move the score would bury exactly the case the
// feature exists for. `nameEqual()` reports the name separately, so nothing is
// hidden — it is reported apart rather than folded in.
//
// This is not hypothetical. The project's one real 128-Patch User bank holds
// twelve pairs that are the same sound: eleven exact duplicates, and `Vocal
// Fall 1` / `Vocal Fall 2`, which differ in exactly one byte of 2816 — the
// twelfth name character. `PatchFingerprint` calls that last pair two different
// Patches, correctly for its own question and uselessly for this one.
//
// Pure: no I/O, no clock, no Qt. Derived metadata, never written back into
// Roland data.
class PatchSimilarity
{
public:
    // Similarity of one block.
    struct Block
    {
        std::string block;          // "Patch Common", "Tone 1", ...
        int comparedParameters = 0;
        int equalParameters = 0;
        [[nodiscard]] int differingParameters() const noexcept
        {
            return comparedParameters - equalParameters;
        }
        // 0.0 .. 1.0, or 1.0 for a block with nothing to compare.
        [[nodiscard]] double score() const noexcept;
    };

    [[nodiscard]] static PatchSimilarity compare(const xpmodel::Xp60Patch& left,
                                                 const xpmodel::Xp60Patch& right);

    // 0.0 .. 1.0 over the sound parameters — the name is excluded, see above.
    [[nodiscard]] double score() const noexcept;
    // Same parameters *and* the same name: indistinguishable Patches.
    [[nodiscard]] bool identical() const noexcept;
    // Same sound, whatever they are called. This is what "near-duplicate"
    // hunting is really looking for when it finds an exact match.
    [[nodiscard]] bool identicalSound() const noexcept;
    [[nodiscard]] bool nameEqual() const noexcept { return m_nameEqual; }

    [[nodiscard]] int comparedParameters() const noexcept { return m_compared; }
    [[nodiscard]] int equalParameters() const noexcept { return m_equal; }
    [[nodiscard]] int differingParameters() const noexcept { return m_compared - m_equal; }

    // Per block, in layout order: Patch Common then Tone 1..4. A musician
    // reading "Tone 3 is 96% the same, the rest identical" learns more than a
    // single number, and it is what the Compare screen draws.
    [[nodiscard]] const std::vector<Block>& blocks() const noexcept { return m_blocks; }

    // Every parameter that differs, with both values in display form. The same
    // list the transfer mismatch report uses, so there is one explanation of a
    // difference in the application rather than two.
    [[nodiscard]] const xpmodel::Xp60PatchDiff& differences() const noexcept { return m_diff; }

    // "97% alike — 5 parameters differ (Tone 3 4, Patch Common 1)". Includes a
    // note when only the name differs, because that is the case most easily
    // misread as a real difference.
    [[nodiscard]] std::string summary() const;

    // Score expressed the way a UI shows it: 0..100, rounded to the nearest
    // whole percent, but never rounded *up* to 100 for Patches that actually
    // differ — "100%" must mean identical.
    [[nodiscard]] int percent() const noexcept;

private:
    xpmodel::Xp60PatchDiff m_diff;
    std::vector<Block> m_blocks;
    int m_compared = 0;
    int m_equal = 0;
    bool m_nameEqual = true;
};

} // namespace xp60studio::library
