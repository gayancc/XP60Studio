#pragma once

#include <array>
#include <optional>
#include <string_view>

namespace xp60studio::xpmodel {

// Documentation-derived: Roland XP-60/XP-80 Owner's Manual pp.199-203
// (PDF pages 201-205), with algorithm descriptions on pp.74-88.
// MIDI Implementation p.223 maps raw 0..39 to display types 1..40.
// These names do not imply a verified mapping of EFX Parameter 1..12 to
// each algorithm's controls or physical-unit conversions.
inline constexpr std::array<std::string_view, 40> kEfxTypeNames{
    "STEREO-EQ", "OVERDRIVE", "DISTORTION", "PHASER", "SPECTRUM",
    "ENHANCER", "AUTO-WAH", "ROTARY", "COMPRESSOR", "LIMITER",
    "HEXA-CHORUS", "TREMOLO-CHORUS", "SPACE-D", "STEREO-CHORUS", "STEREO-FLANGER",
    "STEP-FLANGER", "STEREO-DELAY", "MODULATION-DELAY", "TRIPLE-TAP-DELAY", "QUADRUPLE-TAP-DELAY",
    "TIME-CONTROL-DELAY", "2VOICE-PITCH-SHIFTER", "FBK-PITCH-SHIFTER", "REVERB", "GATE-REVERB",
    "OVERDRIVE -> CHORUS", "OVERDRIVE -> FLANGER", "OVERDRIVE -> DELAY",
    "DISTORTION -> CHORUS", "DISTORTION -> FLANGER", "DISTORTION -> DELAY",
    "ENHANCER -> CHORUS", "ENHANCER -> FLANGER", "ENHANCER -> DELAY",
    "CHORUS -> DELAY", "FLANGER -> DELAY", "CHORUS -> FLANGER",
    "CHORUS/DELAY", "FLANGER/DELAY", "CHORUS/FLANGER"
};

inline std::optional<std::string_view> efxTypeName(int raw)
{
    if (raw < 0 || raw >= static_cast<int>(kEfxTypeNames.size())) return {};
    return kEfxTypeNames[static_cast<std::size_t>(raw)];
}

} // namespace xp60studio::xpmodel
