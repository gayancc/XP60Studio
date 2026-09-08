#pragma once

#include "xpmodel/Xp60Patch.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace xp60studio::interaction {

// One draggable point of an envelope, in plot coordinates.
//
// Both axes are 0..1. Nothing here is in seconds or decibels, and that is a
// decision rather than an omission — see `EnvelopeGeometry`.
struct EnvelopePoint
{
    double x = 0.0;
    double y = 0.0;
    // The stage this point ends: 1..4 for T1/L1 .. T4/L4. 0 for the origin,
    // which is not draggable.
    int stage = 0;
    bool draggable = false;
    // The raw values behind it, as stored in the Patch.
    int timeRaw = 0;
    int levelRaw = 0;
    // Roland's own names, so a tooltip says "Time 2" and "Level 2" rather than
    // something this project invented.
    std::string_view timeName;
    std::string_view levelName;
};

// Which envelope of a Tone is being drawn.
enum class EnvelopeKind { Pitch, Filter, Amplifier };

[[nodiscard]] std::string_view envelopeKindName(EnvelopeKind kind) noexcept;

// An envelope as a curve that can be drawn and grabbed.
//
// ── Why the axes carry no units ─────────────────────────────────────────────
//
// Roland documents envelope Time as a value 0..127 and nowhere states what any
// of those values is in milliseconds. The relationship is certainly not linear
// — no synthesizer's is — but this project has no measurement of it and will
// not invent one. Drawing an axis labelled "seconds" from a curve nobody
// measured would be the most convincing kind of wrong: the picture would look
// authoritative and would misinform every judgement made from it.
//
// So the horizontal axis is **proportional to the stored Time values**. That is
// a faithful picture of the data — a stage with twice the Time value is twice
// as wide — and it is honest about being a picture of the data rather than of
// elapsed time. It is also exactly what a musician needs to grab and drag,
// because what they are editing *is* the stored value.
//
// If the times are ever measured on hardware (`DEVICE_ACCEPTANCE.md` would own
// that), a real time axis becomes a second, clearly-labelled mode. Until then
// there is one axis and it says what it is.
//
// The vertical axis is the level's documented display range mapped to 0..1, so
// a bipolar envelope (Pitch, Filter) shows zero in the middle where it belongs
// and a unipolar one (Amplifier) sits on the floor.
class EnvelopeGeometry
{
public:
    // The Amplifier envelope has three levels; the others have four.
    static constexpr std::size_t kMaxPoints = 5;

    [[nodiscard]] static EnvelopeGeometry of(const xpmodel::Xp60Patch& patch,
                                             xpmodel::ToneIndex tone, EnvelopeKind kind);

    [[nodiscard]] EnvelopeKind kind() const noexcept { return m_kind; }
    [[nodiscard]] xpmodel::ToneIndex tone() const noexcept { return m_tone; }
    // Origin first, then one point per stage, ascending in x.
    [[nodiscard]] const std::vector<EnvelopePoint>& points() const noexcept { return m_points; }
    [[nodiscard]] int stageCount() const noexcept { return m_stageCount; }
    // True when zero level sits in the middle of the plot rather than at the
    // bottom: Pitch and Filter envelopes cut as well as boost.
    [[nodiscard]] bool isBipolar() const noexcept { return m_bipolar; }
    // Where zero level falls on the 0..1 vertical axis.
    [[nodiscard]] double zeroLine() const noexcept { return m_zeroLine; }
    // Sum of the stored Time values. The horizontal axis is normalized by this,
    // so a curve whose times are all zero still draws (as a vertical stack)
    // rather than dividing by nothing.
    [[nodiscard]] int totalTimeRaw() const noexcept { return m_totalTimeRaw; }

    // The point nearest to a plot position, and how far away it is, so a view
    // can decide whether a press grabbed anything. Distance is in plot units
    // with the two axes weighted equally; a caller with a non-square plot
    // should scale before asking.
    struct Hit
    {
        int stage = 0;
        double distance = 0.0;
        bool valid = false;
    };
    [[nodiscard]] Hit nearestDraggable(double x, double y) const;

    // Level and Time axis conversions for one stage, exposed because the drag
    // engine and the view must agree on them exactly.
    [[nodiscard]] double levelToY(int levelRaw, int stage) const;
    [[nodiscard]] std::optional<int> yToLevel(double y, int stage) const;

private:
    xpmodel::ToneIndex m_tone = xpmodel::ToneIndex::tone1();
    EnvelopeKind m_kind = EnvelopeKind::Amplifier;
    std::vector<EnvelopePoint> m_points;
    int m_stageCount = 4;
    bool m_bipolar = false;
    double m_zeroLine = 0.0;
    int m_totalTimeRaw = 0;
    // Documented display bounds of this envelope's level parameters.
    int m_levelDisplayMin = 0;
    int m_levelDisplayMax = 127;
    int m_levelRawMin = 0;
    int m_levelRawMax = 127;
    int m_levelScale = 1;
    int m_levelOffset = 0;
};

} // namespace xp60studio::interaction
