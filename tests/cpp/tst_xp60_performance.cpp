// Phase 8 — the XP-60 Performance model, layout and codec.
//
// The Patch model's tests prove the same three things and this mirrors them,
// but the Performance has one check the Patch cannot offer: Roland published an
// RQ1 example for the Temporary Performance whose size is `00 00 1F 19`, and
// the span computed from the transcribed block offsets and sizes must equal it.
// That is an arithmetic check of the transcription against Roland's own bytes.
//
// The round-trip runs over tests/fixtures/xp60/user-bank-amal.syx, a real XP-60
// User bank carrying 32 real Performances of Common plus sixteen Parts.

#include "xp60/Xp60Device.h"
#include "xpmodel/SysExStream.h"
#include "xpmodel/Xp60Performance.h"
#include "xpmodel/Xp60PerformanceCodec.h"
#include "xpmodel/Xp60PerformanceLayout.h"

#include <QFile>
#include <QTest>

#include <set>

using namespace xp60studio;
using xpmodel::PartIndex;
using xpmodel::PerformanceCommonParameter;
using xpmodel::PerformancePartParameter;
using xpmodel::Xp60Performance;
using xpmodel::Xp60PerformanceCodec;
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

} // namespace

class TestXp60Performance : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void layoutMatchesRolandsOwnRequestSize();
    void everyPartHasItsOwnBlockAtTheDocumentedOffset();
    void fetchPlanReadsEveryBlockOnce();
    void decodesEveryPerformanceInTheFixture();
    void roundTripsEveryPerformanceByteForByte();
    void readsThePartMixAndRangeAsTheInstrumentShowsThem();
    void refusesAValueOutsideTheDocumentedRange();
    void sendsOnlyWhatChanged();

private:
    xpmodel::MemoryImage m_image;
};

void TestXp60Performance::initTestCase()
{
    const auto bytes = readFixture();
    QVERIFY2(!bytes.empty(), "golden fixture missing");
    const std::vector<roland::RolandModelId> models{xp60::modelId()};
    const auto stream = xpmodel::parseSysExStream(bytes, models);
    QVERIFY(stream.isClean());
    m_image = xpmodel::imageFromStream(stream);
}

// Roland's published RQ1 for the Temporary Performance is
//   F0 41 10 6A 11 01 00 00 00 00 00 1F 19 47 F7
// whose size field is 3993. The blocks transcribed in
// XP60_PERFORMANCE_PARAMETER_MAP.md must span exactly that.
void TestXp60Performance::layoutMatchesRolandsOwnRequestSize()
{
    QCOMPARE(Xp60PerformanceLayout::commonSize(), 66u);
    QCOMPARE(Xp60PerformanceLayout::partSize(), 25u);
    QCOMPARE(Xp60PerformanceLayout::performanceSpan(), 0x1Fu * 128u + 0x19u);
    QCOMPARE(Xp60PerformanceLayout::performanceSpan(), 3993u);
    QVERIFY(Xp60PerformanceLayout::isComplete());

    // Both tables cover exactly the block Roland declares, and the generator's
    // own tiling check means every byte of it is claimed by exactly one row.
    QCOMPARE(Xp60PerformanceLayout::commonTable().blockSize(), Xp60PerformanceLayout::commonSize());
    QCOMPARE(Xp60PerformanceLayout::partTable().blockSize(), Xp60PerformanceLayout::partSize());
    QCOMPARE(Xp60PerformanceLayout::commonTable().size(), std::size_t{65});
    QCOMPARE(Xp60PerformanceLayout::partTable().size(), std::size_t{23});
    QVERIFY(Xp60PerformanceLayout::commonTable().validate().empty());
    QVERIFY(Xp60PerformanceLayout::partTable().validate().empty());
}

void TestXp60Performance::everyPartHasItsOwnBlockAtTheDocumentedOffset()
{
    const auto blocks = Xp60PerformanceLayout::blocks();
    QCOMPARE(blocks.size(), std::size_t{17});
    QCOMPARE(blocks[0].name, QLatin1String("Performance Common").data());
    QVERIFY(!blocks[0].part.has_value());

    // Parts 1..16 at 10 00 .. 1F 00, stride 01 00 — one block each, no overlap.
    std::set<std::uint32_t> offsets;
    for (const auto part : PartIndex::all()) {
        const auto offset = Xp60PerformanceLayout::partOffset(part);
        QCOMPARE(offset, static_cast<std::uint32_t>((0x10 + part.index()) * 128));
        QVERIFY(offsets.insert(offset).second);
        QCOMPARE(blocks[static_cast<std::size_t>(part.number())].part->number(), part.number());
    }
    QCOMPARE(offsets.size(), std::size_t{16});

    // Part 10 is the Rhythm part, which is why Performance Common's EFX Source
    // skips it.
    QVERIFY(PartIndex::rhythmPart().isRhythmPart());
    QCOMPARE(PartIndex::rhythmPart().number(), 10);
    QVERIFY(!PartIndex::part1().isRhythmPart());
    QVERIFY(!PartIndex::fromNumber(0).has_value());
    QVERIFY(!PartIndex::fromNumber(17).has_value());
}

void TestXp60Performance::fetchPlanReadsEveryBlockOnce()
{
    const auto base = Xp60PerformanceLayout::temporaryPerformanceAddress();
    QCOMPARE(QString::fromStdString(base.toHexString()), QStringLiteral("01 00 00 00"));

    const auto plan = Xp60PerformanceLayout::fetchPlan(base);
    QCOMPARE(plan.size(), std::size_t{17});
    std::set<std::string> addresses;
    std::uint32_t total = 0;
    for (const auto& request : plan) {
        QVERIFY(addresses.insert(request.address.toHexString()).second);
        total += request.size.value();
    }
    // 66 + 16 * 25: the payload actually populated, well under the 3993-byte
    // span, because Roland leaves address space between the blocks.
    QCOMPARE(total, 466u);

    // The User bank is 32 Performances at stride 00 01 00 00.
    QCOMPARE(QString::fromStdString(Xp60PerformanceLayout::userPerformanceAddress(1)->toHexString()),
             QStringLiteral("10 00 00 00"));
    QCOMPARE(QString::fromStdString(Xp60PerformanceLayout::userPerformanceAddress(32)->toHexString()),
             QStringLiteral("10 1F 00 00"));
    QVERIFY(!Xp60PerformanceLayout::userPerformanceAddress(0).has_value());
    QVERIFY(!Xp60PerformanceLayout::userPerformanceAddress(33).has_value());
}

void TestXp60Performance::decodesEveryPerformanceInTheFixture()
{
    int decoded = 0;
    for (int n = 1; n <= Xp60PerformanceLayout::kUserPerformanceCount; ++n) {
        const auto base = *Xp60PerformanceLayout::userPerformanceAddress(n);
        const auto result = Xp60PerformanceCodec::decode(m_image, base);
        QVERIFY2(result.ok(), qPrintable(QStringLiteral("USER:%1\n%2").arg(n).arg(
                                  QString::fromStdString(result.describe()))));
        QCOMPARE(result.errorCount(), std::size_t{0});
        // Real user data must sit inside the documented ranges. A warning here
        // would mean the transcription disagrees with the instrument.
        QVERIFY2(!result.hasWarnings(), qPrintable(QString::fromStdString(result.describe())));
        ++decoded;
    }
    QCOMPARE(decoded, 32);

    const auto first = Xp60PerformanceCodec::decode(m_image, *Xp60PerformanceLayout::userPerformanceAddress(1));
    QCOMPARE(QString::fromStdString(first.performance->name().displayText()), QStringLiteral("SIWAKAASI"));

    // The layout's name reader agrees with the decoded model, without decoding.
    const auto names = Xp60PerformanceLayout::readUserPerformanceNames(m_image);
    QCOMPARE(names.size(), std::size_t{32});
    QCOMPARE(QString::fromStdString(names[0].name->displayText()), QStringLiteral("SIWAKAASI"));
    QCOMPARE(names[31].userNumber, 32);
}

// The codec must not normalise: whatever the instrument sent comes back out.
void TestXp60Performance::roundTripsEveryPerformanceByteForByte()
{
    for (int n = 1; n <= Xp60PerformanceLayout::kUserPerformanceCount; ++n) {
        const auto base = *Xp60PerformanceLayout::userPerformanceAddress(n);
        const auto decoded = Xp60PerformanceCodec::decode(m_image, base);
        QVERIFY(decoded.ok());

        const auto rebuilt = Xp60PerformanceCodec::encodeToImage(*decoded.performance, base);
        for (const auto& block : Xp60PerformanceLayout::blocks()) {
            const auto address = *base.plus(block.offset);
            const auto original = m_image.read(address, block.size);
            const auto again = rebuilt.read(address, block.size);
            QVERIFY(original.has_value() && again.has_value());
            QVERIFY2(*original == *again,
                     qPrintable(QStringLiteral("USER:%1 %2 differs").arg(n).arg(
                         QString::fromUtf8(block.name.data(), static_cast<qsizetype>(block.name.size())))));
        }

        // Decoding what was encoded gives an equal Performance.
        const auto again = Xp60PerformanceCodec::decode(rebuilt, base);
        QVERIFY(again.ok());
        QVERIFY(*again.performance == *decoded.performance);
    }
}

void TestXp60Performance::readsThePartMixAndRangeAsTheInstrumentShowsThem()
{
    const auto decoded = Xp60PerformanceCodec::decode(m_image, *Xp60PerformanceLayout::userPerformanceAddress(1));
    QVERIFY(decoded.ok());
    const auto& performance = *decoded.performance;

    QCOMPARE(performance.activePartCount(), [&] {
        int count = 0;
        for (const auto part : PartIndex::all()) {
            if (performance.raw(part, PerformancePartParameter::ReceiveSwitch) != 0) ++count;
        }
        return count;
    }());

    for (const auto part : PartIndex::all()) {
        const auto mix = performance.mix(part);
        QVERIFY(mix.midiChannel >= 1 && mix.midiChannel <= 16);
        QVERIFY(mix.level >= 0 && mix.level <= 127);
        QVERIFY(mix.voiceReserve >= 0 && mix.voiceReserve <= 64);
        QVERIFY(!mix.panText.empty());
        QVERIFY(!mix.outputAssignLabel.empty());

        const auto range = performance.range(part);
        // Roland prints the pair as bounding each other, and real data obeys it.
        QVERIFY(range.lowerRaw <= range.upperRaw);
        QVERIFY(range.octaveShift >= -3 && range.octaveShift <= 3);
        QVERIFY(range.coarseTune >= -48 && range.coarseTune <= 48);
        QVERIFY(range.fineTune >= -50 && range.fineTune <= 50);
        QVERIFY(!range.lowerNote.empty());

        // The Part names a Patch; the model reports it raw and never guesses a
        // bank from the group type.
        const auto assignment = performance.assignment(part);
        QVERIFY(assignment.groupTypeRaw >= 0 && assignment.groupTypeRaw <= 2);
        QVERIFY(!assignment.groupTypeLabel.empty());
        QCOMPARE(assignment.numberDisplay, assignment.numberRaw + 1);
    }

    // Voice Reserve lives in Performance Common but is reported per Part, and
    // the two must agree.
    QCOMPARE(performance.mix(PartIndex::part1()).voiceReserve,
             performance.raw(PerformanceCommonParameter::VoiceReserve1));

    QVERIFY(!performance.summary().empty());
    QVERIFY(QString::fromStdString(performance.summary()).contains(QStringLiteral("SIWAKAASI")));
}

void TestXp60Performance::refusesAValueOutsideTheDocumentedRange()
{
    auto decoded = Xp60PerformanceCodec::decode(m_image, *Xp60PerformanceLayout::userPerformanceAddress(1));
    QVERIFY(decoded.ok());
    auto performance = *decoded.performance;
    const auto before = performance;

    // Voice Reserve is documented 0..64, not 0..127. Nothing is clamped.
    QVERIFY(performance.setRaw(PerformanceCommonParameter::VoiceReserve1, 64));
    QVERIFY(!performance.setRaw(PerformanceCommonParameter::VoiceReserve1, 65));
    QCOMPARE(performance.raw(PerformanceCommonParameter::VoiceReserve1), 64);

    // Part Coarse Tune is 0..96 (-48..+48).
    QVERIFY(performance.setRaw(PartIndex::part1(), PerformancePartParameter::PartCoarseTune, 96));
    QVERIFY(!performance.setRaw(PartIndex::part1(), PerformancePartParameter::PartCoarseTune, 97));
    QCOMPARE(performance.display(PartIndex::part1(), PerformancePartParameter::PartCoarseTune), 48);

    // A name round-trips through the same 12 bytes the Patch name uses.
    const auto renamed = xpmodel::PatchName::fromText("Live Rig 1");
    QVERIFY(renamed.has_value());
    QVERIFY(performance.setName(*renamed));
    QCOMPARE(QString::fromStdString(performance.name().displayText()), QStringLiteral("Live Rig 1"));

    QVERIFY(!(performance == before));
}

// A Part-level edit must cost a short message, not the whole 3993-byte span.
void TestXp60Performance::sendsOnlyWhatChanged()
{
    const auto base = Xp60PerformanceLayout::temporaryPerformanceAddress();
    auto decoded = Xp60PerformanceCodec::decode(m_image, *Xp60PerformanceLayout::userPerformanceAddress(1));
    QVERIFY(decoded.ok());
    const auto before = *decoded.performance;
    auto after = before;

    const auto deviceId = *roland::RolandDeviceId::fromByte(0x10);
    const auto modelId = *roland::RolandModelId::fromBytes(roland::ByteVector{0x6A});

    // Nothing changed: nothing sent.
    QVERIFY(Xp60PerformanceCodec::encodeChangesToDataSets(before, after, deviceId, modelId, base).empty());

    const int level = before.raw(PartIndex::part1(), PerformancePartParameter::PartLevel);
    QVERIFY(after.setRaw(PartIndex::part1(), PerformancePartParameter::PartLevel, level == 100 ? 99 : 100));
    const auto messages = Xp60PerformanceCodec::encodeChangesToDataSets(before, after, deviceId, modelId, base);
    QCOMPARE(messages.size(), std::size_t{1});
    // One byte, addressed inside Part 1's block.
    QCOMPARE(messages[0].data().size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(messages[0].address().toHexString()), QStringLiteral("01 00 10 06"));

    // The whole Performance is seventeen blocks, none of them over 128 bytes,
    // so it goes out as seventeen messages.
    const auto whole = Xp60PerformanceCodec::encodeToDataSets(after, deviceId, modelId, base);
    QCOMPARE(whole.size(), std::size_t{17});
    std::size_t payload = 0;
    for (const auto& message : whole) {
        QVERIFY(message.data().size() <= 128);
        payload += message.data().size();
    }
    QCOMPARE(payload, std::size_t{466});
}

QTEST_MAIN(TestXp60Performance)
#include "tst_xp60_performance.moc"
