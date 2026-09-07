#pragma once

#include "xpmodel/ParameterDescriptor.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace xp60studio::interaction {

// What to draw after one pointer sample of a single-parameter drag.
struct ParameterDragFrame
{
    // 0..1 along the control, from the **continuous** shadow rather than from
    // the quantised value. A knob drawn from this rotates smoothly across a
    // 16-step parameter instead of clicking between sixteen positions.
    double position = 0.0;
    int raw = 0;
    bool valueChanged = false;
    // What the control is pinned against, if anything. Empty when free.
    std::string limit;
};

struct ParameterDragSettings
{
    // Pointer travel, in the caller's own units, that spans the parameter's
    // whole documented range. A view passes its own pixel height here, so a
    // tall control is finer than a short one without either knowing about the
    // other.
    double travelForFullRange = 200.0;
    // See `EnvelopeDrag`: a stored value only follows once the shadow has
    // crossed the next step's midpoint by this fraction of a step, so a resting
    // hand does not make the value flicker.
    double hysteresisSteps = 0.25;
    // Multiplies the pointer delta. A view binds this to a modifier key for a
    // fine-drag mode; the mechanism is the same either way, so precision costs
    // nothing extra.
    double sensitivity = 1.0;
};

// A smooth drag over one XP-60 parameter.
//
// The same four mechanisms `EnvelopeDrag` uses, factored out for every ordinary
// knob, slider and field in the editor — where the same stickiness shows up and
// is, if anything, more obvious. A Wave Gain has four legal values; drawn from
// the quantised value it snaps between four positions and feels broken. Drawn
// from the shadow it rotates smoothly and lands on the value the hand chose.
//
//  1. A continuous shadow the view draws from; the integer is derived from it.
//  2. Hysteresis at the step boundary, so a resting hand produces no changes.
//  3. Exact accumulation from the grab origin, so no motion is lost and no
//     drift accumulates however many samples arrive.
//  4. One value per drain, so a fast drag cannot flood a 31250-baud link.
//
// It does not smooth, filter or predict the pointer, for the reason given in
// `design/ENVELOPE_INTERACTION.md`: that makes a control feel different rather
// than more accurate.
//
// Deliberately knows nothing about which parameter it is driving. It takes a
// descriptor — the same one that drives decoding, display and validation — so
// the range and the step count are the instrument's, never a caller's guess.
//
// Pure: no Qt, no timers, no I/O.
class ParameterDrag
{
public:
    ParameterDrag() = default;
    explicit ParameterDrag(ParameterDragSettings settings) : m_settings(settings) {}

    [[nodiscard]] const ParameterDragSettings& settings() const noexcept { return m_settings; }
    void setSettings(ParameterDragSettings settings) { m_settings = settings; }

    // Grabs `parameter` at its current value. `pointer` is a single coordinate
    // along whichever axis the control uses; the sign convention is the
    // caller's, so a vertical slider passes a negated y and gets "up increases".
    bool begin(const xpmodel::ParameterDescriptor& parameter, int currentRaw, double pointer);
    [[nodiscard]] bool isDragging() const noexcept { return m_dragging; }

    [[nodiscard]] ParameterDragFrame move(double pointer);
    void end() { m_dragging = false; }
    // The value the grab started from, for an escape-key undo.
    [[nodiscard]] int cancel();

    // The latest value since the last drain, if it changed at all.
    [[nodiscard]] bool hasPendingValue() const noexcept { return m_hasPending; }
    [[nodiscard]] int takePendingValue();
    [[nodiscard]] std::uint64_t coalescedSamples() const noexcept { return m_coalesced; }

    [[nodiscard]] int rawAtGrab() const noexcept { return m_rawAtGrab; }
    [[nodiscard]] std::string_view parameterName() const noexcept { return m_name; }

private:
    ParameterDragSettings m_settings;
    bool m_dragging = false;
    double m_grabPointer = 0.0;
    double m_shadow = 0.0;
    double m_valueAtGrab = 0.0;
    int m_rawAtGrab = 0;
    int m_current = 0;
    int m_rawMin = 0;
    int m_rawMax = 127;
    double m_unitsPerStep = 1.0;
    std::string_view m_name;

    int m_pending = 0;
    bool m_hasPending = false;
    std::uint64_t m_coalesced = 0;
};

} // namespace xp60studio::interaction
