#include "midi/MidiTypes.h"
#include "midi/SysExAssembler.h"

#include <QtTest>

using namespace xp60studio::midi;

namespace {

struct Collector
{
    std::vector<MidiBytes> messages;
    SysExAssembler::Emit sink()
    {
        return [this](MidiBytes bytes) { messages.push_back(std::move(bytes)); };
    }
};

} // namespace

class SysExAssemblerTest : public QObject
{
    Q_OBJECT

private slots:
    void completeSysExPassesThrough()
    {
        SysExAssembler assembler;
        Collector out;
        const MidiBytes msg{0xF0, 0x41, 0x10, 0x6A, 0x12, 0x03, 0x00, 0x00, 0x00, 0x41, 0x3C, 0xF7};
        assembler.feed(msg, out.sink());
        QCOMPARE(out.messages.size(), std::size_t(1));
        QCOMPARE(out.messages[0], msg);
        QVERIFY(!assembler.sysExInProgress());
    }

    void fragmentedSysExIsReassembled()
    {
        SysExAssembler assembler;
        Collector out;
        const MidiBytes msg{0xF0, 0x41, 0x10, 0x6A, 0x12, 0x03, 0x00, 0x00, 0x00, 0x41, 0x3C, 0xF7};
        assembler.feed(MidiByteSpan(msg.data(), 4), out.sink());
        QVERIFY(assembler.sysExInProgress());
        QCOMPARE(assembler.pendingBytes(), std::size_t(4));
        QVERIFY(out.messages.empty());
        assembler.feed(MidiByteSpan(msg.data() + 4, 5), out.sink());
        QVERIFY(out.messages.empty());
        assembler.feed(MidiByteSpan(msg.data() + 9, msg.size() - 9), out.sink());
        QCOMPARE(out.messages.size(), std::size_t(1));
        QCOMPARE(out.messages[0], msg);
        QVERIFY(!assembler.sysExInProgress());
    }

    void byteAtATime()
    {
        SysExAssembler assembler;
        Collector out;
        const MidiBytes msg{0xF0, 0x41, 0x10, 0x6A, 0x11, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x71, 0xF7};
        for (const Byte b : msg) {
            assembler.feed(MidiByteSpan(&b, 1), out.sink());
        }
        QCOMPARE(out.messages.size(), std::size_t(1));
        QCOMPARE(out.messages[0], msg);
    }

    void twoSysExInOneChunk()
    {
        SysExAssembler assembler;
        Collector out;
        const MidiBytes a{0xF0, 0x41, 0x01, 0xF7};
        const MidiBytes b{0xF0, 0x43, 0x02, 0x03, 0xF7};
        MidiBytes both = a;
        both.insert(both.end(), b.begin(), b.end());
        assembler.feed(both, out.sink());
        QCOMPARE(out.messages.size(), std::size_t(2));
        QCOMPARE(out.messages[0], a);
        QCOMPARE(out.messages[1], b);
    }

    void realtimeBytesInsideSysExPassThrough()
    {
        SysExAssembler assembler;
        Collector out;
        const MidiBytes chunk{0xF0, 0x41, 0xF8, 0x10, 0xFE, 0xF7};
        assembler.feed(chunk, out.sink());
        QCOMPARE(out.messages.size(), std::size_t(3));
        QCOMPARE(out.messages[0], MidiBytes{0xF8});
        QCOMPARE(out.messages[1], MidiBytes{0xFE});
        QCOMPARE(out.messages[2], (MidiBytes{0xF0, 0x41, 0x10, 0xF7}));
    }

    void channelMessagesSplitAcrossChunks()
    {
        SysExAssembler assembler;
        Collector out;
        const MidiBytes noteOn{0x90, 0x3C, 0x64};
        assembler.feed(MidiByteSpan(noteOn.data(), 1), out.sink());
        assembler.feed(MidiByteSpan(noteOn.data() + 1, 2), out.sink());
        QCOMPARE(out.messages.size(), std::size_t(1));
        QCOMPARE(out.messages[0], noteOn);

        const MidiBytes programChange{0xC0, 0x05};
        assembler.feed(programChange, out.sink());
        QCOMPARE(out.messages.size(), std::size_t(2));
        QCOMPARE(out.messages[1], programChange);
    }

    void statusByteAbortsUnfinishedSysEx()
    {
        SysExAssembler assembler;
        Collector out;
        const MidiBytes chunk{0xF0, 0x41, 0x10, 0x90, 0x3C, 0x64};
        assembler.feed(chunk, out.sink());
        QCOMPARE(assembler.abortedSysExCount(), std::size_t(1));
        QCOMPARE(out.messages.size(), std::size_t(1));
        QCOMPARE(out.messages[0], (MidiBytes{0x90, 0x3C, 0x64}));
        QVERIFY(!assembler.sysExInProgress());
    }

    void newSysExStartAbortsPrevious()
    {
        SysExAssembler assembler;
        Collector out;
        assembler.feed(MidiBytes{0xF0, 0x41, 0x10}, out.sink());
        assembler.feed(MidiBytes{0xF0, 0x43, 0x01, 0xF7}, out.sink());
        QCOMPARE(assembler.abortedSysExCount(), std::size_t(1));
        QCOMPARE(out.messages.size(), std::size_t(1));
        QCOMPARE(out.messages[0], (MidiBytes{0xF0, 0x43, 0x01, 0xF7}));
    }

    void strayBytesAreCountedNotDelivered()
    {
        SysExAssembler assembler;
        Collector out;
        assembler.feed(MidiBytes{0x3C, 0x64, 0xF7}, out.sink());
        QVERIFY(out.messages.empty());
        QCOMPARE(assembler.strayByteCount(), std::size_t(3));
    }

    void oversizedSysExIsDropped()
    {
        SysExAssembler assembler(16);
        Collector out;
        MidiBytes big{0xF0};
        for (int i = 0; i < 40; ++i) {
            big.push_back(0x01);
        }
        big.push_back(0xF7);
        assembler.feed(big, out.sink());
        QVERIFY(out.messages.empty());
        QCOMPARE(assembler.overflowSysExCount(), std::size_t(1));
        QVERIFY(!assembler.sysExInProgress());

        // The assembler recovers for the next message.
        assembler.feed(MidiBytes{0xF0, 0x41, 0xF7}, out.sink());
        QCOMPARE(out.messages.size(), std::size_t(1));
    }

    void longSysExOfSeveralKilobytes()
    {
        SysExAssembler assembler;
        Collector out;
        MidiBytes big{0xF0, 0x41, 0x10, 0x6A, 0x12};
        for (int i = 0; i < 20000; ++i) {
            big.push_back(static_cast<Byte>(i % 128));
        }
        big.push_back(0xF7);
        // Deliver in irregular chunks.
        std::size_t offset = 0;
        std::size_t chunk = 1;
        while (offset < big.size()) {
            const std::size_t n = std::min(chunk, big.size() - offset);
            assembler.feed(MidiByteSpan(big.data() + offset, n), out.sink());
            offset += n;
            chunk = (chunk * 3) % 257 + 1;
        }
        QCOMPARE(out.messages.size(), std::size_t(1));
        QCOMPARE(out.messages[0], big);
    }

    void resetDropsPendingState()
    {
        SysExAssembler assembler;
        Collector out;
        assembler.feed(MidiBytes{0xF0, 0x41}, out.sink());
        assembler.reset();
        QVERIFY(!assembler.sysExInProgress());
        QCOMPARE(assembler.pendingBytes(), std::size_t(0));
    }

    void expectedLengths()
    {
        QCOMPARE(expectedMessageLength(0x90), std::size_t(3));
        QCOMPARE(expectedMessageLength(0xC3), std::size_t(2));
        QCOMPARE(expectedMessageLength(0xD0), std::size_t(2));
        QCOMPARE(expectedMessageLength(0xE0), std::size_t(3));
        QCOMPARE(expectedMessageLength(0xF2), std::size_t(3));
        QCOMPARE(expectedMessageLength(0xF6), std::size_t(1));
        QCOMPARE(expectedMessageLength(0xF8), std::size_t(1));
        QCOMPARE(expectedMessageLength(0xF0), std::size_t(0));
        QCOMPARE(expectedMessageLength(0x40), std::size_t(0));
    }
};

QTEST_APPLESS_MAIN(SysExAssemblerTest)
#include "tst_sysex_assembler.moc"
