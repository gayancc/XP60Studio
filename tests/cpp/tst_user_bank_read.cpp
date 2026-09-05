// Reading the XP-60's permanent USER bank back into XP60Studio.
//
// This is the backup that makes writing safe to offer, and it answers two of the
// questions AGENTS.md sets as the product standard: "did all 128 patches
// actually reach the keyboard?" and "can I restore exactly what was on the XP-60
// before this change?".
//
// The simulator holds the real 128-Patch fixture at its real `11 nn 00 00`
// addresses, so a read here reads the same bytes it would on the instrument.
//
// What the tests hold to:
//
//   * the whole bank comes back, every Patch equal to what the instrument holds;
//   * the bytes the instrument sent are preserved, not re-encoded, so a stored
//     backup is what actually arrived;
//   * a run that is stopped or that fails still hands back what it read, because
//     a partial backup is worth having;
//   * reading is RQ1 only and changes nothing on the instrument.

#include "services/DeviceSession.h"
#include "services/UserBankRead.h"
#include "support/FakeXp60.h"
#include "xpmodel/Xp60PatchCodec.h"
#include "xpmodel/Xp60PatchLayout.h"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QtTest>

#include <chrono>
#include <memory>

using namespace xp60studio;
using namespace std::chrono_literals;
using services::UserBankRead;
using testsupport::FakeXp60;
using xpmodel::Xp60Patch;
using xpmodel::Xp60PatchCodec;
using xpmodel::Xp60PatchLayout;
using State = services::UserBankRead::State;

namespace {

Xp60Patch patchAt(const xpmodel::MemoryImage& image, int userNumber)
{
    const auto decoded = Xp60PatchCodec::decode(image, *Xp60PatchLayout::userPatchAddress(userNumber));
    return *decoded.patch;
}

struct Fixture
{
    midi::LoopbackMidiTransport* transport = nullptr;
    std::unique_ptr<services::DeviceSession> session;
    std::unique_ptr<UserBankRead> reader;
    std::unique_ptr<FakeXp60> device;

    Fixture()
    {
        auto loopback = std::make_unique<midi::LoopbackMidiTransport>();
        loopback->addInput("in-1", "XP-60 IN");
        loopback->addOutput("out-1", "XP-60 OUT");
        transport = loopback.get();
        session = std::make_unique<services::DeviceSession>(std::move(loopback));
        session->setAutomaticTimeoutPolling(false);
        auto pacing = session->pacing();
        pacing.interMessageDelay = 0ms;
        session->setPacing(pacing);
        reader = std::make_unique<UserBankRead>(*session);
        device = std::make_unique<FakeXp60>(testsupport::fixtureImage());
        session->connectEndpoints("in-1", "out-1");
    }

    void pump(int rounds = 20000)
    {
        for (int i = 0; i < rounds; ++i) {
            QCoreApplication::processEvents();
            const auto replies = device->exchange(*transport);
            for (const auto& reply : replies) {
                const auto bytes = reply.encode();
                transport->injectIncoming(midi::MidiByteSpan(bytes.data(), bytes.size()));
            }
            QCoreApplication::processEvents();
            if (replies.empty() && transport->sentMessages().empty() && session->pendingSendCount() == 0
                && !reader->isBusy()) {
                return;
            }
        }
    }
};

} // namespace

class TestUserBankRead : public QObject
{
    Q_OBJECT

private slots:
    void readsTheWholeUserBank();
    void preservesTheBytesTheInstrumentSent();
    void aStoppedRunKeepsWhatItAlreadyRead();
    void refusesAnImpossibleRequestWithoutTouchingTheInstrument();
};

void TestUserBankRead::readsTheWholeUserBank()
{
    Fixture f;
    QSignalSpy finished(f.reader.get(), &UserBankRead::finished);

    QVERIFY(f.reader->readWholeBank());
    QCOMPARE(f.reader->total(), std::size_t{128});
    f.pump();

    QCOMPARE(f.reader->state(), State::Completed);
    QCOMPARE(f.reader->completed(), std::size_t{128});
    QCOMPARE(finished.count(), 1);
    QVERIFY(finished.at(0).at(0).toBool());

    // Every Patch equals what the instrument holds at that slot, and each one
    // knows which slot it came from.
    const auto& patches = f.reader->readPatches();
    QCOMPARE(patches.size(), std::size_t{128});
    for (std::size_t i = 0; i < patches.size(); ++i) {
        QCOMPARE(patches[i].userNumber, static_cast<int>(i) + 1);
        QVERIFY2(patches[i].patch == patchAt(f.device->memory(), patches[i].userNumber),
                 qPrintable(QStringLiteral("USER:%1 differs").arg(patches[i].userNumber)));
    }
}

// A backup made of re-encoded bytes is not a backup of what the instrument had.
// Roland pads its blocks and the XP-60 sends a 129-byte Tone block in one
// message where the encoder would split it at 128, so "what arrived" and "what
// we would send" are genuinely different byte streams.
void TestUserBankRead::preservesTheBytesTheInstrumentSent()
{
    Fixture f;
    QVERIFY(f.reader->read({1, 2, 3}));
    f.pump();
    QCOMPARE(f.reader->state(), State::Completed);

    for (const auto& slot : f.reader->readPatches()) {
        QVERIFY2(!slot.originalSysEx.empty(), "the bytes the instrument sent are kept");
        // The message count is deliberately not asserted. A real XP-60 sends a
        // whole 129-byte Tone block in one DT1 (hardware-verified,
        // ROLAND_XP60_PROTOCOL_FACTS.md §2.1) where the simulator re-encodes and
        // splits at the documented 128-byte limit. Pinning a count here would
        // assert the simulator's shape rather than the requirement, which is
        // that whatever arrived is what is kept.
        std::size_t messages = 0;
        for (const auto byte : slot.originalSysEx) {
            if (byte == roland::kSysExStart) {
                ++messages;
            }
        }
        QVERIFY(messages >= 5);
        // What was kept re-parses to the Patch that was decoded from it.
        const std::vector<roland::RolandModelId> models{xp60::modelId()};
        const auto image = xpmodel::imageFromStream(xpmodel::parseSysExStream(slot.originalSysEx, models));
        const auto decoded = Xp60PatchCodec::decode(image, *Xp60PatchLayout::userPatchAddress(slot.userNumber));
        QVERIFY(decoded.ok());
        QVERIFY(*decoded.patch == slot.patch);
    }
}

void TestUserBankRead::aStoppedRunKeepsWhatItAlreadyRead()
{
    Fixture f;
    QVERIFY(f.reader->read({10, 11, 12, 13, 14}));

    // Let a couple of Patches land, then stop.
    for (int i = 0; i < 4000 && f.reader->completed() < 2; ++i) {
        QCoreApplication::processEvents();
        const auto replies = f.device->exchange(*f.transport);
        for (const auto& reply : replies) {
            const auto bytes = reply.encode();
            f.transport->injectIncoming(midi::MidiByteSpan(bytes.data(), bytes.size()));
        }
        QCoreApplication::processEvents();
    }
    QVERIFY(f.reader->completed() >= 2);
    f.reader->cancel();
    f.pump();

    QCOMPARE(f.reader->state(), State::Cancelled);
    // A partial backup is still worth having, so nothing read is thrown away.
    QVERIFY(!f.reader->readPatches().empty());
    QVERIFY(f.reader->readPatches().size() < 5);
    QVERIFY(f.reader->message().contains(QStringLiteral("kept")));
    for (const auto& slot : f.reader->readPatches()) {
        QVERIFY(slot.patch == patchAt(f.device->memory(), slot.userNumber));
    }
}

void TestUserBankRead::refusesAnImpossibleRequestWithoutTouchingTheInstrument()
{
    Fixture f;
    const auto before = f.device->dataSetsReceived();

    QVERIFY(!f.reader->read({}));
    QVERIFY(!f.reader->read({1, 129}));
    QCOMPARE(f.reader->state(), State::Failed);
    QVERIFY(f.reader->message().contains(QStringLiteral("128-slot")));
    f.pump();

    // Reading is RQ1 only, and a refused read sends nothing at all: the
    // instrument received no data sets either way.
    QCOMPARE(f.device->dataSetsReceived(), before);
    QCOMPARE(f.reader->completed(), std::size_t{0});
}

QTEST_MAIN(TestUserBankRead)
#include "tst_user_bank_read.moc"
