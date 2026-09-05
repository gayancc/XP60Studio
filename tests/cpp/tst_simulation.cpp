#include "support/FakeXp60.h"

#include "midi/LoopbackMidiTransport.h"
#include "services/DeviceSession.h"
#include "simulation/DemoBootstrap.h"
#include "simulation/DemoFixture.h"
#include "simulation/DemoReplyPump.h"
#include "simulation/SimulatedXp60.h"
#include "xp60/Xp60Device.h"
#include "xpmodel/Xp60PatchLayout.h"

#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QTest>

#include <memory>

using namespace xp60studio;
using namespace xp60studio::simulation;

class TstSimulation : public QObject
{
    Q_OBJECT

private slots:
    void embeddedFixtureLoadsTemporaryArea();
    void replyPumpAnswersTemporaryNameRequest();
    void fakeXp60WrapsSimulatedXp60();
};

void TstSimulation::embeddedFixtureLoadsTemporaryArea()
{
    const auto image = temporaryAreaFromFixture(4);
    QVERIFY(!image.isEmpty());
    const auto name = xpmodel::Xp60PatchLayout::readTemporaryPatchName(image);
    QVERIFY(name.has_value());
    QVERIFY(!name->isBlank());
}

void TstSimulation::replyPumpAnswersTemporaryNameRequest()
{
    auto bundle = createDemoTransport();
    auto* transport = bundle.raw;
    QVERIFY(transport != nullptr);

    services::DeviceSession session(std::move(bundle.transport));
    session.setAutomaticTimeoutPolling(false);
    auto pacing = session.pacing();
    pacing.interMessageDelay = std::chrono::milliseconds(0);
    session.setPacing(pacing);

    SimulatedXp60 device(temporaryAreaFromFixture(4));
    DemoReplyPump pump(device, *transport);
    pump.start(1);

    QVERIFY(session.connectEndpoints(kDemoInputId, kDemoOutputId));
    QVERIFY(session.fetchTemporaryPatch());

    const auto deadline = QDeadlineTimer(std::chrono::seconds(2));
    while (!deadline.hasExpired()) {
        QCoreApplication::processEvents();
        if (session.patchFetch().state == services::DeviceSession::PatchFetchState::Completed) {
            break;
        }
    }

    QCOMPARE(session.patchFetch().state, services::DeviceSession::PatchFetchState::Completed);
    QVERIFY(session.patchFetch().patch.has_value());
    pump.stop();
}

void TstSimulation::fakeXp60WrapsSimulatedXp60()
{
    auto loopback = std::make_unique<midi::LoopbackMidiTransport>();
    loopback->addInput("in-1", "IN");
    loopback->addOutput("out-1", "OUT");
    auto* transport = loopback.get();

    services::DeviceSession session(std::move(loopback));
    session.setAutomaticTimeoutPolling(false);
    auto pacing = session.pacing();
    pacing.interMessageDelay = std::chrono::milliseconds(0);
    session.setPacing(pacing);

    testsupport::FakeXp60 fake(testsupport::temporaryAreaWith(4));
    QVERIFY(session.connectEndpoints("in-1", "out-1"));
    QVERIFY(session.fetchTemporaryPatch());

    for (int i = 0; i < 80; ++i) {
        QCoreApplication::processEvents();
        const auto replies = fake.exchange(*transport);
        for (const auto& reply : replies) {
            const auto bytes = reply.encode();
            transport->injectIncoming(midi::MidiByteSpan(bytes.data(), bytes.size()));
        }
        QCoreApplication::processEvents();
        if (session.patchFetch().state == services::DeviceSession::PatchFetchState::Completed) {
            break;
        }
    }

    QCOMPARE(session.patchFetch().state, services::DeviceSession::PatchFetchState::Completed);
}

QTEST_MAIN(TstSimulation)
#include "tst_simulation.moc"
