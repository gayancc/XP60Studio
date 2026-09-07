// Phase 8 — the Performance editor's view model and 16-Part mixer data.
//
// Runs over the golden fixture's real Performances rather than constructed
// ones, so the strips are exercised against the values a musician's own
// instrument actually holds.

#include "midi/LoopbackMidiTransport.h"
#include "presentation/PerformanceViewModel.h"
#include "support/FakeXp60.h"
#include "xp60/Xp60Device.h"
#include "xpmodel/SysExStream.h"
#include "xpmodel/Xp60PerformanceCodec.h"

#include <QCoreApplication>
#include <QFile>
#include <QSignalSpy>
#include <QTest>

#include <chrono>
#include <memory>

using namespace std::chrono_literals;

using namespace xp60studio;
using presentation::PerformanceViewModel;
using xpmodel::PartIndex;
using xpmodel::PerformanceCommonParameter;
using xpmodel::PerformancePartParameter;
using xpmodel::Xp60PerformanceLayout;

namespace {

roland::ByteVector readFixture()
{
    QFile file(QStringLiteral(XP60STUDIO_FIXTURE_DIR "/user-bank-amal.syx"));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QByteArray bytes = file.readAll();
    return roland::ByteVector(reinterpret_cast<const roland::Byte*>(bytes.constData()),
                              reinterpret_cast<const roland::Byte*>(bytes.constData()) + bytes.size());
}

QVariantMap strip(const PerformanceViewModel& model, int partNumber)
{
    return model.parts().at(partNumber - 1).toMap();
}

} // namespace

class TestPerformanceEditor : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    void emptyUntilAPerformanceIsAdopted();
    void everyStripCarriesWhatTheMixerDraws();
    void editsAreUndoableAndRefuseValuesOutsideTheMap();
    void aCrossingKeyRangeIsRefusedWhole();
    void voiceReserveIsEditedThroughTheStripButLivesInCommon();
    void revertGoesBackToWhatWasFetched();
    void sendingNeedsAConnectedInstrument();
    void aPersistentWriteIsRefusedUntilThereIsAnInstrumentAndAnArm();
    void theWritePlanNamesTheDestinationBeforeAnythingIsArmed();
    void writingToAUserSlotVerifiesAndTheSnapshotPutsItBack();

private:
    [[nodiscard]] xpmodel::Xp60Performance fixturePerformance(int userNumber) const;

    xpmodel::MemoryImage m_image;
    std::unique_ptr<services::DeviceSession> m_session;
    std::unique_ptr<PerformanceViewModel> m_model;
};

void TestPerformanceEditor::initTestCase()
{
    const auto bytes = readFixture();
    QVERIFY2(!bytes.empty(), "golden fixture missing");
    const std::vector<roland::RolandModelId> models{xp60::modelId()};
    const auto stream = xpmodel::parseSysExStream(bytes, models);
    QVERIFY(stream.isClean());
    m_image = xpmodel::imageFromStream(stream);
}

void TestPerformanceEditor::init()
{
    m_model.reset();
    m_session = std::make_unique<services::DeviceSession>(std::make_unique<midi::LoopbackMidiTransport>());
    m_session->setAutomaticTimeoutPolling(false);
    m_model = std::make_unique<PerformanceViewModel>(*m_session);
}

xpmodel::Xp60Performance TestPerformanceEditor::fixturePerformance(int userNumber) const
{
    const auto decoded =
        xpmodel::Xp60PerformanceCodec::decode(m_image, *Xp60PerformanceLayout::userPerformanceAddress(userNumber));
    return *decoded.performance;
}

void TestPerformanceEditor::emptyUntilAPerformanceIsAdopted()
{
    QVERIFY(!m_model->hasPerformance());
    QVERIFY(m_model->parts().isEmpty());
    QVERIFY(m_model->name().isEmpty());
    QCOMPARE(m_model->activePartCount(), 0);
    QVERIFY(!m_model->modified());
    QVERIFY(!m_model->canUndo());
    // Nothing to send, and no instrument to send it to.
    QVERIFY(!m_model->canSend());
    QVERIFY(!m_model->sendToTemporary());
    QCOMPARE(m_model->partCount(), 16);
}

void TestPerformanceEditor::everyStripCarriesWhatTheMixerDraws()
{
    m_model->adopt(fixturePerformance(1), QStringLiteral("fixture"));
    QVERIFY(m_model->hasPerformance());
    QCOMPARE(m_model->name(), QStringLiteral("SIWAKAASI"));
    QCOMPARE(m_model->parts().size(), 16);
    QVERIFY(m_model->tempo() >= 20 && m_model->tempo() <= 250);
    QVERIFY(!m_model->keyboardMode().isEmpty());

    for (int n = 1; n <= 16; ++n) {
        const auto part = strip(*m_model, n);
        QCOMPARE(part.value("partNumber").toInt(), n);
        QCOMPARE(part.value("isRhythmPart").toBool(), n == 10);
        const int channel = part.value("midiChannel").toInt();
        QVERIFY(channel >= 1 && channel <= 16);
        QVERIFY(part.value("level").toInt() >= 0 && part.value("level").toInt() <= 127);
        QVERIFY(part.value("voiceReserve").toInt() >= 0 && part.value("voiceReserve").toInt() <= 64);
        QVERIFY(!part.value("panText").toString().isEmpty());
        QVERIFY(!part.value("outputAssignLabel").toString().isEmpty());
        QVERIFY(!part.value("keyLowerNote").toString().isEmpty());
        QVERIFY(part.value("keyLowerRaw").toInt() <= part.value("keyUpperRaw").toInt());
        QVERIFY(!part.value("patchGroupLabel").toString().isEmpty());
        // The Part names a Patch by number as the instrument displays it.
        QVERIFY(part.value("patchNumber").toInt() >= 1);
    }
}

void TestPerformanceEditor::editsAreUndoableAndRefuseValuesOutsideTheMap()
{
    m_model->adopt(fixturePerformance(1), QStringLiteral("fixture"));
    QSignalSpy spy(m_model.get(), &PerformanceViewModel::changed);
    const int before = strip(*m_model, 3).value("level").toInt();
    const int target = before == 100 ? 99 : 100;

    QVERIFY(m_model->setPartLevel(3, target));
    QCOMPARE(strip(*m_model, 3).value("level").toInt(), target);
    QVERIFY(m_model->modified());
    QVERIFY(m_model->canUndo());
    QVERIFY(m_model->undoLabel().contains(QStringLiteral("Part 3")));
    QCOMPARE(spy.count(), 1);

    // Setting the same value again is a success but not a step: undo must not
    // acquire an entry that changes nothing.
    QVERIFY(m_model->setPartLevel(3, target));
    QCOMPARE(spy.count(), 1);

    m_model->undo();
    QCOMPARE(strip(*m_model, 3).value("level").toInt(), before);
    QVERIFY(!m_model->modified());
    QVERIFY(m_model->canRedo());
    m_model->redo();
    QCOMPARE(strip(*m_model, 3).value("level").toInt(), target);

    // Out of range is refused, not clamped, and records no step.
    const int steps = spy.count();
    QVERIFY(!m_model->setPartLevel(3, 128));
    QVERIFY(!m_model->setPartVoiceReserve(3, 65));       // documented 0..64
    QVERIFY(!m_model->setPartOctaveShift(3, 4));         // documented -3..+3
    QVERIFY(!m_model->setPartMidiChannel(3, 17));
    QVERIFY(!m_model->setPartMidiChannel(3, 0));
    QCOMPARE(spy.count(), steps);
    QCOMPARE(strip(*m_model, 3).value("level").toInt(), target);

    // A Part number outside 1..16 is refused before anything is touched.
    QVERIFY(!m_model->setPartLevel(0, 10));
    QVERIFY(!m_model->setPartLevel(17, 10));

    // The boundary values themselves are legal.
    QVERIFY(m_model->setPartVoiceReserve(3, 64));
    QVERIFY(m_model->setPartOctaveShift(3, -3));
    QCOMPARE(strip(*m_model, 3).value("octaveShift").toInt(), -3);
    QVERIFY(m_model->setPartMidiChannel(3, 16));
    QCOMPARE(strip(*m_model, 3).value("midiChannel").toInt(), 16);
}

// Roland prints each keyboard bound in terms of the other, so a crossing pair
// is refused whole rather than half-applied.
void TestPerformanceEditor::aCrossingKeyRangeIsRefusedWhole()
{
    m_model->adopt(fixturePerformance(1), QStringLiteral("fixture"));
    QVERIFY(m_model->setPartKeyRange(1, 36, 96));
    QCOMPARE(strip(*m_model, 1).value("keyLowerRaw").toInt(), 36);
    QCOMPARE(strip(*m_model, 1).value("keyUpperRaw").toInt(), 96);

    QVERIFY(!m_model->setPartKeyRange(1, 97, 96));
    QCOMPARE(strip(*m_model, 1).value("keyLowerRaw").toInt(), 36);
    QCOMPARE(strip(*m_model, 1).value("keyUpperRaw").toInt(), 96);

    // Equal bounds are a single playable key, which the instrument allows.
    QVERIFY(m_model->setPartKeyRange(1, 60, 60));
    QCOMPARE(strip(*m_model, 1).value("keyLowerNote").toString(),
             strip(*m_model, 1).value("keyUpperNote").toString());
}

void TestPerformanceEditor::voiceReserveIsEditedThroughTheStripButLivesInCommon()
{
    m_model->adopt(fixturePerformance(1), QStringLiteral("fixture"));
    QVERIFY(m_model->setPartVoiceReserve(5, 12));
    QCOMPARE(strip(*m_model, 5).value("voiceReserve").toInt(), 12);
    // Written to Performance Common's fifth Voice Reserve, not to the Part.
    QCOMPARE(m_model->working().raw(PerformanceCommonParameter::VoiceReserve5), 12);
    QCOMPARE(strip(*m_model, 6).value("voiceReserve").toInt(),
             m_model->working().raw(PerformanceCommonParameter::VoiceReserve6));
}

void TestPerformanceEditor::revertGoesBackToWhatWasFetched()
{
    const auto original = fixturePerformance(2);
    m_model->adopt(original, QStringLiteral("fixture"));
    QVERIFY(m_model->setPartLevel(1, 7));
    QVERIFY(m_model->setPartReceives(2, !strip(*m_model, 2).value("receives").toBool()));
    QVERIFY(m_model->setName(QStringLiteral("Changed")));
    QVERIFY(m_model->modified());

    m_model->revert();
    QVERIFY(!m_model->modified());
    QVERIFY(m_model->working() == original);
    // Revert is itself undoable: it is an edit, not an escape hatch.
    QVERIFY(m_model->canUndo());
    m_model->undo();
    QVERIFY(m_model->modified());

    // A name the instrument cannot hold is refused.
    QVERIFY(!m_model->setName(QStringLiteral("This name is far too long for the XP-60")));
}

void TestPerformanceEditor::sendingNeedsAConnectedInstrument()
{
    m_model->adopt(fixturePerformance(1), QStringLiteral("fixture"));
    // Not connected: no fetch, no send, and nothing silently queued.
    QVERIFY(!m_model->canFetch());
    QVERIFY(!m_model->canSend());
    QVERIFY(!m_model->fetchTemporary());
    QVERIFY(!m_model->fetchUser(1));
    QVERIFY(!m_model->sendToTemporary());
    QCOMPARE(m_session->pendingDataSetBatches(), std::size_t{0});

    // USER numbers outside 1..32 are refused whatever the connection state.
    QVERIFY(!m_model->fetchUser(0));
    QVERIFY(!m_model->fetchUser(33));
}

namespace {

// A connected instrument, for the one thing that cannot be tested offline.
// The write path itself is covered against the real device model in
// tst_snapshot_restore_run; what matters here is that the view model drives it
// and reports back what QML needs.
struct Connected
{
    protocol::TimePoint now{std::chrono::duration_cast<protocol::Clock::duration>(1000ms)};
    midi::LoopbackMidiTransport* transport = nullptr;
    std::unique_ptr<services::DeviceSession> session;
    std::unique_ptr<PerformanceViewModel> model;
    std::unique_ptr<testsupport::FakeXp60> device;

    Connected()
    {
        auto loopback = std::make_unique<midi::LoopbackMidiTransport>();
        loopback->addInput("in-1", "XP-60 IN");
        loopback->addOutput("out-1", "XP-60 OUT");
        transport = loopback.get();
        session = std::make_unique<services::DeviceSession>(std::move(loopback));
        session->setAutomaticTimeoutPolling(false);
        session->setClocks([this] { return now; }, {});
        auto pacing = session->pacing();
        pacing.interMessageDelay = 0ms;
        session->setPacing(pacing);
        model = std::make_unique<PerformanceViewModel>(*session);
        device = std::make_unique<testsupport::FakeXp60>(testsupport::fixtureImage());
        session->connectEndpoints("in-1", "out-1");
    }

    void pump(int rounds = 60000)
    {
        for (int i = 0; i < rounds; ++i) {
            QCoreApplication::processEvents();
            const auto replies = device->exchange(*transport);
            for (const auto& reply : replies) {
                const auto bytes = reply.encode();
                transport->injectIncoming(midi::MidiByteSpan(bytes.data(), bytes.size()));
            }
            QCoreApplication::processEvents();
            if (replies.empty() && transport->sentMessages().empty()
                && session->pendingSendCount() == 0 && !model->userWriteBusy()) {
                return;
            }
        }
    }

    [[nodiscard]] xpmodel::Xp60Performance inUserSlot(int userNumber) const
    {
        return *xpmodel::Xp60PerformanceCodec::decode(
                    device->memory(), *Xp60PerformanceLayout::userPerformanceAddress(userNumber))
                    .performance;
    }
};

} // namespace

void TestPerformanceEditor::aPersistentWriteIsRefusedUntilThereIsAnInstrumentAndAnArm()
{
    // Nothing adopted: nothing to write, nothing to arm for.
    QVERIFY(!m_model->canArmUserWrite());
    QVERIFY(!m_model->armUserWrite());
    QVERIFY(!m_model->writeToUserSlot(1));

    m_model->adopt(fixturePerformance(1), QStringLiteral("fixture"));
    // Adopted but offline. A persistent write replaces something the musician
    // stored, so it is refused rather than queued for later.
    QVERIFY(!m_model->canArmUserWrite());
    QVERIFY(!m_model->armUserWrite());
    QVERIFY(!m_model->userWriteArmed());
    QVERIFY(!m_model->writeToUserSlot(1));
    QCOMPARE(m_model->userWriteTotal(), 0);
    QVERIFY(!m_model->canRestoreUserWrite());
    QVERIFY(!m_model->canRetryUserWrite());
    QCOMPARE(m_model->userPerformanceCount(), 32);
}

void TestPerformanceEditor::theWritePlanNamesTheDestinationBeforeAnythingIsArmed()
{
    QVERIFY(m_model->userWritePlan(1).isEmpty()); // nothing adopted
    m_model->adopt(fixturePerformance(1), QStringLiteral("fixture"));

    const auto plan = m_model->userWritePlan(7);
    QVERIFY(!plan.isEmpty());
    QVERIFY(plan.contains(QStringLiteral("7")));

    // Slots the instrument does not have are refused rather than clamped into
    // one it does.
    QVERIFY(m_model->userWritePlan(0).isEmpty());
    QVERIFY(m_model->userWritePlan(33).isEmpty());
    QVERIFY(!m_model->writeToUserSlot(0));
    QVERIFY(!m_model->writeToUserSlot(33));
}

void TestPerformanceEditor::writingToAUserSlotVerifiesAndTheSnapshotPutsItBack()
{
    Connected c;
    const auto before = c.inUserSlot(9);
    c.model->adopt(c.inUserSlot(1), QStringLiteral("fixture"));
    QVERIFY(c.model->name() != QString::fromStdString(before.name().displayText()));

    QVERIFY(c.model->canArmUserWrite());
    QVERIFY(c.model->armUserWrite());
    QVERIFY(c.model->userWriteArmed());

    QSignalSpy finished(c.model.get(), &PerformanceViewModel::userWriteFinished);
    QVERIFY(c.model->writeToUserSlot(9));
    c.pump();

    QCOMPARE(finished.size(), 1);
    QCOMPARE(finished.at(0).at(0).toBool(), true);
    QCOMPARE(c.model->userWriteState(), QStringLiteral("Completed"));
    QCOMPARE(c.model->userWriteCompleted(), 1);
    QCOMPARE(c.model->userWriteTotal(), 1);
    QVERIFY(c.model->userWriteUnwritten().isEmpty());
    QCOMPARE(c.inUserSlot(9).name().displayText(), c.inUserSlot(1).name().displayText());

    // The snapshot taken before the write is the undo, and it is a real one:
    // the slot goes back to the bytes it held.
    QVERIFY(c.model->canRestoreUserWrite());
    QVERIFY(c.model->restoreUserWrite());
    c.pump();
    QVERIFY(c.inUserSlot(9) == before);
}

QTEST_MAIN(TestPerformanceEditor)
#include "tst_performance_editor.moc"
