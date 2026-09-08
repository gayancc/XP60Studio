#pragma once

#include "library/PatchComponentCopy.h"
#include "xpmodel/Xp60Patch.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace xp60studio::library {

// One parameter a variation moved.
struct VariationChange
{
    std::string block;      // "Patch Common", "Tone 2", ...
    std::string parameterName;
    std::string category;
    int before = 0;
    int after = 0;
};

struct PatchVariationOptions
{
    // How far a parameter may move, as a percentage of **its own** documented
    // range. A range is not a unit: ±5% of a 0..127 field is about ±6, and of a
    // 0..15 field is about ±1. Using an absolute step instead would move a
    // coarse parameter across its whole range while barely touching a fine one.
    int amountPercent = 5;
    // Fraction of eligible parameters to touch at all, 1..100. A variation that
    // nudges everything is a different sound; one that nudges a scattering of
    // things is a variation.
    int densityPercent = 40;
    // Which components may be varied. Empty means every one that has any
    // continuous parameters.
    std::vector<PatchComponent> components;
    // Which Tones may be varied. Empty means all four. A Tone that is switched
    // off is never varied whatever this says: changing a Tone nobody hears is
    // change without effect.
    std::vector<xpmodel::ToneIndex> tones;
    // Deterministic. The same seed, Patch and options always give the same
    // result, so a variation a musician liked can be reproduced exactly and a
    // test can assert on one.
    std::uint64_t seed = 1;
};

struct PatchVariationResult
{
    bool ok = false;
    std::string reason;  // set exactly when ok is false

    std::optional<xpmodel::Xp60Patch> patch;
    std::vector<VariationChange> changes;

    int eligibleParameters = 0;  // continuous parameters the scope allowed
    int changedParameters = 0;
    int clampedParameters = 0;   // moves cut short by a documented range edge

    [[nodiscard]] std::string summary() const;
};

// Makes a variation of a Patch by nudging its continuous parameters.
//
// ── What it will not touch, and why that is most of the design ──────────────
//
// **Nothing discrete.** A parameter is left alone when it carries an
// enumeration (Filter Type, LFO Waveform, a controller destination), when its
// documented range is 0..1 (every switch), or when it belongs to the Wave
// category. Those fields hold identities, not quantities: Filter Type 3 is not
// "a bit more" than 2, wave 118 is not "near" 119, and Controller 1
// Destination 2 naming a different target is not a small change. Moving one is
// not a variation of a sound, it is a different sound with the same name.
//
// **Nothing in a Tone that is switched off.** Changing what nobody hears is
// change without effect, and it would make two variations that sound identical
// compare as different.
//
// **Never the Patch name.** A variation is a new sound and deserves a new name,
// but choosing one is the musician's job, not this class's.
//
// ── What it does ────────────────────────────────────────────────────────────
//
// Each eligible parameter is either left alone or moved by up to
// `amountPercent` of its **own** documented range, in either direction, and the
// result is clamped into that range. Clamping is counted and reported rather
// than hidden: a Cutoff already at 127 cannot go up, and a variation that
// silently did nothing there while claiming to have varied it would be lying
// about its own reach.
//
// Deterministic from `seed`, so a variation can be reproduced exactly.
//
// This class makes no claim about how the result sounds. It says which
// parameters moved and by how much; whether the outcome is musical is for the
// musician to judge. `sounddna` is where perceptual claims would live, and it
// publishes none.
//
// Pure: no I/O, no Qt, no database, no global random state.
class PatchVariation
{
public:
    [[nodiscard]] static PatchVariationResult apply(const xpmodel::Xp60Patch& patch,
                                                    const PatchVariationOptions& options);

    // Whether a variation may move this parameter, and why not when it may not.
    // Exposed so a UI can show the reason beside a control it will not touch.
    [[nodiscard]] static bool isContinuous(const xpmodel::ParameterDescriptor& parameter) noexcept;
    [[nodiscard]] static std::string_view whyNotVaried(
        const xpmodel::ParameterDescriptor& parameter) noexcept;
};

} // namespace xp60studio::library
