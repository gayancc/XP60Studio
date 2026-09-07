#pragma once

#include "xpmodel/Xp60Patch.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace xp60studio::library {

// A part of a Patch that can be copied into another Patch.
//
// The groupings are Roland's own: every one is exactly the set of parameters
// the Parameter Address Map files under one category, so "copy the filter"
// means the five TVF parameters the instrument itself calls TVF, not a
// selection somebody thought looked right.
enum class PatchComponent {
    // Tone-scoped -----------------------------------------------------------
    WholeTone,          // every parameter of one Tone, including its switch
    Wave,               // wave group/number/gain and FXM
    Pitch,
    PitchEnvelope,
    Filter,             // TVF
    FilterEnvelope,     // TVF Envelope
    Amplifier,          // TVA
    AmplifierEnvelope,  // TVA Envelope
    Lfo1,
    Lfo2,
    Controllers,
    ControlSwitches,
    KeyVelocityRange,   // keyboard and velocity ranges, cross fade
    ToneDelay,
    TonePan,
    ToneEffectSends,    // this Tone's EFX / Chorus / Reverb / output routing

    // Patch-scoped ----------------------------------------------------------
    PatchEffects,       // EFX, Chorus and Reverb settings in Patch Common
    PatchCommonSettings,// level, pan, bend range, portamento, key assign...
    Structure,          // Structure Type 1&2 / 3&4 and the boosters
};

[[nodiscard]] std::string_view patchComponentName(PatchComponent component) noexcept;
// True for the components that need a source and destination Tone.
[[nodiscard]] bool patchComponentIsToneScoped(PatchComponent component) noexcept;
[[nodiscard]] std::vector<PatchComponent> allPatchComponents();

struct PatchCopyRequest
{
    PatchComponent component = PatchComponent::WholeTone;
    // Required for a Tone-scoped component, ignored otherwise. Copying Tone 1
    // onto Tone 3 is a normal thing to want, so they are separate.
    std::optional<xpmodel::ToneIndex> fromTone;
    std::optional<xpmodel::ToneIndex> toTone;
};

struct PatchCopyResult
{
    bool ok = false;
    std::string reason;  // set exactly when ok is false

    // The destination with the copy applied. Present only when ok.
    std::optional<xpmodel::Xp60Patch> patch;

    int parametersCopied = 0;   // parameters the component covers
    int parametersChanged = 0;  // of those, how many actually differed

    // What was *not* copied but might matter, in plain words.
    //
    // This is the part that earns the class its place. Copying a Tone into
    // another Patch is easy; knowing that the Structure pairing it belonged to
    // stayed behind, or that its wave needs a board the destination's other
    // Tones do not, is the difference between a useful tool and a trap.
    std::vector<std::string> notes;

    [[nodiscard]] std::string summary() const;
};

// Copies one component of a Patch into another Patch.
//
// ── What it will not do ─────────────────────────────────────────────────────
//
// **It never copies the Patch name.** A Patch that took on the name of the one
// a filter came from would be a Patch nobody could find again.
//
// **It never interpolates.** Every value is carried across exactly as it was
// stored. Roland's parameters include wave numbers, filter types and controller
// destinations, which are identities rather than quantities — the number 3 in a
// Filter Type field is not "between" 2 and 4 in any sense the instrument
// respects — so this class copies and does not blend. `ROADMAP.md` Phase 10
// states the same rule as "do not interpolate discrete IDs blindly"; the way
// this class obeys it is by not interpolating at all.
//
// **It applies whole or not at all.** The copy is built on a working copy of
// the destination and returned only if every parameter was accepted. A
// component half applied would leave a Patch nobody designed.
//
// ── What it says ────────────────────────────────────────────────────────────
//
// Every result carries notes about what stayed behind: a Structure pairing that
// belongs to Patch Common rather than to the Tone, effect sends that route into
// Patch-level settings the copy did not bring, a wave that needs an expansion
// board. Nothing is refused on those grounds — the user may know exactly what
// they are doing — but nothing is silent either.
//
// Pure: no I/O, no Qt, no database.
class PatchComponentCopy
{
public:
    [[nodiscard]] static PatchCopyResult apply(const xpmodel::Xp60Patch& destination,
                                               const xpmodel::Xp60Patch& source,
                                               const PatchCopyRequest& request);

    // The Patch Common / Tone parameter categories a component covers. Exposed
    // so a UI can show exactly what a copy will touch before it happens.
    [[nodiscard]] static std::vector<std::string_view> categoriesOf(PatchComponent component);
};

} // namespace xp60studio::library
