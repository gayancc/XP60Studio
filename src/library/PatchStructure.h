#pragma once

#include "xpmodel/Xp60Patch.h"

#include <array>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace xp60studio::library {

// What a Patch is built out of, read from its own documented parameters.
//
// Every field here is a fact the Parameter Address Map already states — how
// many Tones are switched on, which Structure pairs them, which waves they
// play, whether a Tone is confined to part of the keyboard or part of the
// velocity range. Nothing is inferred from a Patch name, and nothing claims to
// know how the Patch sounds. That distinction is the whole point: this is
// searchable structure, not a guess at timbre. `sounddna` is where perceptual
// claims live, and it publishes none until the evidence gates pass.
//
// Pure: no I/O, no Qt, no database. Derived metadata, never written back.
struct PatchStructure
{
    struct Tone
    {
        int number = 0;                 // 1..4
        bool enabled = false;
        int waveGroupTypeRaw = 0;       // 0 INT, 1 <PCM>, 2 EXP
        int waveGroupId = 0;
        int waveNumberDisplay = 0;      // as Roland prints it, from 1
        int filterTypeRaw = 0;
        int keyLow = 0;                 // MIDI note
        int keyHigh = 127;
        int velocityLow = 1;
        int velocityHigh = 127;

        // Narrower than the whole keyboard / the whole velocity range. These
        // are what make a Patch a split or a velocity stack, and they are the
        // two structural questions a musician most often searches on.
        [[nodiscard]] bool isKeyLimited() const noexcept { return keyLow > 0 || keyHigh < 127; }
        [[nodiscard]] bool isVelocityLimited() const noexcept
        {
            return velocityLow > 1 || velocityHigh < 127;
        }
        [[nodiscard]] bool usesExpansionWave() const noexcept { return waveGroupTypeRaw == 2; }
    };

    std::array<Tone, xpmodel::ToneIndex::kCount> tones{};
    int enabledToneCount = 0;
    int structureType12Raw = 0;
    int structureType34Raw = 0;
    bool booster12 = false;
    bool booster34 = false;
    int efxTypeRaw = 0;
    int reverbTypeRaw = 0;
    int chorusLevel = 0;
    int reverbLevel = 0;
    int keyAssignModeRaw = 0;           // 0 poly, 1 mono (per the table)
    bool portamento = false;
    // Wave Expansion groups any enabled Tone refers to, ascending. Empty means
    // the Patch plays entirely from internal waves.
    std::set<int> expansionGroups;

    [[nodiscard]] static PatchStructure of(const xpmodel::Xp60Patch& patch);

    // Only enabled Tones count towards these: a split written into a Tone that
    // is switched off is not a split anybody hears.
    [[nodiscard]] bool hasKeyboardSplit() const noexcept;
    [[nodiscard]] bool hasVelocitySwitching() const noexcept;
    [[nodiscard]] bool usesExpansion() const noexcept { return !expansionGroups.empty(); }
    [[nodiscard]] bool isLayered() const noexcept;

    [[nodiscard]] std::string summary() const;
};

// A search over structure rather than over names.
//
// "Four-Tone velocity stacks that need no expansion board" is a question the
// name field cannot answer and this can. Every field is a narrowing: an unset
// one constrains nothing, and set fields combine with AND — the same rule
// `LibraryQuery` follows, so the two compose without surprises.
//
// Sets mean "any of": `filterTypes = {1, 2}` matches a Patch with at least one
// enabled Tone using either. Requiring *every* Tone to match would be a
// different and much less useful question, so it is not what the sets mean, and
// `allTonesMatchWave` is the one place the stricter reading is offered.
struct StructuralQuery
{
    std::optional<int> minimumEnabledTones;
    std::optional<int> maximumEnabledTones;
    std::optional<bool> keyboardSplit;
    std::optional<bool> velocitySwitching;
    std::optional<bool> usesExpansion;
    std::optional<bool> portamento;
    // At least one enabled Tone uses one of these filter types.
    std::set<int> filterTypes;
    // The Patch's EFX / Reverb type is one of these.
    std::set<int> efxTypes;
    std::set<int> reverbTypes;
    // Structure Type 1&2 or 3&4 is one of these.
    std::set<int> structureTypes;
    // At least one enabled Tone refers to one of these expansion groups.
    std::set<int> expansionGroups;
    // At least one enabled Tone plays this exact wave. Group type and id must
    // match too, because wave 12 of one board is not wave 12 of another.
    struct WaveMatch
    {
        int groupTypeRaw = 0;
        int groupId = 0;
        int numberDisplay = 0;
    };
    std::optional<WaveMatch> wave;
    // Require every enabled Tone to play `wave`, not merely one of them.
    bool allTonesMatchWave = false;

    [[nodiscard]] bool isEmpty() const noexcept;
    [[nodiscard]] bool matches(const PatchStructure& structure) const;
    // Convenience: builds the structure and matches it.
    [[nodiscard]] bool matches(const xpmodel::Xp60Patch& patch) const;
    // What this query asks for, in words, for a UI that must show the user what
    // it just searched. An empty query says so rather than returning "".
    [[nodiscard]] std::string describe() const;
};

} // namespace xp60studio::library
