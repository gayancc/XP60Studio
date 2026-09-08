#pragma once

#include "xpmodel/Xp60Patch.h"

#include <string_view>
#include <vector>

namespace xp60studio::interaction {

// Roland's eight LFO waveforms, as the Parameter Address Map enumerates them.
enum class LfoWaveform { Triangle, Sine, Sawtooth, Square, Trapezoid, SampleAndHold, Random, Chaos };

[[nodiscard]] std::string_view lfoWaveformName(LfoWaveform waveform) noexcept;
// Roland's own four-character label: "TRI", "S&H", ...
[[nodiscard]] std::string_view lfoWaveformLabel(LfoWaveform waveform) noexcept;
// True for the two waveforms whose shape is not a fixed curve. Drawing them as
// one is a lie a plot should not tell; a view shows a representative shape and
// says it is representative.
[[nodiscard]] bool lfoWaveformIsAperiodic(LfoWaveform waveform) noexcept;

// One sampled point of the drawn LFO curve.
struct LfoSample
{
    double x = 0.0;  // 0..1 across the plot
    double y = 0.0;  // 0..1, with 0.5 the resting line for a bipolar shape
};

// An LFO as a curve that can be drawn.
//
// ── What the axes are, and are not ──────────────────────────────────────────
//
// Roland documents LFO Rate as 0..127 and never says what any of those values
// is in Hz. As with envelope Time (`EnvelopeGeometry`), this project has not
// measured it and will not draw an axis in units it cannot justify.
//
// So the horizontal axis is **one plot width**, and the number of cycles drawn
// across it is proportional to the Rate value. A faster Rate draws more cycles.
// That is a true statement about the stored value and makes the control legible
// without claiming a frequency.
//
// Delay Time and Fade Time are drawn the same way: as fractions of the plot
// proportional to their stored values, shading the region where the LFO has not
// started and the region over which it is arriving.
//
// ── The waveform is never interpolated ──────────────────────────────────────
//
// The waveform is one of eight named shapes, an identity rather than a
// quantity. It is drawn as itself and switched, never blended — the same rule
// `PatchVariation` and `PatchComponentCopy` follow for every discrete field.
// Sample & Hold and Random have no fixed shape at all, so `isAperiodic()` says
// so and a view can label what it is showing as representative.
class LfoGeometry
{
public:
    // Which of a Tone's two LFOs.
    enum class Which { Lfo1, Lfo2 };

    [[nodiscard]] static LfoGeometry of(const xpmodel::Xp60Patch& patch, xpmodel::ToneIndex tone,
                                        Which which);

    [[nodiscard]] LfoWaveform waveform() const noexcept { return m_waveform; }
    [[nodiscard]] int rateRaw() const noexcept { return m_rateRaw; }
    [[nodiscard]] int delayRaw() const noexcept { return m_delayRaw; }
    [[nodiscard]] int fadeRaw() const noexcept { return m_fadeRaw; }
    [[nodiscard]] bool keyTrigger() const noexcept { return m_keyTrigger; }

    // How many cycles the plot shows, proportional to Rate. Always at least
    // one, so a Rate of zero still draws a shape rather than a flat line the
    // user cannot grab.
    [[nodiscard]] double cycles() const noexcept { return m_cycles; }
    // Fractions of the plot width. `delayFraction` is the part before the LFO
    // starts; `fadeFraction` the part over which it reaches full depth.
    [[nodiscard]] double delayFraction() const noexcept { return m_delayFraction; }
    [[nodiscard]] double fadeFraction() const noexcept { return m_fadeFraction; }

    // The curve, sampled evenly across the plot. Deterministic for every
    // waveform including the aperiodic ones, which are seeded from the stored
    // Rate so the same Patch always draws the same picture — a plot that
    // reshuffled itself on every repaint would look like a fault.
    [[nodiscard]] std::vector<LfoSample> curve(int sampleCount = 256) const;

private:
    LfoWaveform m_waveform = LfoWaveform::Triangle;
    int m_rateRaw = 0;
    int m_delayRaw = 0;
    int m_fadeRaw = 0;
    bool m_keyTrigger = false;
    double m_cycles = 1.0;
    double m_delayFraction = 0.0;
    double m_fadeFraction = 0.0;
};

} // namespace xp60studio::interaction
