#pragma once

#include "library/ExpansionProfile.h"
#include "xpmodel/Xp60Patch.h"

#include <array>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace xp60studio::library {

// Whether a Patch will sound the way its author intended on *this* XP-60.
//
// The question a musician actually asks of an imported bank is "which of these
// will play properly on my instrument?", and the answer turns entirely on Wave
// Expansion Boards: a Tone pointing at a wave from a board that is not installed
// has nothing to sound.
//
// ── The three-valued answer, and why it is not two-valued ────────────────────
//
// XP60Studio cannot tell which board a Wave Group ID denotes — that is not
// documented and this project's evidence declines to settle it (see
// `ExpansionProfile`). So "missing" is only sayable when the musician has told
// us enough: they declared their boards *and* which groups those boards answer
// to. Short of that the honest verdict is `Unknown`, and the UI says what is
// needed to turn it into an answer.
//
// Reporting `Missing` on a profile nobody filled in would be worse than useless:
// it would train musicians to ignore the warning.
enum class ToneCompatibility {
    // The Tone's switch is off, so what it points at cannot be heard. Reported
    // rather than skipped: turning the Tone on later is a normal edit, and a
    // musician deserves to know it would then need a board.
    Disabled,
    // An internal wave. Always playable.
    Internal,
    // An expansion wave from a group one of the declared boards provides.
    ExpansionAvailable,
    // An expansion wave from a group no declared board provides, on a profile
    // complete enough for that to mean something.
    ExpansionMissing,
    // An expansion wave whose availability cannot be decided: no board has been
    // declared at all, or a declared board's wave group is not yet known.
    ExpansionUnknown,
};

[[nodiscard]] std::string_view toneCompatibilityName(ToneCompatibility value) noexcept;
[[nodiscard]] std::string_view toneCompatibilityLabel(ToneCompatibility value) noexcept;

struct ToneCompatibilityReport
{
    int toneNumber = 0; // 1..4
    ToneCompatibility status = ToneCompatibility::Internal;
    bool enabled = true;
    // Set for every expansion reference, whatever the verdict.
    std::optional<int> waveGroupId;
    std::optional<int> waveNumberRaw;
    // The slot that provides it, when one does.
    std::optional<int> providedBySlot;
};

// What a whole Patch needs, and whether this instrument has it.
struct PatchCompatibilityReport
{
    std::array<ToneCompatibilityReport, 4> tones{};

    // Every expansion wave group the Patch refers to, enabled Tones and
    // disabled ones alike, in ascending order.
    std::set<int> requiredGroups;
    // The subset of those that nothing declared provides.
    std::set<int> missingGroups;

    // No enabled Tone needs a board that is missing. A Patch with a *disabled*
    // Tone needing a missing board is still playable as it stands, which is why
    // the two are counted apart.
    [[nodiscard]] bool playable() const noexcept { return missingEnabledTones() == 0; }
    [[nodiscard]] bool usesExpansion() const noexcept { return !requiredGroups.empty(); }
    [[nodiscard]] int missingEnabledTones() const noexcept;
    [[nodiscard]] int unknownTones() const noexcept;
    // True when any verdict is Unknown, so the caller must not present the
    // result as a clean pass.
    [[nodiscard]] bool undecided() const noexcept { return unknownTones() > 0; }

    // One line for a row or a tooltip, e.g. "Needs expansion groups 5, 14 —
    // group 14 is not installed".
    [[nodiscard]] std::string summary() const;
};

// Analyses `patch` against `profile`. Pure: no I/O, no clock, no Qt.
[[nodiscard]] PatchCompatibilityReport analysePatch(const xpmodel::Xp60Patch& patch,
                                                    const ExpansionProfile& profile);

// The expansion groups one Patch needs, in ascending order. Disabled Tones
// count: turning one on is an ordinary edit, and a musician deserves to know it
// would then want a board.
[[nodiscard]] std::set<int> requiredExpansionGroups(const xpmodel::Xp60Patch& patch);

// The expansion groups a set of Patches needs, in ascending order. Used to
// answer "what boards does this bank want?" without analysing each Patch twice.
[[nodiscard]] std::set<int> requiredExpansionGroups(const std::vector<xpmodel::Xp60Patch>& patches);

} // namespace xp60studio::library
