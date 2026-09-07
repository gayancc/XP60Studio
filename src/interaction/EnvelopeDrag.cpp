#include "interaction/EnvelopeDrag.h"

#include "xpmodel/Xp60PatchLayout.h"

#include <algorithm>
#include <cmath>

namespace xp60studio::interaction {

using xpmodel::ParameterDescriptor;
using xpmodel::ToneIndex;
using xpmodel::ToneParameter;
using xpmodel::Xp60Patch;
using xpmodel::Xp60PatchLayout;

namespace {

struct StageParameters
{
    ToneParameter time{};
    ToneParameter level{};
    bool hasLevel = false;
};

StageParameters stageParametersOf(EnvelopeKind kind, int stage)
{
    const auto index = static_cast<std::size_t>(stage - 1);
    static constexpr std::array<ToneParameter, 4> kPitchTimes{
        ToneParameter::PitchEnvelopeTime1, ToneParameter::PitchEnvelopeTime2,
        ToneParameter::PitchEnvelopeTime3, ToneParameter::PitchEnvelopeTime4};
    static constexpr std::array<ToneParameter, 4> kPitchLevels{
        ToneParameter::PitchEnvelopeLevel1, ToneParameter::PitchEnvelopeLevel2,
        ToneParameter::PitchEnvelopeLevel3, ToneParameter::PitchEnvelopeLevel4};
    static constexpr std::array<ToneParameter, 4> kFilterTimes{
        ToneParameter::FilterEnvelopeTime1, ToneParameter::FilterEnvelopeTime2,
        ToneParameter::FilterEnvelopeTime3, ToneParameter::FilterEnvelopeTime4};
    static constexpr std::array<ToneParameter, 4> kFilterLevels{
        ToneParameter::FilterEnvelopeLevel1, ToneParameter::FilterEnvelopeLevel2,
        ToneParameter::FilterEnvelopeLevel3, ToneParameter::FilterEnvelopeLevel4};
    static constexpr std::array<ToneParameter, 4> kLevelTimes{
        ToneParameter::LevelEnvelopeTime1, ToneParameter::LevelEnvelopeTime2,
        ToneParameter::LevelEnvelopeTime3, ToneParameter::LevelEnvelopeTime4};
    static constexpr std::array<ToneParameter, 3> kLevelLevels{
        ToneParameter::LevelEnvelopeLevel1, ToneParameter::LevelEnvelopeLevel2,
        ToneParameter::LevelEnvelopeLevel3};

    switch (kind) {
    case EnvelopeKind::Pitch:
        return {kPitchTimes[index], kPitchLevels[index], true};
    case EnvelopeKind::Filter:
        return {kFilterTimes[index], kFilterLevels[index], true};
    case EnvelopeKind::Amplifier:
        // Stage 4 of the TVA envelope falls to silence and has no Level of its
        // own, so its point moves horizontally only.
        return stage <= 3 ? StageParameters{kLevelTimes[index], kLevelLevels[index], true}
                          : StageParameters{kLevelTimes[index], ToneParameter{}, false};
    }
    return {};
}

const ParameterDescriptor& descriptorOf(ToneParameter parameter)
{
    return Xp60PatchLayout::patchToneTable().parameters()[static_cast<std::size_t>(parameter)];
}

} // namespace

bool EnvelopeDrag::begin(const Xp60Patch& patch, ToneIndex tone, EnvelopeKind kind, int stage,
                         double pointerX, double pointerY)
{
    if (stage < 1 || stage > 4) {
        return false;
    }
    const auto geometry = EnvelopeGeometry::of(patch, tone, kind);
    const auto spec = stageParametersOf(kind, stage);

    m_dragging = true;
    m_stage = stage;
    m_tone = tone;
    m_kind = kind;
    m_grabX = pointerX;
    m_grabY = pointerY;
    m_pending.clear();
    m_coalesced = 0;

    m_timeParameter = spec.time;
    m_levelParameter = spec.level;
    m_hasLevel = spec.hasLevel;

    const auto& timeDescriptor = descriptorOf(m_timeParameter);
    m_timeName = timeDescriptor.name;
    m_timeRawMin = timeDescriptor.rawMin;
    m_timeRawMax = timeDescriptor.rawMax;
    m_timeAtGrabRaw = patch.raw(tone, m_timeParameter);
    m_timeAtGrab = static_cast<double>(m_timeAtGrabRaw);
    m_timeShadow = m_timeAtGrab;

    if (m_hasLevel) {
        const auto& levelDescriptor = descriptorOf(m_levelParameter);
        m_levelName = levelDescriptor.name;
        m_levelRawMin = levelDescriptor.rawMin;
        m_levelRawMax = levelDescriptor.rawMax;
        m_levelAtGrabRaw = patch.raw(tone, m_levelParameter);
    } else {
        m_levelName = {};
        m_levelRawMin = 0;
        m_levelRawMax = 0;
        m_levelAtGrabRaw = 0;
    }
    m_levelAtGrab = static_cast<double>(m_levelAtGrabRaw);
    m_levelShadow = m_levelAtGrab;

    // Plot units per raw step, fixed here for the whole drag.
    //
    // Deliberately fixed: the horizontal axis is normalized by the *total* of
    // the four Time values, so widening one stage narrows the others. If the
    // mapping were recomputed each sample, the handle would accelerate away
    // from the pointer as the drag proceeded — the control would feel like it
    // was fighting back. Fixing the scale at the grab keeps one pixel of
    // pointer travel worth the same number of steps for the whole gesture.
    const double totalTime = std::max(1.0, static_cast<double>(geometry.totalTimeRaw()));
    m_xPerTimeStep = 1.0 / totalTime;

    const int levelSteps = m_levelRawMax - m_levelRawMin;
    if (m_hasLevel && levelSteps > 0) {
        // One step of level in plot units, from the geometry's own mapping so
        // the drag and the drawing cannot disagree.
        const double topY = geometry.levelToY(m_levelRawMax, stage);
        const double bottomY = geometry.levelToY(m_levelRawMin, stage);
        m_yPerLevelStep = (topY - bottomY) / levelSteps;
    } else {
        m_yPerLevelStep = 0.0;
    }
    return true;
}

int EnvelopeDrag::quantise(double shadow, int current, int rawMin, int rawMax) const
{
    // Hysteresis: the stored value only follows once the shadow has passed the
    // midpoint by a fraction of a step, so a hand resting on a boundary does
    // not make it flicker.
    const double band = std::clamp(m_settings.hysteresisSteps, 0.0, 0.49);
    const double difference = shadow - current;
    int next = current;
    if (difference > 0.5 + band) {
        next = static_cast<int>(std::floor(shadow + 0.5 - band));
    } else if (difference < -(0.5 + band)) {
        next = static_cast<int>(std::ceil(shadow - 0.5 + band));
    }
    return std::clamp(next, rawMin, rawMax);
}

void EnvelopeDrag::queue(ToneParameter parameter, std::string_view name, int raw)
{
    // Coalesce: one entry per parameter, holding the latest value. A drag
    // producing hundreds of samples a second must not produce hundreds of
    // messages.
    for (auto& edit : m_pending) {
        if (edit.parameter == parameter) {
            edit.raw = raw;
            return;
        }
    }
    m_pending.push_back(DragEdit{parameter, name, raw});
}

DragFrame EnvelopeDrag::move(const Xp60Patch& patch, double pointerX, double pointerY)
{
    DragFrame frame;
    if (!m_dragging) {
        frame.points = EnvelopeGeometry::of(patch, m_tone, m_kind).points();
        return frame;
    }
    ++m_coalesced;

    // Exact accumulation: the shadow is computed from the grab origin every
    // time, never integrated sample by sample, so a thousand small moves land
    // exactly where one large move would. Drift is not small here, it is
    // impossible.
    if (m_settings.horizontal && m_xPerTimeStep > 0.0) {
        const double dx = pointerX - m_grabX;
        if (std::abs(dx) >= m_settings.motionEpsilon || dx == 0.0) {
            m_timeShadow = std::clamp(m_timeAtGrab + dx / m_xPerTimeStep,
                                      static_cast<double>(m_timeRawMin),
                                      static_cast<double>(m_timeRawMax));
        }
    }
    if (m_settings.vertical && m_hasLevel && m_yPerLevelStep != 0.0) {
        const double dy = pointerY - m_grabY;
        if (std::abs(dy) >= m_settings.motionEpsilon || dy == 0.0) {
            m_levelShadow = std::clamp(m_levelAtGrab + dy / m_yPerLevelStep,
                                       static_cast<double>(m_levelRawMin),
                                       static_cast<double>(m_levelRawMax));
        }
    }

    const int currentTime = patch.raw(m_tone, m_timeParameter);
    const int nextTime = quantise(m_timeShadow, currentTime, m_timeRawMin, m_timeRawMax);
    if (nextTime != currentTime) {
        queue(m_timeParameter, m_timeName, nextTime);
        frame.valueChanged = true;
    }
    int nextLevel = m_levelAtGrabRaw;
    if (m_hasLevel) {
        const int currentLevel = patch.raw(m_tone, m_levelParameter);
        nextLevel = quantise(m_levelShadow, currentLevel, m_levelRawMin, m_levelRawMax);
        if (nextLevel != currentLevel) {
            queue(m_levelParameter, m_levelName, nextLevel);
            frame.valueChanged = true;
        }
    }

    // Say what the handle is pinned against, if anything. A control that stops
    // moving without saying why reads as broken.
    if (m_timeShadow >= m_timeRawMax && m_settings.horizontal) {
        frame.limit = std::string(m_timeName) + " is at its maximum.";
    } else if (m_timeShadow <= m_timeRawMin && m_settings.horizontal) {
        frame.limit = std::string(m_timeName) + " is at its minimum.";
    } else if (m_hasLevel && m_settings.vertical && m_levelShadow >= m_levelRawMax) {
        frame.limit = std::string(m_levelName) + " is at its maximum.";
    } else if (m_hasLevel && m_settings.vertical && m_levelShadow <= m_levelRawMin) {
        frame.limit = std::string(m_levelName) + " is at its minimum.";
    }

    // Draw from the shadow, not from the quantised value. This is the whole
    // trick: the handle sits exactly under the pointer at display resolution
    // while the parameter beneath it moves in its own steps.
    const auto geometry = EnvelopeGeometry::of(patch, m_tone, m_kind);
    frame.points = geometry.points();
    const double shadowX
        = m_grabX + (m_timeShadow - m_timeAtGrab) * m_xPerTimeStep * (m_settings.horizontal ? 1.0 : 0.0);
    const double shadowY = m_hasLevel && m_settings.vertical
        ? m_grabY + (m_levelShadow - m_levelAtGrab) * m_yPerLevelStep
        : m_grabY;
    frame.handleX = shadowX;
    frame.handleY = shadowY;
    for (auto& point : frame.points) {
        if (point.stage == m_stage) {
            point.x = shadowX;
            point.y = shadowY;
            point.timeRaw = nextTime;
            point.levelRaw = m_hasLevel ? nextLevel : point.levelRaw;
        }
    }
    return frame;
}

void EnvelopeDrag::end()
{
    m_dragging = false;
}

std::vector<DragEdit> EnvelopeDrag::cancel()
{
    std::vector<DragEdit> back;
    if (m_dragging) {
        back.push_back(DragEdit{m_timeParameter, m_timeName, m_timeAtGrabRaw});
        if (m_hasLevel) {
            back.push_back(DragEdit{m_levelParameter, m_levelName, m_levelAtGrabRaw});
        }
    }
    m_dragging = false;
    m_pending.clear();
    m_coalesced = 0;
    return back;
}

std::vector<DragEdit> EnvelopeDrag::takeEdits()
{
    auto edits = std::move(m_pending);
    m_pending.clear();
    m_coalesced = 0;
    return edits;
}

} // namespace xp60studio::interaction
