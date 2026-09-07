#pragma once

#include "interaction/EnvelopeGeometry.h"
#include "xpmodel/Xp60Patch.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace xp60studio::interaction {

// One parameter the drag wants written, at its latest value.
struct DragEdit
{
    xpmodel::ToneParameter parameter{};
    std::string_view parameterName;
    int raw = 0;
};

// What the view should draw after one pointer sample.
struct DragFrame
{
    // Where to draw the grabbed handle. **Continuous**, not re-derived from the
    // quantised parameter — this is what makes a coarse parameter feel smooth
    // instead of sticky. See the class comment.
    double handleX = 0.0;
    double handleY = 0.0;
    // The whole curve, with the grabbed point at its continuous position and
    // the others where the current parameter values put them.
    std::vector<EnvelopePoint> points;
    // True when this sample actually moved a stored value. A view can use it to
    // decide whether to flash a value readout; the drawing does not depend on
    // it, and that is the point.
    bool valueChanged = false;
    // What the handle is pinned against right now, if anything: "Time 2 is at
    // its maximum". Empty when the drag is free.
    std::string limit;
};

// How the drag behaves. Every field is a real mechanism with a test, not a
// tuning knob nobody understands.
struct EnvelopeDragSettings
{
    // A stored value only changes once the continuous position has crossed the
    // next step's midpoint by this fraction of a step. Without it a hand
    // resting exactly on a boundary makes the value flicker between two
    // neighbours several times a second, which reads as noise and floods the
    // MIDI queue.
    double hysteresisSteps = 0.25;
    // Pointer travel below this (in plot units) is accumulated rather than
    // discarded, so a slow, careful drag never loses motion. This is the
    // difference between a control that responds to a one-pixel nudge and one
    // that ignores it.
    double motionEpsilon = 1e-9;
    // Horizontal drags change a stage's Time. Vertical drags change its Level.
    // Both at once is the normal case; either can be switched off for a view
    // that offers axis locks.
    bool horizontal = true;
    bool vertical = true;
};

// Dragging a point of an envelope, smoothly, over parameters that are coarse
// integers.
//
// ── The problem this exists to solve ────────────────────────────────────────
//
// An XP-60 envelope Time is an integer 0..127 and a Level often 0..127 too, but
// a plot is a few hundred pixels wide. Map the pointer straight onto the
// integer and read the integer back to draw, and the handle moves in visible
// jumps, sticks under the cursor, and stutters whenever the hand hovers on a
// boundary. That is the "glitchy and stuck" feel, and it is not caused by
// frame rate — it is caused by drawing from a quantised value.
//
// ── The four mechanisms that fix it ─────────────────────────────────────────
//
//  1. **A continuous shadow.** The drag keeps the grabbed point's position as a
//     double from the moment it was grabbed, and the view draws *that*. The
//     stored integer is derived from it, never the other way round. The handle
//     therefore tracks the pointer exactly, at whatever resolution the display
//     has, while the parameter underneath moves in its own steps.
//
//  2. **Hysteresis at the step boundary.** The integer only changes once the
//     shadow has passed the midpoint by a fraction of a step. A hand held still
//     on a boundary produces one change, not a stream of them.
//
//  3. **Exact accumulation.** Deltas are applied to the shadow in floating
//     point from the grab origin, not integrated per sample, so a thousand
//     one-pixel moves land in the same place as one thousand-pixel move. Drift
//     is impossible rather than merely small.
//
//  4. **Coalescing.** `takeEdits()` returns at most one entry per parameter,
//     holding its latest value. A drag producing 400 samples a second yields
//     one message per parameter per drain, which is what makes it safe to feed
//     a 31250-baud link (`ROADMAP.md` Phase 4: "do not send excessive MIDI
//     while dragging controls").
//
// ── What it does not do ─────────────────────────────────────────────────────
//
// It does not smooth, filter, predict or ease the pointer. Those make a control
// feel *different*, not more accurate, and on an editor whose whole purpose is
// exact values they put the handle somewhere the user did not put it. The
// smoothness here comes from not throwing information away in the first place.
//
// It also does not invent a time axis: horizontal position is proportional to
// the stored Time values, for the reason `EnvelopeGeometry` gives.
//
// Pure: no Qt, no timers, no I/O. A view feeds it pointer samples and draws
// what comes back; a service drains the edits at whatever rate the link allows.
class EnvelopeDrag
{
public:
    EnvelopeDrag() = default;
    explicit EnvelopeDrag(EnvelopeDragSettings settings) : m_settings(settings) {}

    [[nodiscard]] const EnvelopeDragSettings& settings() const noexcept { return m_settings; }
    void setSettings(EnvelopeDragSettings settings) { m_settings = settings; }

    // Grabs the point of `stage`. False when the stage has nothing draggable.
    // `patch` is the working copy the drag will edit; it is not written here.
    bool begin(const xpmodel::Xp60Patch& patch, xpmodel::ToneIndex tone, EnvelopeKind kind,
               int stage, double pointerX, double pointerY);
    [[nodiscard]] bool isDragging() const noexcept { return m_dragging; }
    [[nodiscard]] int stage() const noexcept { return m_stage; }

    // One pointer sample. Returns what to draw. Safe to call at any rate,
    // including faster than the display refreshes.
    [[nodiscard]] DragFrame move(const xpmodel::Xp60Patch& patch, double pointerX, double pointerY);

    // Ends the drag. The last frame's values stand.
    void end();
    // Abandons the drag and reports the edits needed to put every parameter
    // back where it was when `begin()` was called — an escape-key undo that
    // does not depend on the editor's history.
    [[nodiscard]] std::vector<DragEdit> cancel();

    // Pending edits, at most one per parameter, latest value, cleared by the
    // call. Drain this on a timer or whenever the link is free.
    [[nodiscard]] std::vector<DragEdit> takeEdits();
    [[nodiscard]] std::size_t pendingEditCount() const noexcept { return m_pending.size(); }
    // How many samples have been folded into the pending edits since the last
    // drain. A view can show it; a test asserts the coalescing actually works.
    [[nodiscard]] std::uint64_t coalescedSamples() const noexcept { return m_coalesced; }

private:
    void queue(xpmodel::ToneParameter parameter, std::string_view name, int raw);
    [[nodiscard]] int quantise(double shadow, int current, int rawMin, int rawMax) const;

    EnvelopeDragSettings m_settings;
    bool m_dragging = false;
    int m_stage = 0;
    xpmodel::ToneIndex m_tone = xpmodel::ToneIndex::tone1();
    EnvelopeKind m_kind = EnvelopeKind::Amplifier;

    double m_grabX = 0.0;
    double m_grabY = 0.0;
    // The continuous values the drag started from, and where they are now.
    double m_timeAtGrab = 0.0;
    double m_levelAtGrab = 0.0;
    double m_timeShadow = 0.0;
    double m_levelShadow = 0.0;
    int m_timeAtGrabRaw = 0;
    int m_levelAtGrabRaw = 0;
    // Plot units per raw step, fixed at grab time so the mapping cannot shift
    // under the pointer as neighbouring stages change the total width.
    double m_xPerTimeStep = 0.0;
    double m_yPerLevelStep = 0.0;

    xpmodel::ToneParameter m_timeParameter{};
    xpmodel::ToneParameter m_levelParameter{};
    std::string_view m_timeName;
    std::string_view m_levelName;
    bool m_hasLevel = false;
    int m_timeRawMin = 0;
    int m_timeRawMax = 127;
    int m_levelRawMin = 0;
    int m_levelRawMax = 127;

    std::vector<DragEdit> m_pending;
    std::uint64_t m_coalesced = 0;
};

} // namespace xp60studio::interaction
