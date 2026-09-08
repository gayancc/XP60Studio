// The LFO panel: a curve drawn from stored values, and knobs that drag
// smoothly over coarse parameters.
//
// Same discipline as the envelope editor. The curve claims no frequency because
// Roland publishes none, and the drag keeps its resolution instead of trying to
// recover it after quantising.

#include "interaction/LfoGeometry.h"
#include "interaction/ParameterDrag.h"

#include "xp60/Xp60Device.h"
#include "xpmodel/SysExStream.h"
#include "xpmodel/Xp60PatchCodec.h"
#include "xpmodel/Xp60PatchLayout.h"

#include <QtTest>

#include <algorithm>
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

const ParameterDescriptor& descriptor(std::string_view id)
{
    const auto* found = Xp60PatchLayout::patchToneTable().find(id);
    Q_ASSERT(found != nullptr);
    return *found;
}

} // namespace

class LfoPanelTest : public QObject
{
    Q_OBJECT

private slots:
    // --- the curve ----------------------------------------------------------

    void everyWaveformDrawsInsideThePlot()
    {
        auto patch = fixturePatch(1);
        for (int raw = 0; raw <= 7; ++raw) {
            QVERIFY(patch.setRaw(ToneIndex::tone1(), ToneParameter::Lfo1Waveform, raw));
            const auto lfo = LfoGeometry::of(patch, ToneIndex::tone1(), LfoGeometry::Which::Lfo1);
            QCOMPARE(static_cast<int>(lfo.waveform()), raw);
            QVERIFY(!lfoWaveformName(lfo.waveform()).empty());
            QVERIFY(!lfoWaveformLabel(lfo.waveform()).empty());

            const auto curve = lfo.curve(256);
            QCOMPARE(curve.size(), std::size_t(256));
            QCOMPARE(curve.front().x, 0.0);
            QCOMPARE(curve.back().x, 1.0);
            for (std::size_t i = 0; i < curve.size(); ++i) {
                QVERIFY(curve[i].y >= 0.0 && curve[i].y <= 1.0);
                if (i > 0) {
                    QVERIFY(curve[i].x > curve[i - 1].x);
                }
            }
        }
    }

    void theAperiodicWaveformsSayThatTheyAre()
    {
        // Sample & Hold, Random and Chaos have no fixed shape, so a plot that
        // drew one as a repeating curve without saying so would misrepresent
        // the instrument.
        QVERIFY(lfoWaveformIsAperiodic(LfoWaveform::SampleAndHold));
        QVERIFY(lfoWaveformIsAperiodic(LfoWaveform::Random));
        QVERIFY(lfoWaveformIsAperiodic(LfoWaveform::Chaos));
        for (const auto waveform : {LfoWaveform::Triangle, LfoWaveform::Sine, LfoWaveform::Sawtooth,
                                    LfoWaveform::Square, LfoWaveform::Trapezoid}) {
            QVERIFY(!lfoWaveformIsAperiodic(waveform));
        }
    }

    void anAperiodicCurveIsStableAcrossRepaints()
    {
        auto patch = fixturePatch(1);
        QVERIFY(patch.setRaw(ToneIndex::tone1(), ToneParameter::Lfo1Waveform,
                             static_cast<int>(LfoWaveform::Random)));
        const auto lfo = LfoGeometry::of(patch, ToneIndex::tone1(), LfoGeometry::Which::Lfo1);

        // A plot that reshuffled itself every repaint would look like a fault.
        const auto first = lfo.curve(128);
        const auto second = lfo.curve(128);
        QCOMPARE(first.size(), second.size());
        for (std::size_t i = 0; i < first.size(); ++i) {
            QCOMPARE(first[i].y, second[i].y);
        }
    }

    void aFasterRateDrawsMoreCycles()
    {
        auto patch = fixturePatch(1);
        QVERIFY(patch.setRaw(ToneIndex::tone1(), ToneParameter::Lfo1Waveform,
                             static_cast<int>(LfoWaveform::Sine)));
        QVERIFY(patch.setRaw(ToneIndex::tone1(), ToneParameter::Lfo1DelayTime, 0));
        QVERIFY(patch.setRaw(ToneIndex::tone1(), ToneParameter::Lfo1FadeTime, 0));

        QVERIFY(patch.setRaw(ToneIndex::tone1(), ToneParameter::Lfo1Rate, 0));
        const auto slow = LfoGeometry::of(patch, ToneIndex::tone1(), LfoGeometry::Which::Lfo1);
        QVERIFY(patch.setRaw(ToneIndex::tone1(), ToneParameter::Lfo1Rate, 127));
        const auto fast = LfoGeometry::of(patch, ToneIndex::tone1(), LfoGeometry::Which::Lfo1);

        QVERIFY(fast.cycles() > slow.cycles());
        // Rate zero still draws a shape rather than a flat line nobody can read.
        QVERIFY(slow.cycles() >= 1.0);
    }

    void delayAndFadeShadeTheStartOfThePlot()
    {
        auto patch = fixturePatch(1);
        QVERIFY(patch.setRaw(ToneIndex::tone1(), ToneParameter::Lfo1Waveform,
                             static_cast<int>(LfoWaveform::Sine)));
        QVERIFY(patch.setRaw(ToneIndex::tone1(), ToneParameter::Lfo1Rate, 100));
        QVERIFY(patch.setRaw(ToneIndex::tone1(), ToneParameter::Lfo1DelayTime, 64));
        QVERIFY(patch.setRaw(ToneIndex::tone1(), ToneParameter::Lfo1FadeTime, 0));

        const auto lfo = LfoGeometry::of(patch, ToneIndex::tone1(), LfoGeometry::Which::Lfo1);
        QVERIFY(lfo.delayFraction() > 0.0);
        // A long delay must not push the waveform off the plot entirely.
        QVERIFY(lfo.delayFraction() <= 0.5);

        // Before the delay elapses the LFO is not running, so the curve rests
        // on the centre line rather than pretending to start at note-on.
        for (const auto& sample : lfo.curve(400)) {
            if (sample.x < lfo.delayFraction() - 0.01) {
                QVERIFY(std::abs(sample.y - 0.5) < 1e-9);
            }
        }
    }

    void bothLfosAreReadIndependently()
    {
        auto patch = fixturePatch(1);
        QVERIFY(patch.setRaw(ToneIndex::tone1(), ToneParameter::Lfo1Rate, 10));
        QVERIFY(patch.setRaw(ToneIndex::tone1(), ToneParameter::Lfo2Rate, 120));
        const auto one = LfoGeometry::of(patch, ToneIndex::tone1(), LfoGeometry::Which::Lfo1);
        const auto two = LfoGeometry::of(patch, ToneIndex::tone1(), LfoGeometry::Which::Lfo2);
        QCOMPARE(one.rateRaw(), 10);
        QCOMPARE(two.rateRaw(), 120);
    }

    // --- the knobs ----------------------------------------------------------

    void aKnobTracksThePointerAtDisplayResolution()
    {
        ParameterDrag drag;
        const auto& rate = descriptor("tone.lfo1_rate");
        QVERIFY(drag.begin(rate, 64, 0.0));

        double previous = -1.0;
        for (int i = 1; i <= 400; ++i) {
            // Sub-step motion: less than one parameter step per sample. A
            // control drawn from the quantised value stands still here.
            const auto frame = drag.move(0.2 * i);
            QVERIFY2(frame.position > previous, qPrintable(QStringLiteral("stuck at %1").arg(i)));
            previous = frame.position;
            QVERIFY(frame.position >= 0.0 && frame.position <= 1.0);
        }
    }

    void aCoarseParameterStillMovesSmoothly()
    {
        // Wave Gain has four legal values. Drawn from the quantised value it
        // snaps between four positions and feels broken; drawn from the shadow
        // it moves continuously and lands where the hand put it.
        ParameterDrag drag;
        const auto& gain = descriptor("tone.wave_gain");
        QCOMPARE(gain.rawMax - gain.rawMin, 3);
        QVERIFY(drag.begin(gain, 0, 0.0));

        double previous = -1.0;
        int distinctValues = 0;
        int last = -1;
        for (int i = 1; i <= 200; ++i) {
            const auto frame = drag.move(1.0 * i);
            QVERIFY(frame.position >= previous);
            previous = frame.position;
            if (frame.raw != last) {
                last = frame.raw;
                ++distinctValues;
            }
        }
        // The value passed through its four steps...
        QVERIFY(distinctValues >= 3);
        // ...while the drawn position moved far more finely than four places.
        QVERIFY(previous > 0.9);
    }

    void aRestingHandOnABoundaryChangesNothing()
    {
        ParameterDrag drag;
        ParameterDragSettings settings;
        settings.travelForFullRange = 127.0;  // one unit per step
        drag.setSettings(settings);
        const auto& rate = descriptor("tone.lfo1_rate");
        QVERIFY(drag.begin(rate, 64, 0.0));

        drag.move(0.5);  // sitting exactly on a step boundary
        const int settled = drag.takePendingValue();
        int changes = 0;
        for (int i = 0; i < 200; ++i) {
            const double jitter = (i % 2 == 0 ? 0.1 : -0.1);
            if (drag.move(0.5 + jitter).valueChanged) {
                ++changes;
            }
        }
        QCOMPARE(changes, 0);
        QCOMPARE(drag.takePendingValue(), settled);
    }

    void manySmallMovesLandWhereOneBigMoveWould()
    {
        const auto& rate = descriptor("tone.lfo1_rate");
        ParameterDrag fine;
        QVERIFY(fine.begin(rate, 20, 0.0));
        for (int i = 1; i <= 3000; ++i) {
            fine.move(0.05 * i);
        }
        ParameterDrag coarse;
        QVERIFY(coarse.begin(rate, 20, 0.0));
        coarse.move(0.05 * 3000);

        QCOMPARE(fine.takePendingValue(), coarse.takePendingValue());
    }

    void everySampleCoalescesIntoOneValue()
    {
        ParameterDrag drag;
        const auto& rate = descriptor("tone.lfo1_rate");
        QVERIFY(drag.begin(rate, 0, 0.0));
        for (int i = 1; i <= 900; ++i) {
            drag.move(0.3 * i);
        }
        QVERIFY(drag.coalescedSamples() >= 900);
        QVERIFY(drag.hasPendingValue());
        const int value = drag.takePendingValue();
        // One value, the latest — not 900 messages into a 31250-baud link.
        QVERIFY(!drag.hasPendingValue());
        QCOMPARE(drag.coalescedSamples(), std::uint64_t(0));
        QCOMPARE(value, rate.rawMax);
    }

    void draggingPastTheEndPinsAndSaysWhy()
    {
        ParameterDrag drag;
        const auto& rate = descriptor("tone.lfo1_rate");
        QVERIFY(drag.begin(rate, 64, 0.0));
        const auto frame = drag.move(100000.0);
        QCOMPARE(frame.raw, rate.rawMax);
        QCOMPARE(frame.position, 1.0);
        QVERIFY(frame.limit.find("maximum") != std::string::npos);
        QVERIFY(frame.limit.find("LFO1 Rate") != std::string::npos);
    }

    void cancelReturnsTheValueTheGrabStartedFrom()
    {
        ParameterDrag drag;
        const auto& rate = descriptor("tone.lfo1_rate");
        QVERIFY(drag.begin(rate, 42, 0.0));
        drag.move(500.0);
        QVERIFY(drag.hasPendingValue());
        QCOMPARE(drag.cancel(), 42);
        QVERIFY(!drag.hasPendingValue());
        QVERIFY(!drag.isDragging());
    }

    void aParameterWithNoRangeCannotBeGrabbed()
    {
        // A one-value field has nothing to drag, and saying so is better than
        // handing back a control that does nothing.
        ParameterDescriptor fixed;
        fixed.name = "Fixed";
        fixed.rawMin = 5;
        fixed.rawMax = 5;
        ParameterDrag drag;
        QVERIFY(!drag.begin(fixed, 5, 0.0));
        QVERIFY(!drag.isDragging());
    }

    void sensitivityScalesTheDragWithoutChangingTheRules()
    {
        const auto& rate = descriptor("tone.lfo1_rate");
        ParameterDrag normal;
        QVERIFY(normal.begin(rate, 0, 0.0));
        normal.move(100.0);
        const int coarse = normal.takePendingValue();

        ParameterDrag fine;
        ParameterDragSettings settings;
        settings.sensitivity = 0.25;  // a fine-drag modifier
        fine.setSettings(settings);
        QVERIFY(fine.begin(rate, 0, 0.0));
        fine.move(100.0);
        const int precise = fine.takePendingValue();

        QVERIFY2(precise < coarse, "a fine drag covers less ground for the same travel");
        QVERIFY(precise > 0);
    }
};

QTEST_MAIN(LfoPanelTest)
#include "tst_lfo_panel.moc"
