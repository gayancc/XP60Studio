#include "interaction/ParameterDrag.h"

#include <algorithm>
#include <cmath>

namespace xp60studio::interaction {

bool ParameterDrag::begin(const xpmodel::ParameterDescriptor& parameter, int currentRaw,
                          double pointer)
{
    if (parameter.rawMax <= parameter.rawMin) {
        return false;  // nothing to drag
    }
    m_dragging = true;
    m_name = parameter.name;
    m_rawMin = parameter.rawMin;
    m_rawMax = parameter.rawMax;
    m_rawAtGrab = std::clamp(currentRaw, m_rawMin, m_rawMax);
    m_current = m_rawAtGrab;
    m_valueAtGrab = static_cast<double>(m_rawAtGrab);
    m_shadow = m_valueAtGrab;
    m_grabPointer = pointer;
    m_pending = m_rawAtGrab;
    m_hasPending = false;
    m_coalesced = 0;

    // Pointer units per step, fixed at the grab. The range comes from the
    // instrument's own descriptor, so a 0..3 parameter gets four coarse steps
    // across the same travel a 0..127 one spreads over 128 fine ones — which is
    // what makes both feel like the same control.
    const double steps = static_cast<double>(m_rawMax - m_rawMin);
    const double travel = std::max(1e-9, m_settings.travelForFullRange);
    m_unitsPerStep = travel / steps;
    return true;
}

ParameterDragFrame ParameterDrag::move(double pointer)
{
    ParameterDragFrame frame;
    if (!m_dragging) {
        frame.raw = m_current;
        return frame;
    }
    ++m_coalesced;

    // Exact accumulation from the grab origin: never integrated per sample, so
    // a thousand small moves land exactly where one large move would.
    const double sensitivity = m_settings.sensitivity <= 0.0 ? 1.0 : m_settings.sensitivity;
    const double delta = (pointer - m_grabPointer) * sensitivity / m_unitsPerStep;
    const double wanted = m_valueAtGrab + delta;
    m_shadow = std::clamp(wanted, static_cast<double>(m_rawMin), static_cast<double>(m_rawMax));

    // Hysteresis at the step boundary.
    const double band = std::clamp(m_settings.hysteresisSteps, 0.0, 0.49);
    const double difference = m_shadow - m_current;
    int next = m_current;
    if (difference > 0.5 + band) {
        next = static_cast<int>(std::floor(m_shadow + 0.5 - band));
    } else if (difference < -(0.5 + band)) {
        next = static_cast<int>(std::ceil(m_shadow - 0.5 + band));
    }
    next = std::clamp(next, m_rawMin, m_rawMax);
    if (next != m_current) {
        m_current = next;
        // Coalesce: one value, the latest, whatever the sample rate.
        m_pending = next;
        m_hasPending = true;
        frame.valueChanged = true;
    }

    // Drawn from the shadow, so the control moves at display resolution even
    // when the parameter has four legal values.
    const double span = static_cast<double>(m_rawMax - m_rawMin);
    frame.position = span <= 0.0 ? 0.0 : (m_shadow - m_rawMin) / span;
    frame.raw = m_current;

    if (wanted >= m_rawMax) {
        frame.limit = std::string(m_name) + " is at its maximum.";
    } else if (wanted <= m_rawMin) {
        frame.limit = std::string(m_name) + " is at its minimum.";
    }
    return frame;
}

int ParameterDrag::cancel()
{
    m_dragging = false;
    m_current = m_rawAtGrab;
    m_shadow = m_valueAtGrab;
    m_pending = m_rawAtGrab;
    m_hasPending = false;
    m_coalesced = 0;
    return m_rawAtGrab;
}

int ParameterDrag::takePendingValue()
{
    m_hasPending = false;
    m_coalesced = 0;
    return m_pending;
}

} // namespace xp60studio::interaction
