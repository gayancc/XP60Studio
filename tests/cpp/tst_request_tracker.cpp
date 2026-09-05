#include "protocol/RolandRequestTracker.h"
#include "protocol/TransferPacing.h"
#include "roland/RolandSysExMessage.h"
#include "xp60/Xp60Device.h"

#include <QtTest>

#include <chrono>

using namespace xp60studio;
using namespace xp60studio::protocol;
using namespace xp60studio::roland;
using namespace std::chrono_literals;

namespace {

const RolandDeviceId kDevice = RolandDeviceId::factoryDefault();
const RolandModelId kModel = xp60::modelId();

TimePoint at(std::chrono::milliseconds ms)
{
    return TimePoint(std::chrono::duration_cast<Clock::duration>(ms));
}

RolandSysExMessage rq1(RolandAddress address, std::uint32_t size)
{
    return RolandSysExMessage::dataRequest(kDevice, kModel, address, RolandSize::fromValue(size).value());
}

RolandSysExMessage dt1(RolandAddress address, std::size_t count, Byte fill = 0x21)
{
    return RolandSysExMessage::dataSet(kDevice, kModel, address, ByteVector(count, fill)).value();
}

RequestTimeouts timeouts()
{
    return RequestTimeouts{1500ms, 1000ms};
}

} // namespace

class RequestTrackerTest : public QObject
{
    Q_OBJECT

private slots:
    void lifecycleSingleChunk()
    {
        RolandRequestTracker tracker(timeouts());
        const RolandAddress address(0x03, 0, 0, 0);
        const auto id = tracker.enqueue(rq1(address, 12), at(0ms));
        QVERIFY(id.isValid());
        QCOMPARE(tracker.find(id)->state, RequestState::RequestSent);
        QVERIFY(tracker.hasOutstanding());
        QVERIFY(!tracker.nextDeadline().has_value()); // not sent yet: no deadline

        QVERIFY(tracker.markSent(id, at(10ms)));
        QCOMPARE(tracker.find(id)->state, RequestState::AwaitingData);
        QCOMPARE(*tracker.nextDeadline(), at(1510ms));

        const auto match = tracker.onDataSet(dt1(address, 12), at(50ms));
        QCOMPARE(match.outcome, RolandRequestTracker::MatchOutcome::Completed);
        QCOMPARE(match.requestId.value(), id);
        const auto* op = tracker.find(id);
        QCOMPARE(op->state, RequestState::Completed);
        QCOMPARE(op->receivedBytes, 12u);
        QCOMPARE(op->chunkCount, 1u);
        QCOMPARE(op->data.size(), std::size_t(12));
        QCOMPARE(op->data[0], Byte(0x21));
        QVERIFY(!tracker.hasOutstanding());
        QVERIFY(isTerminal(op->state));
    }

    void multiChunkResponseInOrder()
    {
        RolandRequestTracker tracker(timeouts());
        const RolandAddress address(0x11, 0, 0, 0);
        // XP-60 packets carry at most 128 data bytes: 300 = 128 + 128 + 44.
        const auto id = tracker.enqueue(rq1(address, 300), at(0ms));
        tracker.markSent(id, at(0ms));

        auto m1 = tracker.onDataSet(dt1(address, 128, 0x01), at(20ms));
        QCOMPARE(m1.outcome, RolandRequestTracker::MatchOutcome::Accepted);
        QCOMPARE(tracker.find(id)->state, RequestState::Receiving);
        QCOMPARE(tracker.find(id)->receivedBytes, 128u);
        // Between-chunk timeout now applies from last activity.
        QCOMPARE(*tracker.nextDeadline(), at(1020ms));

        auto m2 = tracker.onDataSet(dt1(*address.plus(128), 128, 0x02), at(40ms));
        QCOMPARE(m2.outcome, RolandRequestTracker::MatchOutcome::Accepted);
        QCOMPARE(tracker.find(id)->receivedBytes, 256u);

        auto m3 = tracker.onDataSet(dt1(*address.plus(256), 44, 0x03), at(60ms));
        QCOMPARE(m3.outcome, RolandRequestTracker::MatchOutcome::Completed);
        const auto* op = tracker.find(id);
        QCOMPARE(op->state, RequestState::Completed);
        QCOMPARE(op->chunkCount, 3u);
        QCOMPARE(op->data[0], Byte(0x01));
        QCOMPARE(op->data[128], Byte(0x02));
        QCOMPARE(op->data[299], Byte(0x03));
        QVERIFY(op->notes.empty());
    }

    void outOfOrderChunksStillComplete()
    {
        RolandRequestTracker tracker(timeouts());
        const RolandAddress address(0x11, 0, 0, 0);
        const auto id = tracker.enqueue(rq1(address, 20), at(0ms));
        tracker.markSent(id, at(0ms));
        QCOMPARE(tracker.onDataSet(dt1(*address.plus(10), 10, 0x02), at(1ms)).outcome, RolandRequestTracker::MatchOutcome::Accepted);
        QCOMPARE(tracker.onDataSet(dt1(address, 10, 0x01), at(2ms)).outcome, RolandRequestTracker::MatchOutcome::Completed);
        const auto* op = tracker.find(id);
        QCOMPARE(op->state, RequestState::Completed);
        QCOMPARE(op->notes.size(), std::size_t(1)); // out-of-order observation recorded
        QCOMPARE(op->data[0], Byte(0x01));
        QCOMPARE(op->data[10], Byte(0x02));
    }

    // Recorded from a physical XP-60 on 2026-09-04 (see docs/HARDWARE_VALIDATION_XP60.md).
    // Roland's own published RQ1 example asks for a 3993-byte span at 01 00 00 00
    // and is answered with 466 payload bytes: Performance Common, then the 16
    // Parts at 01 00 10 00 .. 01 00 1F 00. Roland block addresses are padded, so
    // the gaps between blocks hold no data and never arrive.
    void paddedMultiBlockResponseCompletes()
    {
        RolandRequestTracker tracker(timeouts());
        const RolandAddress address(0x01, 0, 0, 0);
        const auto id = tracker.enqueue(rq1(address, 3993), at(0ms));
        tracker.markSent(id, at(0ms));

        QCOMPARE(tracker.onDataSet(dt1(address, 66, 0x01), at(32ms)).outcome,
            RolandRequestTracker::MatchOutcome::Accepted);

        // 16 Performance Parts, 25 bytes each, 128 address units apart.
        for (int part = 0; part < 16; ++part) {
            const auto partAddress = *address.plus(2048 + static_cast<std::uint64_t>(part) * 128);
            const auto outcome = tracker.onDataSet(dt1(partAddress, 25, 0x02), at(69ms + part * 37ms)).outcome;
            const auto expected = part == 15 ? RolandRequestTracker::MatchOutcome::Completed
                                             : RolandRequestTracker::MatchOutcome::Accepted;
            QCOMPARE(outcome, expected);
        }

        const auto* op = tracker.find(id);
        QCOMPARE(op->state, RequestState::Completed);
        QCOMPARE(op->chunkCount, 17u);
        // Payload actually received, not the requested address span.
        QCOMPARE(op->receivedBytes, 466u);
        QCOMPARE(op->expectedBytes, 3993u);
        // Padding gaps between blocks are normal Roland addressing, not anomalies.
        QVERIFY(op->notes.empty());
        QCOMPARE(op->data[0], Byte(0x01));
        QCOMPARE(op->data[2048], Byte(0x02));
    }

    // A padded reply that stops short of the span end is not complete: the
    // difference between "the gaps are padding" and "a block never arrived" is
    // that the device's replies reached the end of what was asked for.
    void paddedResponseStoppingShortDoesNotComplete()
    {
        RolandRequestTracker tracker(timeouts());
        const RolandAddress address(0x03, 0, 0, 0);
        const auto id = tracker.enqueue(rq1(address, 3072), at(0ms));
        tracker.markSent(id, at(0ms));

        tracker.onDataSet(dt1(address, 73, 0x01), at(35ms));
        for (int tone = 0; tone < 4; ++tone) {
            const auto toneAddress = *address.plus(2048 + static_cast<std::uint64_t>(tone) * 256);
            QCOMPARE(tracker.onDataSet(dt1(toneAddress, 129, 0x02), at(106ms + tone * 70ms)).outcome,
                RolandRequestTracker::MatchOutcome::Accepted);
        }

        const auto* op = tracker.find(id);
        QCOMPARE(op->state, RequestState::Receiving);
        QCOMPARE(op->receivedBytes, 589u);
        QVERIFY(!op->isComplete());
    }

    // The padded-completion path must not fire once the device has gone
    // backwards: a gap below the high-water mark may be data still in flight
    // rather than Roland padding, so full byte coverage is required instead.
    void paddedCompletionIsRefusedAfterAnOutOfOrderChunk()
    {
        RolandRequestTracker tracker(timeouts());
        const RolandAddress address(0x01, 0, 0, 0);
        const auto id = tracker.enqueue(rq1(address, 512), at(0ms));
        tracker.markSent(id, at(0ms));

        tracker.onDataSet(dt1(address, 10, 0x01), at(1ms));            // offset 0..9
        tracker.onDataSet(dt1(*address.plus(256), 10, 0x02), at(2ms)); // jump forward
        tracker.onDataSet(dt1(*address.plus(128), 10, 0x03), at(3ms)); // backwards: anomaly
        QVERIFY(tracker.find(id)->sawChunkOutOfOrder);

        // Reaching the end of the span must no longer be enough on its own.
        const auto match = tracker.onDataSet(dt1(*address.plus(502), 10, 0x04), at(4ms));
        QCOMPARE(match.outcome, RolandRequestTracker::MatchOutcome::Accepted);
        const auto* op = tracker.find(id);
        QCOMPARE(op->coveredThroughBytes, 512u);
        QCOMPARE(op->expectedBytes, 512u);
        QVERIFY(!op->isComplete());
        QCOMPARE(op->state, RequestState::Receiving);
    }

    // A reply that never delivered the requested start address is missing its
    // head, so padding rules do not apply however far forward it reaches.
    void paddedCompletionIsRefusedWhenTheHeadNeverArrived()
    {
        RolandRequestTracker tracker(timeouts());
        const RolandAddress address(0x01, 0, 0, 0);
        const auto id = tracker.enqueue(rq1(address, 512), at(0ms));
        tracker.markSent(id, at(0ms));

        // Ascending throughout, but the first chunk starts past the request address.
        tracker.onDataSet(dt1(*address.plus(128), 10, 0x01), at(1ms));
        const auto match = tracker.onDataSet(dt1(*address.plus(502), 10, 0x02), at(2ms));

        QCOMPARE(match.outcome, RolandRequestTracker::MatchOutcome::Accepted);
        const auto* op = tracker.find(id);
        QVERIFY(!op->sawChunkOutOfOrder);
        QCOMPARE(op->coveredThroughBytes, 512u);
        QVERIFY(!op->isComplete());
        QCOMPARE(op->state, RequestState::Receiving);
    }

    void overlappingChunkIsNotedNotFatal()
    {
        RolandRequestTracker tracker(timeouts());
        const RolandAddress address(0x03, 0, 0, 0);
        const auto id = tracker.enqueue(rq1(address, 8), at(0ms));
        tracker.markSent(id, at(0ms));
        tracker.onDataSet(dt1(address, 6, 0x01), at(1ms));
        const auto match = tracker.onDataSet(dt1(*address.plus(4), 4, 0x02), at(2ms));
        QCOMPARE(match.outcome, RolandRequestTracker::MatchOutcome::Completed);
        const auto* op = tracker.find(id);
        QCOMPARE(op->receivedBytes, 8u);
        QVERIFY(!op->notes.empty());
        QCOMPARE(op->data[4], Byte(0x02)); // latest data wins
    }

    void dataBeyondRangeFailsValidation()
    {
        RolandRequestTracker tracker(timeouts());
        const RolandAddress address(0x03, 0, 0, 0);
        const auto id = tracker.enqueue(rq1(address, 12), at(0ms));
        tracker.markSent(id, at(0ms));
        const auto match = tracker.onDataSet(dt1(*address.plus(8), 8), at(1ms));
        QCOMPARE(match.outcome, RolandRequestTracker::MatchOutcome::Rejected);
        QCOMPARE(tracker.find(id)->state, RequestState::FailedValidation);
        QVERIFY(!tracker.find(id)->failureReason.empty());
        QVERIFY(!tracker.hasOutstanding());
    }

    void unrelatedDataIsReportedAsUnsolicited()
    {
        RolandRequestTracker tracker(timeouts());
        QCOMPARE(tracker.onDataSet(dt1(RolandAddress(0x03, 0, 0, 0), 4), at(0ms)).outcome,
                 RolandRequestTracker::MatchOutcome::NoOutstandingRequest);

        const auto id = tracker.enqueue(rq1(RolandAddress(0x03, 0, 0, 0), 12), at(0ms));
        tracker.markSent(id, at(0ms));
        // Different address range
        QCOMPARE(tracker.onDataSet(dt1(RolandAddress(0x11, 0, 0, 0), 4), at(1ms)).outcome,
                 RolandRequestTracker::MatchOutcome::NoMatch);
        // Different device ID
        const auto otherDevice = RolandSysExMessage::dataSet(RolandDeviceId::fromDisplayNumber(18).value(), kModel,
                                                             RolandAddress(0x03, 0, 0, 0), ByteVector(4, 0)).value();
        QCOMPARE(tracker.onDataSet(otherDevice, at(2ms)).outcome, RolandRequestTracker::MatchOutcome::NoMatch);
        // An RQ1 is never a response
        QCOMPARE(tracker.onDataSet(rq1(RolandAddress(0x03, 0, 0, 0), 4), at(3ms)).outcome,
                 RolandRequestTracker::MatchOutcome::NoMatch);
        QCOMPARE(tracker.find(id)->state, RequestState::AwaitingData);
    }

    void dataIsNotMatchedToUnsentRequests()
    {
        RolandRequestTracker tracker(timeouts());
        const RolandAddress address(0x03, 0, 0, 0);
        const auto id = tracker.enqueue(rq1(address, 12), at(0ms));
        QVERIFY(id.isValid());
        QCOMPARE(tracker.onDataSet(dt1(address, 12), at(1ms)).outcome, RolandRequestTracker::MatchOutcome::NoMatch);
    }

    void firstResponseTimeout()
    {
        RolandRequestTracker tracker(timeouts());
        const auto id = tracker.enqueue(rq1(RolandAddress(0x03, 0, 0, 0), 12), at(0ms));
        tracker.markSent(id, at(100ms));
        QVERIFY(tracker.expire(at(1599ms)).empty());
        const auto expired = tracker.expire(at(1600ms));
        QCOMPARE(expired.size(), std::size_t(1));
        QCOMPARE(expired[0], id);
        QCOMPARE(tracker.find(id)->state, RequestState::TimedOut);
        QVERIFY(tracker.find(id)->failureReason.find("1500") != std::string::npos);
        // Late data is unsolicited now.
        QCOMPARE(tracker.onDataSet(dt1(RolandAddress(0x03, 0, 0, 0), 12), at(1700ms)).outcome,
                 RolandRequestTracker::MatchOutcome::NoOutstandingRequest);
    }

    void betweenChunkTimeout()
    {
        RolandRequestTracker tracker(timeouts());
        const RolandAddress address(0x11, 0, 0, 0);
        const auto id = tracker.enqueue(rq1(address, 256), at(0ms));
        tracker.markSent(id, at(0ms));
        tracker.onDataSet(dt1(address, 128), at(1400ms)); // arrives before the first-response deadline
        QVERIFY(tracker.expire(at(2399ms)).empty());
        QCOMPARE(tracker.expire(at(2400ms)).size(), std::size_t(1));
        const auto* op = tracker.find(id);
        QCOMPARE(op->state, RequestState::TimedOut);
        QCOMPARE(op->receivedBytes, 128u); // partial data preserved for inspection
    }

    void unsentRequestsNeverTimeOut()
    {
        RolandRequestTracker tracker(timeouts());
        const auto id = tracker.enqueue(rq1(RolandAddress(0x03, 0, 0, 0), 12), at(0ms));
        QVERIFY(id.isValid());
        QVERIFY(tracker.expire(at(1h)).empty());
    }

    void cancellation()
    {
        RolandRequestTracker tracker(timeouts());
        const auto a = tracker.enqueue(rq1(RolandAddress(0x03, 0, 0, 0), 12), at(0ms));
        const auto b = tracker.enqueue(rq1(RolandAddress(0x11, 0, 0, 0), 12), at(0ms));
        tracker.markSent(a, at(0ms));
        QVERIFY(tracker.cancel(a, at(5ms), "user"));
        QCOMPARE(tracker.find(a)->state, RequestState::Cancelled);
        QCOMPARE(tracker.find(a)->failureReason, std::string("user"));
        QVERIFY(!tracker.cancel(a, at(6ms))); // already terminal
        QCOMPARE(tracker.cancelAll(at(7ms)), std::size_t(1));
        QCOMPARE(tracker.find(b)->state, RequestState::Cancelled);
        QVERIFY(!tracker.hasOutstanding());
        QVERIFY(!tracker.markSent(b, at(8ms)));
    }

    void explicitFailure()
    {
        RolandRequestTracker tracker(timeouts());
        const auto id = tracker.enqueue(rq1(RolandAddress(0x03, 0, 0, 0), 12), at(0ms));
        QVERIFY(tracker.fail(id, at(1ms), "send failed"));
        QCOMPARE(tracker.find(id)->state, RequestState::FailedValidation);
        QCOMPARE(tracker.find(id)->failureReason, std::string("send failed"));
    }

    void earliestOutstandingRequestWinsWhenRangesOverlap()
    {
        RolandRequestTracker tracker(timeouts());
        const RolandAddress address(0x03, 0, 0, 0);
        const auto first = tracker.enqueue(rq1(address, 12), at(0ms));
        const auto second = tracker.enqueue(rq1(address, 12), at(0ms));
        tracker.markSent(first, at(0ms));
        tracker.markSent(second, at(1ms));
        const auto match = tracker.onDataSet(dt1(address, 12), at(2ms));
        QCOMPARE(match.requestId.value(), first);
        QCOMPARE(tracker.find(second)->state, RequestState::AwaitingData);
        const auto match2 = tracker.onDataSet(dt1(address, 12), at(3ms));
        QCOMPARE(match2.requestId.value(), second);
    }

    void enqueueRejectsNonRequests()
    {
        RolandRequestTracker tracker(timeouts());
        QVERIFY(!tracker.enqueue(dt1(RolandAddress(0x03, 0, 0, 0), 4), at(0ms)).isValid());
        QVERIFY(tracker.operations().empty());
    }

    void historyLimitKeepsOutstanding()
    {
        RolandRequestTracker tracker(timeouts());
        tracker.setHistoryLimit(2);
        const auto a = tracker.enqueue(rq1(RolandAddress(0x03, 0, 0, 0), 1), at(0ms));
        tracker.markSent(a, at(0ms));
        tracker.onDataSet(dt1(RolandAddress(0x03, 0, 0, 0), 1), at(1ms));
        const auto b = tracker.enqueue(rq1(RolandAddress(0x03, 0, 0, 0), 1), at(2ms));
        const auto c = tracker.enqueue(rq1(RolandAddress(0x03, 0, 0, 0), 1), at(3ms));
        QCOMPARE(tracker.operations().size(), std::size_t(2));
        QVERIFY(tracker.find(a) == nullptr); // oldest finished one dropped
        QVERIFY(tracker.find(b) != nullptr);
        QVERIFY(tracker.find(c) != nullptr);
    }

    void stateNames()
    {
        QCOMPARE(QString::fromUtf8(requestStateName(RequestState::FailedValidation).data()), QStringLiteral("FailedValidation"));
        QCOMPARE(QString::fromUtf8(requestStateLabel(RequestState::AwaitingData).data()), QStringLiteral("Awaiting data"));
        QVERIFY(isTerminal(RequestState::Cancelled));
        QVERIFY(!isTerminal(RequestState::Receiving));
    }

    // Pacing helpers ---------------------------------------------------------
    void chunkDataSetSplitsAndAdvancesAddress()
    {
        ByteVector data(600);
        for (std::size_t i = 0; i < data.size(); ++i) {
            data[i] = static_cast<Byte>(i % 128);
        }
        const auto whole = RolandSysExMessage::dataSet(kDevice, kModel, RolandAddress(0x11, 0, 0, 0), data).value();
        // XP-60 rule: packets of 128 bytes or less. 600 = 4 * 128 + 88.
        const auto chunks = chunkDataSet(whole, xp60::transferDefaults().maxDataSetPayloadBytes);
        QCOMPARE(chunks.size(), std::size_t(5));
        for (std::size_t i = 0; i < 4; ++i) {
            QCOMPARE(chunks[i].data().size(), std::size_t(128));
        }
        QCOMPARE(chunks[4].data().size(), std::size_t(88));
        QCOMPARE(QString::fromStdString(chunks[0].address().toHexString()), QStringLiteral("11 00 00 00"));
        QCOMPARE(QString::fromStdString(chunks[1].address().toHexString()), QStringLiteral("11 00 01 00"));
        QCOMPARE(QString::fromStdString(chunks[2].address().toHexString()), QStringLiteral("11 00 02 00"));
        QCOMPARE(QString::fromStdString(chunks[3].address().toHexString()), QStringLiteral("11 00 03 00"));
        QCOMPARE(QString::fromStdString(chunks[4].address().toHexString()), QStringLiteral("11 00 04 00"));
        QCOMPARE(chunks[4].data()[87], data[599]);
        // Every chunk is a valid message on its own.
        for (const auto& chunk : chunks) {
            QVERIFY(chunk.encode().back() == 0xF7);
        }
        QVERIFY(chunkDataSet(rq1(RolandAddress(), 4), 128).empty());
        QVERIFY(chunkDataSet(whole, 0).empty());
        QCOMPARE(chunkDataSet(whole, 1000).size(), std::size_t(1));
    }

    void pacingValidity()
    {
        TransferPacing pacing;
        QVERIFY(pacing.isValid());
        pacing.maxDataSetPayloadBytes = 0;
        QVERIFY(!pacing.isValid());
        pacing.maxDataSetPayloadBytes = 128;
        pacing.timeouts.firstResponse = 0ms;
        QVERIFY(!pacing.isValid());
    }
};

QTEST_APPLESS_MAIN(RequestTrackerTest)
#include "tst_request_tracker.moc"
