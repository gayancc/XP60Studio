#include "diagnostics/ProtocolLogEntry.h"
#include "roland/HexFormat.h"
#include "roland/RolandCodec.h"
#include "xp60/Xp60Device.h"

#include <QtTest>

using namespace xp60studio;
using namespace xp60studio::diagnostics;
using namespace xp60studio::roland;

namespace {

std::chrono::system_clock::time_point fixedTime()
{
    return std::chrono::system_clock::time_point(std::chrono::milliseconds(1'700'000'000'115LL));
}

} // namespace

class ProtocolLogTest : public QObject
{
    Q_OBJECT

private slots:
    void requestEntryReadsLikeTheBrief()
    {
        const auto rq1 = RolandSysExMessage::dataRequest(RolandDeviceId::factoryDefault(), xp60::modelId(),
                                                         RolandAddress(0x11, 0, 0, 0), RolandSize(0, 0, 0x0C, 0));
        const auto entry = logRolandMessage(LogDirection::Out, rq1, "WIDI Master", 7, fixedTime());
        QCOMPARE(entry.direction, LogDirection::Out);
        QCOMPARE(entry.kind, LogKind::RolandDataRequest);
        QCOMPARE(entry.deviceIdDisplay.value(), 17);
        QCOMPARE(QString::fromStdString(entry.commandName), QStringLiteral("RQ1"));
        QCOMPARE(QString::fromStdString(entry.addressHex), QStringLiteral("11 00 00 00"));
        QCOMPARE(QString::fromStdString(entry.sizeHex), QStringLiteral("00 00 0C 00"));
        QCOMPARE(entry.checksum, ChecksumStatus::Valid);
        QCOMPARE(entry.requestId.value(), std::uint64_t(7));
        QCOMPARE(QString::fromStdString(entry.summary),
                 QStringLiteral("Roland RQ1 device=17 address=11 00 00 00 size=00 00 0C 00 req=#7"));
        QCOMPARE(QString::fromStdString(entry.rawHex), QStringLiteral("F0 41 10 6A 11 11 00 00 00 00 00 0C 00 63 F7"));

        const QString line = QString::fromStdString(formatLogLine(entry));
        // "HH:MM:SS.mmm OUT Roland RQ1 ..." — the time-of-day depends on the
        // local zone, so only check the shape.
        QVERIFY(line.contains(QStringLiteral(".115 OUT Roland RQ1 device=17 address=11 00 00 00 size=00 00 0C 00")));
        QCOMPARE(line.indexOf(QStringLiteral(" OUT ")), 12);
    }

    void dataSetEntry()
    {
        const auto dt1 = RolandSysExMessage::dataSet(RolandDeviceId::factoryDefault(), xp60::modelId(),
                                                     RolandAddress(0x11, 0, 0, 0), ByteVector(128, 0x40)).value();
        const auto entry = logRolandMessage(LogDirection::In, dt1, "WIDI Master", std::nullopt, fixedTime());
        QCOMPARE(entry.kind, LogKind::RolandDataSet);
        QCOMPARE(entry.payloadLength, std::size_t(128));
        QCOMPARE(QString::fromStdString(entry.summary),
                 QStringLiteral("Roland DT1 device=17 address=11 00 00 00 bytes=128 checksum=OK"));
        QVERIFY(formatLogLine(entry).find(" IN  Roland DT1") != std::string::npos);
    }

    void checksumFailureEntry()
    {
        const auto raw = parseHexBytes("F0 41 10 6A 12 03 00 00 00 57 61 72 6D 37 F7").value();
        const auto decoded = decodeRolandSysEx(raw, xp60::modelId());
        QVERIFY(!decoded.ok());
        const auto entry = logParseFailure(LogDirection::In, midi::MidiByteSpan(raw.data(), raw.size()), *decoded.failure,
                                           "IN", fixedTime());
        QCOMPARE(entry.kind, LogKind::RolandInvalid);
        QCOMPARE(entry.severity, LogSeverity::Error);
        QCOMPARE(entry.checksum, ChecksumStatus::Invalid);
        QCOMPARE(entry.deviceIdDisplay.value(), 17);
        QVERIFY(entry.summary.find("InvalidChecksum") != std::string::npos);
        QVERIFY(entry.summary.find("checksum=INVALID") != std::string::npos);
        QVERIFY(entry.detail.find("expected 66") != std::string::npos);
        QCOMPARE(QString::fromStdString(entry.rawHex), QStringLiteral("F0 41 10 6A 12 03 00 00 00 57 61 72 6D 37 F7"));
    }

    void unsupportedModelIsAWarningNotAnError()
    {
        const auto raw = parseHexBytes("F0 41 10 00 10 12 03 00 00 00 57 7A F7").value();
        const auto decoded = decodeRolandSysEx(raw, xp60::modelId());
        const auto entry = logParseFailure(LogDirection::In, midi::MidiByteSpan(raw.data(), raw.size()), *decoded.failure,
                                           "IN", fixedTime());
        QCOMPARE(entry.kind, LogKind::RolandInvalid);
        QCOMPARE(entry.severity, LogSeverity::Warning);
        QCOMPARE(entry.checksum, ChecksumStatus::NotApplicable);
    }

    void nonRolandSysExIsInformational()
    {
        const auto raw = parseHexBytes("F0 43 10 4C 00 00 7E 00 F7").value();
        const auto decoded = decodeRolandSysEx(raw, xp60::modelId());
        const auto entry = logParseFailure(LogDirection::In, midi::MidiByteSpan(raw.data(), raw.size()), *decoded.failure,
                                           "IN", fixedTime());
        QCOMPARE(entry.kind, LogKind::OtherSysEx);
        QCOMPARE(entry.severity, LogSeverity::Info);
        QVERIFY(entry.summary.find("manufacturer=43") != std::string::npos);
    }

    void rawMidiSummaries()
    {
        const midi::MidiBytes noteOn{0x90, 0x3C, 0x64};
        auto entry = logRawMidi(LogDirection::In, noteOn, "IN", fixedTime());
        QCOMPARE(entry.kind, LogKind::ChannelMessage);
        QCOMPARE(QString::fromStdString(entry.summary), QStringLiteral("Note On ch=1 data=3C 64"));

        const midi::MidiBytes clock{0xF8};
        entry = logRawMidi(LogDirection::In, clock, "IN", fixedTime());
        QCOMPARE(entry.kind, LogKind::SystemMessage);
        QCOMPARE(QString::fromStdString(entry.summary), QStringLiteral("Timing Clock"));

        const midi::MidiBytes program{0xC9, 0x05};
        entry = logRawMidi(LogDirection::Out, program, "OUT", fixedTime());
        QCOMPARE(QString::fromStdString(entry.summary), QStringLiteral("Program Change ch=10 data=05"));
    }

    void systemEntries()
    {
        const auto entry = logSystem(LogKind::Transport, LogSeverity::Error, "Send failed", fixedTime(), "detail", 3);
        QCOMPARE(entry.direction, LogDirection::System);
        QCOMPARE(entry.requestId.value(), std::uint64_t(3));
        QVERIFY(formatLogLine(entry).find(" SYS Send failed") != std::string::npos);
    }

    void names()
    {
        QCOMPARE(QString::fromUtf8(logDirectionName(LogDirection::In).data()), QStringLiteral("IN"));
        QCOMPARE(QString::fromUtf8(checksumStatusName(ChecksumStatus::Invalid).data()), QStringLiteral("INVALID"));
        QCOMPARE(QString::fromUtf8(logKindName(LogKind::RolandDataSet).data()), QStringLiteral("RolandDataSet"));
        QCOMPARE(QString::fromUtf8(logSeverityName(LogSeverity::Warning).data()), QStringLiteral("Warning"));
    }
};

QTEST_APPLESS_MAIN(ProtocolLogTest)
#include "tst_protocol_log.moc"
