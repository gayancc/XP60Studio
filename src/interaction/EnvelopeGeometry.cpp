#include "interaction/EnvelopeGeometry.h"

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

struct EnvelopeParameters
{
    std::array<ToneParameter, 4> times{};
    std::array<ToneParameter, 4> levels{};
    int stageCount = 4;
    std::string_view levelIdPrefix;
};

EnvelopeParameters parametersOf(EnvelopeKind kind)
{
    switch (kind) {
    case EnvelopeKind::Pitch:
        return {{ToneParameter::PitchEnvelopeTime1, ToneParameter::PitchEnvelopeTime2,
                 ToneParameter::PitchEnvelopeTime3, ToneParameter::PitchEnvelopeTime4},
                {ToneParameter::PitchEnvelopeLevel1, ToneParameter::PitchEnvelopeLevel2,
                 ToneParameter::PitchEnvelopeLevel3, ToneParameter::PitchEnvelopeLevel4},
                4,
                "tone.pitch_envelope_level_1"};
    case EnvelopeKind::Filter:
        return {{ToneParameter::FilterEnvelopeTime1, ToneParameter::FilterEnvelopeTime2,
                 ToneParameter::FilterEnvelopeTime3, ToneParameter::FilterEnvelopeTime4},
                {ToneParameter::FilterEnvelopeLevel1, ToneParameter::FilterEnvelopeLevel2,
                 ToneParameter::FilterEnvelopeLevel3, ToneParameter::FilterEnvelopeLevel4},
                4,
                "tone.filter_envelope_level_1"};
    case EnvelopeKind::Amplifier:
        // Roland's TVA envelope has four times but only three levels: the
        // fourth stage always falls to silence, which is why L4 does not exist.
        return {{ToneParameter::LevelEnvelopeTime1, ToneParameter::LevelEnvelopeTime2,
                 ToneParameter::LevelEnvelopeTime3, ToneParameter::LevelEnvelopeTime4},
                {ToneParameter::LevelEnvelopeLevel1, ToneParameter::LevelEnvelopeLevel2,
                 ToneParameter::LevelEnvelopeLevel3, ToneParameter::LevelEnvelopeLevel3},
                3,
                "tone.level_envelope_level_1"};
    }
    return {};
}

const ParameterDescriptor* descriptorFor(std::string_view id)
{
    return Xp60PatchLayout::patchToneTable().find(id);
}

} // namespace

std::string_view envelopeKindName(EnvelopeKind kind) noexcept
{
    switch (kind) {
    case EnvelopeKind::Pitch:
        return "Pitch Envelope";
    case EnvelopeKind::Filter:
        return "Filter Envelope";
    case EnvelopeKind::Amplifier:
        return "Amplifier Envelope";
    }
    return "Envelope";
}

EnvelopeGeometry EnvelopeGeometry::of(const Xp60Patch& patch, ToneIndex tone, EnvelopeKind kind)
{
    EnvelopeGeometry geometry;
    geometry.m_tone = tone;
    geometry.m_kind = kind;

    const auto spec = parametersOf(kind);
    geometry.m_stageCount = spec.stageCount;

    // The level range comes from the instrument's own table, so a bipolar
    // envelope draws zero in the middle because Roland says its range is
    // negative to positive — not because this code assumed it.
    if (const auto* level = descriptorFor(spec.levelIdPrefix)) {
        geometry.m_levelRawMin = level->rawMin;
        geometry.m_levelRawMax = level->rawMax;
        geometry.m_levelScale = level->displayScale;
        geometry.m_levelOffset = level->displayOffset;
        geometry.m_levelDisplayMin = std::min(level->toDisplay(level->rawMin),
                                              level->toDisplay(level->rawMax));
        geometry.m_levelDisplayMax = std::max(level->toDisplay(level->rawMin),
                                              level->toDisplay(level->rawMax));
    }
    geometry.m_bipolar = geometry.m_levelDisplayMin < 0;
    const double displaySpan
        = static_cast<double>(geometry.m_levelDisplayMax - geometry.m_levelDisplayMin);
    geometry.m_zeroLine = displaySpan <= 0.0
        ? 0.0
        : static_cast<double>(-geometry.m_levelDisplayMin) / displaySpan;

    std::array<int, 4> times{};
    std::array<int, 4> levels{};
    for (int i = 0; i < 4; ++i) {
        times[static_cast<std::size_t>(i)] = patch.raw(tone, spec.times[static_cast<std::size_t>(i)]);
        levels[static_cast<std::size_t>(i)]
            = patch.raw(tone, spec.levels[static_cast<std::size_t>(i)]);
    }
    geometry.m_totalTimeRaw = times[0] + times[1] + times[2] + times[3];

    // The origin. An envelope starts from wherever the previous stage left the
    // sound, which for a note-on is the zero line.
    EnvelopePoint origin;
    origin.x = 0.0;
    origin.y = geometry.m_bipolar ? geometry.m_zeroLine : 0.0;
    origin.stage = 0;
    origin.draggable = false;
    geometry.m_points.push_back(origin);

    const double denominator = geometry.m_totalTimeRaw > 0
        ? static_cast<double>(geometry.m_totalTimeRaw)
        : 1.0;
    int elapsed = 0;
    const auto parameters = Xp60PatchLayout::patchToneTable().parameters();
    for (int stage = 1; stage <= 4; ++stage) {
        const auto index = static_cast<std::size_t>(stage - 1);
        elapsed += times[index];

        EnvelopePoint point;
        point.stage = stage;
        point.draggable = true;
        point.timeRaw = times[index];
        // The fourth stage of the Amplifier envelope has no level of its own:
        // it falls to silence. Its point is still draggable horizontally.
        const bool hasOwnLevel = stage <= geometry.m_stageCount;
        point.levelRaw = hasOwnLevel ? levels[index] : geometry.m_levelRawMin;
        point.x = geometry.m_totalTimeRaw > 0 ? static_cast<double>(elapsed) / denominator
                                              : static_cast<double>(stage) / 4.0;
        point.y = geometry.levelToY(point.levelRaw, stage);

        // ToneParameter's enumerators are one per row of the table, in table
        // order (the generator emits them that way), so the enum value is the
        // index. Roland's own names go on the point so a tooltip says "Time 2"
        // rather than something this project made up.
        point.timeName = parameters[static_cast<std::size_t>(spec.times[index])].name;
        if (hasOwnLevel) {
            point.levelName = parameters[static_cast<std::size_t>(spec.levels[index])].name;
        }
        geometry.m_points.push_back(point);
    }
    return geometry;
}

double EnvelopeGeometry::levelToY(int levelRaw, int stage) const
{
    if (stage > m_stageCount) {
        // The Amplifier envelope's release always lands on silence.
        return m_bipolar ? m_zeroLine : 0.0;
    }
    const double span = static_cast<double>(m_levelDisplayMax - m_levelDisplayMin);
    if (span <= 0.0) {
        return 0.0;
    }
    const double display = static_cast<double>(levelRaw * m_levelScale + m_levelOffset);
    return std::clamp((display - m_levelDisplayMin) / span, 0.0, 1.0);
}

std::optional<int> EnvelopeGeometry::yToLevel(double y, int stage) const
{
    if (stage > m_stageCount) {
        return std::nullopt;  // nothing to set: this stage has no level
    }
    const double span = static_cast<double>(m_levelDisplayMax - m_levelDisplayMin);
    if (span <= 0.0 || m_levelScale == 0) {
        return std::nullopt;
    }
    const double display = std::clamp(y, 0.0, 1.0) * span + m_levelDisplayMin;
    const double raw = (display - m_levelOffset) / m_levelScale;
    return std::clamp(static_cast<int>(std::lround(raw)), m_levelRawMin, m_levelRawMax);
}

EnvelopeGeometry::Hit EnvelopeGeometry::nearestDraggable(double x, double y) const
{
    Hit best;
    for (const auto& point : m_points) {
        if (!point.draggable) {
            continue;
        }
        const double dx = point.x - x;
        const double dy = point.y - y;
        const double distance = std::sqrt(dx * dx + dy * dy);
        if (!best.valid || distance < best.distance) {
            best.valid = true;
            best.distance = distance;
            best.stage = point.stage;
        }
    }
    return best;
}

} // namespace xp60studio::interaction
