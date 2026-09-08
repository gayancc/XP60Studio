// Phase 11 — driving a setlist from a foot switch.
//
// The hazard this is written around: the application listens to whatever is
// plugged into its MIDI input, which on stage is the XP-60's own MIDI OUT, and
// the XP-60 transmits Program Change whenever a Patch is chosen on its front
// panel. So the tests pin that nothing fires until it is switched on and bound,
// that a Program Change binding says out loud what it has just been aimed at,
// and that a release cannot be mistaken for a second press.

#include "services/LiveMidiNavigation.h"
#include "support/FakeXp60.h"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QtTest>

#include <chrono>
#include <memory>

using namespace xp60studio;
using namespace std::chrono_literals;
using library::LiveTarget;
using library::LiveTargetKind;
using library::Setlist;
using library::SetlistSection;
using library::SetlistSong;
using services::LiveAction;
using services::LiveMidiNavigation;
using services::LiveSession;
using services::LiveTriggerBinding;
using services::LiveTriggerKind;
using testsupport::FakeXp60;

namespace {

LiveTarget userPatch(int number)
{
    return LiveTarget{LiveTargetKind::UserPatchSlot, 0, number, {}};
}

Setlist fourCues()
{
    Setlist list;
    list.name = "Friday";
    list.songs = {SetlistSong{"Opener",
                              {},
                              {SetlistSection{"A", userPatch(1), {}},
                               SetlistSection{"B", userPatch(9), {}},
                               SetlistSection{"C", userPatch(17), {}},
                               SetlistSection{"D", userPatch(25), {}}}}};
    return list;
}

struct Fixture
{
    protocol::TimePoint now{std::chrono::duration_cast<protocol::Clock::duration>(1000ms)};
    midi::LoopbackMidiTransport* transport = nullptr;
    std::unique_ptr<services::DeviceSession> session;
    std::unique_ptr<services::PatchTransfer> transfer;
    std::unique_ptr<LiveSession> live;
    std::unique_ptr<LiveMidiNavigation> nav;

    Fixture()
    {
        auto loopback = std::make_unique<midi::LoopbackMidiTransport>();
        loopback->addInput("in-1", "XP-60 IN");
        loopback->addOutput("out-1", "XP-60 OUT");
        transport = loopback.get();
        session = std::make_unique<services::DeviceSession>(std::move(loopback));
        session->setAutomaticTimeoutPolling(false);
        session->setClocks([this] { return now; }, {});
        transfer = std::make_unique<services::PatchTransfer>(*session);
        live = std::make_unique<LiveSession>(*session, *transfer);
        nav = std::make_unique<LiveMidiNavigation>(*session, *live);
        session->connectEndpoints("in-1", "out-1");
        live->load(fourCues());
    }

    // Puts a channel voice message on the input, the way a pedal or the
    // instrument's own panel would.
    void receive(std::initializer_list<midi::Byte> bytes)
    {
        const midi::MidiBytes message(bytes);
        transport->injectIncoming(midi::MidiByteSpan(message.data(), message.size()));
        QCoreApplication::processEvents();
    }
};

} // namespace

class TestLiveMidiNavigation : public QObject
{
    Q_OBJECT

private slots:
    void nothingHappensUntilItIsSwitchedOnAndBound()
    {
        Fixture f;
        QVERIFY(!f.nav->enabled());
        QVERIFY(f.nav->bindings().empty());

        // The XP-60 sends this whenever a Patch is chosen on its panel. Out of
        // the box it must not move somebody's setlist.
        f.receive({0xC0, 0x05});
        f.receive({0xB0, 0x40, 0x7F});
        QCOMPARE(f.nav->firedCount(), 0);
        QCOMPARE(f.live->position(), -1);

        // Bound but still off.
        QVERIFY(f.nav->addBinding({LiveTriggerKind::ControlChange, 0, 0x40, 64, LiveAction::Next, false}));
        f.receive({0xB0, 0x40, 0x7F});
        QCOMPARE(f.nav->firedCount(), 0);
        QCOMPARE(f.live->position(), -1);
    }

    void aFootSwitchAdvancesOnThePressAndNotOnTheRelease()
    {
        Fixture f;
        f.nav->setEnabled(true);
        QVERIFY(f.nav->addBinding({LiveTriggerKind::ControlChange, 0, 0x40, 64, LiveAction::Next, false}));

        QSignalSpy fired(f.nav.get(), &LiveMidiNavigation::triggered);
        f.receive({0xB0, 0x40, 0x7F});  // press
        QCOMPARE(f.live->position(), 0);
        f.receive({0xB0, 0x40, 0x00});  // release
        // A release that advanced would make every stamp of the pedal skip a
        // section.
        QCOMPARE(f.live->position(), 0);
        f.receive({0xB0, 0x40, 0x7F});
        QCOMPARE(f.live->position(), 1);

        QCOMPARE(fired.size(), 2);
        QCOMPARE(f.nav->firedCount(), 2);
    }

    void aBindingOnlyListensToItsOwnChannelAndController()
    {
        Fixture f;
        f.nav->setEnabled(true);
        QVERIFY(f.nav->addBinding({LiveTriggerKind::ControlChange, 3, 0x40, 64, LiveAction::Next, false}));

        f.receive({0xB0, 0x40, 0x7F});  // channel 1
        f.receive({0xB2, 0x41, 0x7F});  // channel 3, wrong controller
        QCOMPARE(f.live->position(), -1);
        f.receive({0xB2, 0x40, 0x7F});  // channel 3, right controller
        QCOMPARE(f.live->position(), 0);
    }

    void anyChannelIsSpeltZeroRatherThanGuessed()
    {
        Fixture f;
        f.nav->setEnabled(true);
        QVERIFY(f.nav->addBinding({LiveTriggerKind::ControlChange, 0, 0x40, 64, LiveAction::Next, false}));
        f.receive({0xB7, 0x40, 0x7F});
        QCOMPARE(f.live->position(), 0);
    }

    void aProgramChangeCanSelectACueDirectlyAndCountsFromTheWire()
    {
        Fixture f;
        f.nav->setEnabled(true);
        QVERIFY(f.nav->addBinding(
            {LiveTriggerKind::ProgramChange, 0, 0, 64, LiveAction::GoToNumbered, false}));

        // A pedal displaying "1" sends 0. That must select the first cue, not
        // the second.
        f.receive({0xC0, 0x00});
        QCOMPARE(f.live->position(), 0);
        f.receive({0xC0, 0x02});
        QCOMPARE(f.live->position(), 2);
        // Past the end of the list: refused, and the cursor stays put.
        QSignalSpy fired(f.nav.get(), &LiveMidiNavigation::triggered);
        f.receive({0xC0, 0x40});
        QCOMPARE(f.live->position(), 2);
        QCOMPARE(fired.size(), 1);
        QCOMPARE(fired.at(0).at(1).toBool(), false);
    }

    void aNoteOnWithZeroVelocityIsTheReleaseNotAPressOfNoForce()
    {
        Fixture f;
        f.nav->setEnabled(true);
        QVERIFY(f.nav->addBinding({LiveTriggerKind::Note, 0, 36, 1, LiveAction::Next, false}));
        f.receive({0x90, 36, 0x00});  // Note On, velocity 0 — that is a Note Off
        QCOMPARE(f.live->position(), -1);
        f.receive({0x90, 36, 0x40});
        QCOMPARE(f.live->position(), 0);
    }

    void messagesNothingIsBoundToAreLeftAlone()
    {
        Fixture f;
        f.nav->setEnabled(true);
        QVERIFY(f.nav->addBinding({LiveTriggerKind::ControlChange, 0, 0x40, 64, LiveAction::Next, false}));
        f.receive({0x80, 36, 0x40});        // note off
        f.receive({0xE0, 0x00, 0x40});      // pitch bend
        f.receive({0xD0, 0x40});            // channel pressure
        f.receive({0xB0, 0x07, 0x7F});      // a volume pedal
        QCOMPARE(f.live->position(), -1);
        QCOMPARE(f.nav->firedCount(), 0);
        // Not "ignored" either: nothing was bound to them, so there is nothing
        // to report.
        QCOMPARE(f.nav->ignoredCount(), 0);
    }

    void bindingsThatCouldNotWorkAreRefusedWithAReason()
    {
        Fixture f;
        // A threshold of zero fires on the release too.
        QVERIFY(!f.nav->addBinding({LiveTriggerKind::ControlChange, 0, 0x40, 0, LiveAction::Next, false}));
        QVERIFY(LiveMidiNavigation::whyNotUsable(
                    {LiveTriggerKind::ControlChange, 0, 0x40, 0, LiveAction::Next, false})
                    .contains(QStringLiteral("press")));

        QVERIFY(!f.nav->addBinding({LiveTriggerKind::Note, 17, 36, 64, LiveAction::Next, false}));
        QVERIFY(!f.nav->addBinding({LiveTriggerKind::Note, 0, 200, 64, LiveAction::Next, false}));

        // A control change carries a level, not a cue number.
        QVERIFY(!f.nav->addBinding(
            {LiveTriggerKind::ControlChange, 0, 0x40, 64, LiveAction::GoToNumbered, false}));
        QVERIFY(LiveMidiNavigation::whyNotUsable({LiveTriggerKind::ControlChange, 0, 0x40, 64,
                                                  LiveAction::GoToNumbered, false})
                    .contains(QStringLiteral("level")));
        QVERIFY(f.nav->bindings().empty());
    }

    void aProgramChangeBindingWarnsThatTheKeyboardSendsThatToo()
    {
        Fixture f;
        QVERIFY(f.nav->addBinding(
            {LiveTriggerKind::ProgramChange, 0, 0, 64, LiveAction::GoToNumbered, false}));
        QVERIFY(f.nav->addBinding({LiveTriggerKind::ControlChange, 2, 0x40, 64, LiveAction::Next, false}));

        const auto lines = f.nav->describeBindings();
        QCOMPARE(lines.size(), 2);
        // The musician has just aimed their setlist at the message their own
        // front panel emits. That has to be said, not discovered on stage.
        QVERIFY(lines.at(0).contains(QStringLiteral("front panel")));
        QVERIFY(lines.at(1).contains(QStringLiteral("channel 2")));
        QVERIFY(!lines.at(1).contains(QStringLiteral("front panel")));
    }

    void restartMovesTheCursorBackWithoutTransmitting()
    {
        Fixture f;
        f.nav->setEnabled(true);
        QVERIFY(f.nav->addBinding({LiveTriggerKind::ControlChange, 0, 0x40, 64, LiveAction::Next, false}));
        QVERIFY(f.nav->addBinding({LiveTriggerKind::ControlChange, 0, 0x41, 64, LiveAction::Restart, false}));
        f.receive({0xB0, 0x40, 0x7F});
        f.receive({0xB0, 0x40, 0x7F});
        QCOMPARE(f.live->position(), 1);

        const auto sentBefore = f.transport->sentMessages().size();
        f.receive({0xB0, 0x41, 0x7F});
        QCOMPARE(f.live->position(), -1);
        QCOMPARE(f.transport->sentMessages().size(), sentBefore);
    }

    void theCursorStopsAtTheEndsAndSaysTheActionDidNotTake()
    {
        Fixture f;
        f.nav->setEnabled(true);
        QVERIFY(f.nav->addBinding({LiveTriggerKind::ControlChange, 0, 0x40, 64, LiveAction::Previous, false}));
        QSignalSpy fired(f.nav.get(), &LiveMidiNavigation::triggered);
        f.receive({0xB0, 0x40, 0x7F});
        QCOMPARE(f.live->position(), -1);
        QCOMPARE(fired.size(), 1);
        QCOMPARE(fired.at(0).at(1).toBool(), false);
        // It fired — the binding matched and did what it could. Reporting the
        // outcome is not the same as pretending nothing arrived.
        QCOMPARE(f.nav->firedCount(), 1);
    }
};

QTEST_MAIN(TestLiveMidiNavigation)
#include "tst_live_midi_navigation.moc"
