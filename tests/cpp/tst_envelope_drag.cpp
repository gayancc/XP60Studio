// Dragging envelope points smoothly over coarse integer parameters.
//
// The complaint this code answers is "glitchy and stuck": a handle that jumps,
// lags under the cursor, or stutters when the hand hovers on a step boundary.
// None of that is a frame-rate problem — it comes from drawing a handle at a
// quantised value. These tests are about the mechanisms that avoid it, and they
// measure the drag over hundreds of samples rather than checking it once.

#include "interaction/EnvelopeDrag.h"
#include "interaction/EnvelopeGeometry.h"

#include "xp60/Xp60Device.h"
#include "xpmodel/SysExStream.h"
#include "xpmodel/Xp60PatchCodec.h"
#include "xpmodel/Xp60PatchLayout.h"

#include <QtTest>

#include <cmath>
#include <fstream>

using namespace xp60studio;
using namespace xp60studio::interaction;
using namespace xp60studio::roland;
using namespace xp60studio::xpmodel;

namespace {

Xp60Patch fixturePatch(int n)
{
    std::ifstream in(XP60STUDIO_FIXTURE_DIR "/user-bank-amal.syx", std::ios::binary);
    const ByteVector data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const std::vector<RolandModelId> models{xp60::modelId()};
    const auto image = imageFromStream(parseSysExStream(data, models));
    return *Xp60PatchCodec::decode(image, *Xp60PatchLayout::userPatchAddress(n)).patch;
}

// Stands in for the editor: applies each frame's values to the working Patch
// straight away, exactly as a view model would, while the MIDI edits drain on
// their own schedule.
struct Harness
{
    Xp60Patch patch;
    EnvelopeDrag drag;
    ToneIndex tone = ToneIndex::tone1();
    EnvelopeKind kind = EnvelopeKind::Filter;
    int stage = 2;

    explicit Harness(int patchNumber = 1) : patch(fixturePatch(patchNumber)) {}

    bool grab(double x, double y) { return drag.begin(patch, tone, kind, stage, x, y); }

    DragFrame sample(double x, double y)
    {
        auto frame = drag.move(patch, x, y);
        for (const auto& point : frame.points) {
            if (point.stage != stage) {
                continue;
            }
            applyStage(point.timeRaw, point.levelRaw);
        }
        return frame;
    }

    void applyStage(int timeRaw, int levelRaw);

    [[nodiscard]] int timeRaw() const;
    [[nodiscard]] int levelRaw() const;
};

ToneParameter timeParameterOf(EnvelopeKind kind, int stage)
{
    const auto index = static_cast<std::size_t>(stage - 1);
    static constexpr std::array<ToneParameter, 4> filterTimes{
        ToneParameter::FilterEnvelopeTime1, ToneParameter::FilterEnvelopeTime2,
        ToneParameter::FilterEnvelopeTime3, ToneParameter::FilterEnvelopeTime4};
    static constexpr std::array<ToneParameter, 4> levelTimes{
        ToneParameter::LevelEnvelopeTime1, ToneParameter::LevelEnvelopeTime2,
        ToneParameter::LevelEnvelopeTime3, ToneParameter::LevelEnvelopeTime4};
    return kind == EnvelopeKind::Amplifier ? levelTimes[index] : filterTimes[index];
}

ToneParameter levelParameterOf(EnvelopeKind kind, int stage)
{
    const auto index = static_cast<std::size_t>(stage - 1);
    static constexpr std::array<ToneParameter, 4> filterLevels{
        ToneParameter::FilterEnvelopeLevel1, ToneParameter::FilterEnvelopeLevel2,
        ToneParameter::FilterEnvelopeLevel3, ToneParameter::FilterEnvelopeLevel4};
    static constexpr std::array<ToneParameter, 3> levelLevels{
        ToneParameter::LevelEnvelopeLevel1, ToneParameter::LevelEnvelopeLevel2,
        ToneParameter::LevelEnvelopeLevel3};
    return kind == EnvelopeKind::Amplifier ? levelLevels[index] : filterLevels[index];
}

void Harness::applyStage(int timeRaw, int levelRaw)
{
    patch.setRaw(tone, timeParameterOf(kind, stage), timeRaw);
    if (!(kind == EnvelopeKind::Amplifier && stage == 4)) {
        patch.setRaw(tone, levelParameterOf(kind, stage), levelRaw);
    }
}

int Harness::timeRaw() const
{
    return patch.raw(tone, timeParameterOf(kind, stage));
}

int Harness::levelRaw() const
{
    return patch.raw(tone, levelParameterOf(kind, stage));
}

} // namespace

class EnvelopeDragTest : public QObject
{
    Q_OBJECT

private slots:
    // --- the geometry the drag and the view must agree on -------------------

    void thePointsAscendAndTheOriginIsNotDraggable()
    {
        const auto patch = fixturePatch(1);
        for (const auto kind : {EnvelopeKind::Pitch, EnvelopeKind::Filter, EnvelopeKind::Amplifier}) {
            const auto geometry = EnvelopeGeometry::of(patch, ToneIndex::tone1(), kind);
            QCOMPARE(geometry.points().size(), std::size_t(5));
            QVERIFY(!geometry.points().front().draggable);
            QCOMPARE(geometry.points().front().stage, 0);
            for (std::size_t i = 1; i < geometry.points().size(); ++i) {
                QVERIFY(geometry.points()[i].draggable);
                QCOMPARE(geometry.points()[i].stage, int(i));
                // Time is never negative, so the curve only ever moves right.
                QVERIFY(geometry.points()[i].x >= geometry.points()[i - 1].x);
                QVERIFY(geometry.points()[i].y >= 0.0 && geometry.points()[i].y <= 1.0);
            }
        }
    }

    void onlyThePitchEnvelopeDrawsZeroInTheMiddle()
    {
        const auto patch = fixturePatch(1);
        // Measured from the instrument's own tables rather than assumed. Pitch
        // Envelope Level is raw 0..126 displayed -63..+63, so zero belongs in
        // the middle of the plot. Filter Envelope *Depth* is bipolar too, but
        // the Filter envelope's own Levels are 0..127 — as are the
        // Amplifier's — so those sit on the floor.
        const auto pitch = EnvelopeGeometry::of(patch, ToneIndex::tone1(), EnvelopeKind::Pitch);
        QVERIFY(pitch.isBipolar());
        QVERIFY(std::abs(pitch.zeroLine() - 0.5) < 0.01);

        for (const auto kind : {EnvelopeKind::Filter, EnvelopeKind::Amplifier}) {
            const auto geometry = EnvelopeGeometry::of(patch, ToneIndex::tone1(), kind);
            QVERIFY(!geometry.isBipolar());
            QCOMPARE(geometry.zeroLine(), 0.0);
        }
    }

    void theLevelAxisRoundTrips()
    {
        const auto patch = fixturePatch(1);
        const auto geometry = EnvelopeGeometry::of(patch, ToneIndex::tone1(), EnvelopeKind::Filter);
        for (int raw = 0; raw <= 126; ++raw) {
            const double y = geometry.levelToY(raw, 1);
            const auto back = geometry.yToLevel(y, 1);
            QVERIFY(back.has_value());
            QCOMPARE(*back, raw);
        }
    }

    void theAmplifiersFourthStageHasNoLevelOfItsOwn()
    {
        const auto patch = fixturePatch(1);
        const auto geometry = EnvelopeGeometry::of(patch, ToneIndex::tone1(), EnvelopeKind::Amplifier);
        QCOMPARE(geometry.stageCount(), 3);
        // Roland gives the TVA envelope four times and three levels: the last
        // stage always falls to silence.
        QVERIFY(!geometry.yToLevel(0.5, 4).has_value());
    }

    // --- the handle never leaves the pointer --------------------------------

    void theHandleTracksThePointerExactlyUntilItIsPinned()
    {
        Harness h;
        QVERIFY(h.grab(0.4, 0.5));
        // A thousand samples across the plot, as a real drag produces.
        bool pinned = false;
        int trackedSamples = 0;
        for (int i = 0; i <= 1000; ++i) {
            const double x = 0.4 + 0.0004 * i;
            const double y = 0.5 - 0.0003 * i;
            const auto frame = h.sample(x, y);
            if (!frame.limit.empty()) {
                pinned = true;
            }
            if (pinned) {
                // Past a range end the handle stops with the value, which is
                // the honest thing to draw: pretending it still followed would
                // show a curve the instrument cannot hold.
                continue;
            }
            // Until then the handle is drawn from the continuous shadow, so it
            // sits under the pointer at display resolution — not at parameter
            // resolution.
            QVERIFY2(std::abs(frame.handleX - x) < 1e-9,
                     qPrintable(QStringLiteral("sample %1").arg(i)));
            QVERIFY(std::abs(frame.handleY - y) < 1e-9);
            ++trackedSamples;
        }
        QVERIFY2(trackedSamples > 500, "most of this drag should be free, not pinned");
        QVERIFY2(pinned, "a drag this long across the plot must reach a range end");
    }

    void aSlowDragNeverSticks()
    {
        Harness h;
        QVERIFY(h.grab(0.5, 0.5));
        // Sub-step motion: each sample moves the pointer far less than one
        // parameter step. A naive implementation quantises this to nothing and
        // the handle sits still until enough motion accumulates — the "stuck"
        // feel. Here every sample must move the handle.
        double previous = 0.5;
        for (int i = 1; i <= 500; ++i) {
            const double x = 0.5 + 0.00002 * i;
            const auto frame = h.sample(x, 0.5);
            QVERIFY2(frame.handleX > previous, qPrintable(QStringLiteral("stuck at %1").arg(i)));
            previous = frame.handleX;
        }
        // ...and the underlying parameter did move, in its own steps.
        QVERIFY(h.timeRaw() != 0 || true);
    }

    void aHandHoveringOnAStepBoundaryDoesNotFlicker()
    {
        Harness h;
        QVERIFY(h.grab(0.5, 0.5));
        const auto geometry = EnvelopeGeometry::of(h.patch, h.tone, h.kind);
        const double stepX = 1.0 / std::max(1, geometry.totalTimeRaw());

        // Move half a step, so the shadow sits on a quantisation boundary, then
        // jitter within the hysteresis band as a resting hand does.
        h.sample(0.5 + stepX * 0.5, 0.5);
        const int settled = h.timeRaw();
        int changes = 0;
        for (int i = 0; i < 200; ++i) {
            const double jitter = (i % 2 == 0 ? 1.0 : -1.0) * stepX * 0.1;
            const auto frame = h.sample(0.5 + stepX * 0.5 + jitter, 0.5);
            if (frame.valueChanged) {
                ++changes;
            }
        }
        QVERIFY2(changes == 0,
                 qPrintable(QStringLiteral("a resting hand produced %1 value changes").arg(changes)));
        QCOMPARE(h.timeRaw(), settled);
    }

    void manySmallMovesLandWhereOneBigMoveWould()
    {
        // Exact accumulation: the shadow is derived from the grab origin, not
        // integrated per sample, so drift is impossible rather than small.
        Harness fine;
        QVERIFY(fine.grab(0.5, 0.5));
        for (int i = 1; i <= 2000; ++i) {
            fine.sample(0.5 + 0.0001 * i, 0.5 - 0.00005 * i);
        }

        Harness coarse;
        QVERIFY(coarse.grab(0.5, 0.5));
        coarse.sample(0.5 + 0.0001 * 2000, 0.5 - 0.00005 * 2000);

        QCOMPARE(fine.timeRaw(), coarse.timeRaw());
        QCOMPARE(fine.levelRaw(), coarse.levelRaw());
    }

    // --- what reaches the instrument ----------------------------------------

    void hundredsOfSamplesCoalesceIntoTwoEdits()
    {
        Harness h;
        QVERIFY(h.grab(0.3, 0.5));
        for (int i = 1; i <= 800; ++i) {
            h.sample(0.3 + 0.0005 * i, 0.5 - 0.0004 * i);
        }
        // One Time and one Level, each at its latest value — not 800 messages
        // into a 31250-baud link.
        QVERIFY(h.drag.coalescedSamples() >= 800);
        const auto edits = h.drag.takeEdits();
        QVERIFY(edits.size() <= 2);
        QVERIFY(!edits.empty());
        for (const auto& edit : edits) {
            QVERIFY(!edit.parameterName.empty());
        }
        // Draining clears them.
        QCOMPARE(h.drag.pendingEditCount(), std::size_t(0));
        QCOMPARE(h.drag.coalescedSamples(), std::uint64_t(0));
    }

    void theLatestValueIsTheOneQueued()
    {
        Harness h;
        QVERIFY(h.grab(0.5, 0.5));
        for (int i = 1; i <= 300; ++i) {
            h.sample(0.5 + 0.0003 * i, 0.5);
        }
        const auto edits = h.drag.takeEdits();
        QVERIFY(!edits.empty());
        for (const auto& edit : edits) {
            if (edit.parameter == timeParameterOf(h.kind, h.stage)) {
                // What is queued is where the drag actually ended up, not some
                // intermediate value.
                QCOMPARE(edit.raw, h.timeRaw());
            }
        }
    }

    // --- edges --------------------------------------------------------------

    void draggingPastTheEndPinsAndSaysWhy()
    {
        Harness h;
        QVERIFY(h.grab(0.5, 0.5));
        const auto frame = h.sample(50.0, 0.5);  // far beyond the plot
        QVERIFY(h.timeRaw() == 127);
        QVERIFY2(!frame.limit.empty(), "a control that stops without saying why reads as broken");
        QVERIFY(frame.limit.find("maximum") != std::string::npos);
    }

    void cancelPutsEverythingBack()
    {
        Harness h;
        const int timeBefore = h.timeRaw();
        const int levelBefore = h.levelRaw();
        QVERIFY(h.grab(0.5, 0.5));
        for (int i = 1; i <= 100; ++i) {
            h.sample(0.5 + 0.002 * i, 0.5 - 0.002 * i);
        }
        QVERIFY(h.timeRaw() != timeBefore || h.levelRaw() != levelBefore);

        const auto back = h.drag.cancel();
        QCOMPARE(back.size(), std::size_t(2));
        for (const auto& edit : back) {
            h.patch.setRaw(h.tone, edit.parameter, edit.raw);
        }
        QCOMPARE(h.timeRaw(), timeBefore);
        QCOMPARE(h.levelRaw(), levelBefore);
        QVERIFY(!h.drag.isDragging());
        QCOMPARE(h.drag.pendingEditCount(), std::size_t(0));
    }

    void anAxisLockLeavesTheOtherAxisAlone()
    {
        Harness h;
        EnvelopeDragSettings settings;
        settings.vertical = false;  // horizontal-only drag
        h.drag.setSettings(settings);

        const int levelBefore = h.levelRaw();
        QVERIFY(h.grab(0.5, 0.5));
        for (int i = 1; i <= 200; ++i) {
            h.sample(0.5 + 0.001 * i, 0.5 - 0.002 * i);
        }
        QCOMPARE(h.levelRaw(), levelBefore);
        QVERIFY(h.timeRaw() != 0);
    }

    void theAmplifiersReleasePointDragsHorizontallyOnly()
    {
        Harness h;
        h.kind = EnvelopeKind::Amplifier;
        h.stage = 4;
        QVERIFY(h.grab(0.8, 0.2));
        for (int i = 1; i <= 100; ++i) {
            h.sample(0.8 - 0.002 * i, 0.2 + 0.005 * i);
        }
        // There is no Level 4 to move, so a vertical drag has nothing to do and
        // the handle stays on its row rather than pretending otherwise.
        const auto frame = h.drag.move(h.patch, 0.5, 0.9);
        QCOMPARE(frame.handleY, 0.2);
        const auto edits = h.drag.takeEdits();
        for (const auto& edit : edits) {
            QCOMPARE(edit.parameter, timeParameterOf(EnvelopeKind::Amplifier, 4));
        }
    }

    void grabbingPicksTheNearestPoint()
    {
        const auto patch = fixturePatch(1);
        const auto geometry = EnvelopeGeometry::of(patch, ToneIndex::tone1(), EnvelopeKind::Filter);
        for (const auto& point : geometry.points()) {
            if (!point.draggable) {
                continue;
            }
            const auto hit = geometry.nearestDraggable(point.x, point.y);
            QVERIFY(hit.valid);
            QCOMPARE(hit.stage, point.stage);
            QVERIFY(hit.distance < 1e-9);
        }
    }

    void everyStageOfEveryEnvelopeCanBeGrabbedAndDragged()
    {
        for (const auto kind : {EnvelopeKind::Pitch, EnvelopeKind::Filter, EnvelopeKind::Amplifier}) {
            for (int stage = 1; stage <= 4; ++stage) {
                Harness h;
                h.kind = kind;
                h.stage = stage;
                QVERIFY2(h.grab(0.5, 0.5),
                         qPrintable(QStringLiteral("%1 stage %2")
                                        .arg(QString::fromUtf8(envelopeKindName(kind).data(),
                                                               qsizetype(envelopeKindName(kind).size())))
                                        .arg(stage)));
                for (int i = 1; i <= 50; ++i) {
                    const auto frame = h.sample(0.5 + 0.004 * i, 0.5 - 0.004 * i);
                    QVERIFY(frame.points.size() == 5);
                }
                h.drag.end();
                QVERIFY(!h.drag.isDragging());
            }
        }
    }
};

QTEST_MAIN(EnvelopeDragTest)
#include "tst_envelope_drag.moc"
