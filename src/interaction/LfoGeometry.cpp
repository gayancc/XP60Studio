#include "interaction/LfoGeometry.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace xp60studio::interaction {

using xpmodel::ToneIndex;
using xpmodel::ToneParameter;
using xpmodel::Xp60Patch;

namespace {

// The most cycles a plot draws at Rate 127. A picture with more cycles than
// this is a grey band, not a waveform; the number is a drawing decision and is
// stated as one rather than dressed up as a frequency.
constexpr double kMaxCycles = 8.0;

double fractionOf(int raw, int rawMax)
{
    return rawMax <= 0 ? 0.0 : std::clamp(static_cast<double>(raw) / rawMax, 0.0, 1.0);
}

// A small deterministic hash, so Sample & Hold and Random draw the same picture
// every repaint for the same Patch. A plot that reshuffled itself would look
// like a fault.
double stepValue(std::uint32_t seed, int index)
{
    std::uint32_t x = seed * 2654435761u + static_cast<std::uint32_t>(index) * 40503u;
    x ^= x >> 15;
    x *= 2246822519u;
    x ^= x >> 13;
    return static_cast<double>(x % 10001u) / 10000.0;
}

} // namespace

std::string_view lfoWaveformName(LfoWaveform waveform) noexcept
{
    switch (waveform) {
    case LfoWaveform::Triangle:
        return "Triangle";
    case LfoWaveform::Sine:
        return "Sine";
    case LfoWaveform::Sawtooth:
        return "Sawtooth";
    case LfoWaveform::Square:
        return "Square";
    case LfoWaveform::Trapezoid:
        return "Trapezoid";
    case LfoWaveform::SampleAndHold:
        return "Sample & Hold";
    case LfoWaveform::Random:
        return "Random";
    case LfoWaveform::Chaos:
        return "Chaos";
    }
    return "Unknown";
}

std::string_view lfoWaveformLabel(LfoWaveform waveform) noexcept
{
    // Roland's own labels, from the Parameter Address Map's Tone footnote 4.
    switch (waveform) {
    case LfoWaveform::Triangle:
        return "TRI";
    case LfoWaveform::Sine:
        return "SIN";
    case LfoWaveform::Sawtooth:
        return "SAW";
    case LfoWaveform::Square:
        return "SQR";
    case LfoWaveform::Trapezoid:
        return "TRP";
    case LfoWaveform::SampleAndHold:
        return "S&H";
    case LfoWaveform::Random:
        return "RND";
    case LfoWaveform::Chaos:
        return "CHS";
    }
    return "";
}

bool lfoWaveformIsAperiodic(LfoWaveform waveform) noexcept
{
    // These three have no fixed shape. Drawing one as a repeating curve without
    // saying so would misrepresent what the instrument does.
    return waveform == LfoWaveform::SampleAndHold || waveform == LfoWaveform::Random
        || waveform == LfoWaveform::Chaos;
}

LfoGeometry LfoGeometry::of(const Xp60Patch& patch, ToneIndex tone, Which which)
{
    LfoGeometry geometry;
    const bool first = which == Which::Lfo1;
    const auto waveformParameter
        = first ? ToneParameter::Lfo1Waveform : ToneParameter::Lfo2Waveform;
    const auto rateParameter = first ? ToneParameter::Lfo1Rate : ToneParameter::Lfo2Rate;
    const auto delayParameter
        = first ? ToneParameter::Lfo1DelayTime : ToneParameter::Lfo2DelayTime;
    const auto fadeParameter = first ? ToneParameter::Lfo1FadeTime : ToneParameter::Lfo2FadeTime;
    const auto triggerParameter
        = first ? ToneParameter::Lfo1KeyTrigger : ToneParameter::Lfo2KeyTrigger;

    const int waveformRaw = std::clamp(patch.raw(tone, waveformParameter), 0, 7);
    geometry.m_waveform = static_cast<LfoWaveform>(waveformRaw);
    geometry.m_rateRaw = patch.raw(tone, rateParameter);
    geometry.m_delayRaw = patch.raw(tone, delayParameter);
    geometry.m_fadeRaw = patch.raw(tone, fadeParameter);
    geometry.m_keyTrigger = patch.raw(tone, triggerParameter) != 0;

    // Cycles across the plot, proportional to the stored Rate. Not Hz: Roland
    // publishes no mapping and this project has not measured one.
    geometry.m_cycles = 1.0 + fractionOf(geometry.m_rateRaw, 127) * (kMaxCycles - 1.0);
    // Delay and Fade take at most half the plot each, so a long delay still
    // leaves the waveform visible instead of pushing it off the edge.
    geometry.m_delayFraction = fractionOf(geometry.m_delayRaw, 127) * 0.5;
    geometry.m_fadeFraction = fractionOf(geometry.m_fadeRaw, 127) * 0.5;
    return geometry;
}

std::vector<LfoSample> LfoGeometry::curve(int sampleCount) const
{
    std::vector<LfoSample> out;
    const int count = std::max(2, sampleCount);
    out.reserve(static_cast<std::size_t>(count));

    const auto seed = static_cast<std::uint32_t>(m_rateRaw * 131 + static_cast<int>(m_waveform));
    const double stepsPerPlot = std::max(1.0, std::round(m_cycles));

    for (int i = 0; i < count; ++i) {
        const double x = static_cast<double>(i) / (count - 1);
        // Before the delay has elapsed the LFO is not running: the curve rests
        // on the centre line rather than pretending to start at note-on.
        const double active = x <= m_delayFraction
            ? 0.0
            : (m_fadeFraction <= 0.0
                   ? 1.0
                   : std::clamp((x - m_delayFraction) / m_fadeFraction, 0.0, 1.0));

        const double phase = m_delayFraction >= 1.0
            ? 0.0
            : (x - m_delayFraction) / (1.0 - m_delayFraction) * m_cycles;
        const double wrapped = phase - std::floor(phase);

        double value = 0.0;  // -1..1
        switch (m_waveform) {
        case LfoWaveform::Triangle:
            value = wrapped < 0.5 ? (4.0 * wrapped - 1.0) : (3.0 - 4.0 * wrapped);
            break;
        case LfoWaveform::Sine:
            value = std::sin(2.0 * std::numbers::pi * wrapped);
            break;
        case LfoWaveform::Sawtooth:
            value = 2.0 * wrapped - 1.0;
            break;
        case LfoWaveform::Square:
            value = wrapped < 0.5 ? 1.0 : -1.0;
            break;
        case LfoWaveform::Trapezoid:
            // A triangle with its peaks flattened: rise, hold, fall, hold.
            if (wrapped < 0.25) {
                value = wrapped / 0.25 * 2.0 - 1.0;
            } else if (wrapped < 0.5) {
                value = 1.0;
            } else if (wrapped < 0.75) {
                value = 1.0 - (wrapped - 0.5) / 0.25 * 2.0;
            } else {
                value = -1.0;
            }
            break;
        case LfoWaveform::SampleAndHold:
        case LfoWaveform::Random:
        case LfoWaveform::Chaos: {
            // No fixed shape. A representative one is drawn, deterministically
            // from the stored Rate so the picture is stable, and
            // `lfoWaveformIsAperiodic` lets the view say it is representative.
            const int step = static_cast<int>(std::floor(phase * stepsPerPlot / m_cycles
                                                         * stepsPerPlot));
            value = stepValue(seed, step) * 2.0 - 1.0;
            break;
        }
        }

        LfoSample sample;
        sample.x = x;
        sample.y = std::clamp(0.5 + value * active * 0.5, 0.0, 1.0);
        out.push_back(sample);
    }
    return out;
}

} // namespace xp60studio::interaction
