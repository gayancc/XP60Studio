#include "roland/HexFormat.h"
#include "roland/RolandCodec.h"
#include "xp60/Xp60Device.h"
#include "xpmodel/SysExStream.h"
#include "xpmodel/Xp60PatchCodec.h"

#include <QtTest>

#include <fstream>
#include <map>
#include <set>

using namespace xp60studio;
using namespace xp60studio::roland;
using namespace xp60studio::xpmodel;

// Validates the model against real XP-60 user data rather than synthetic
// blocks. See tests/fixtures/xp60/README.md for the fixture's provenance.
//
// The fixture is authoritative: if an assertion here fails, the code or the
// transcribed parameter map is wrong, not the file.
class GoldenFixtureTest : public QObject
{
    Q_OBJECT

public:
    GoldenFixtureTest()
    {
        std::ifstream in(QStringLiteral(XP60STUDIO_FIXTURE_DIR "/user-bank-amal.syx").toStdString(), std::ios::binary);
        m_data.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }

private:
    ByteVector m_data;
    std::vector<RolandModelId> m_models{xp60::modelId()};

private slots:
    void initTestCase()
    {
        QVERIFY2(!m_data.empty(), "fixture not found: " XP60STUDIO_FIXTURE_DIR "/user-bank-amal.syx");
        QCOMPARE(m_data.size(), std::size_t(112206));
    }

    void streamParsesCleanly()
    {
        const auto stream = parseSysExStream(m_data, m_models);
        QVERIFY(stream.isClean());
        QCOMPARE(stream.items.size(), std::size_t(1314));
        QCOMPARE(stream.dataSetCount, std::size_t(1314));
        QCOMPARE(stream.requestCount, std::size_t(0));
        QCOMPARE(stream.rejectedRolandCount, std::size_t(0)); // every checksum validates
        QCOMPARE(stream.nonRolandSysExCount, std::size_t(0));
        QCOMPARE(stream.otherMidiCount, std::size_t(0));
        QCOMPARE(stream.strayBytes, std::size_t(0));
        QCOMPARE(stream.abortedSysEx, std::size_t(0));
    }

    void everyMessageReEncodesByteForByte()
    {
        const auto stream = parseSysExStream(m_data, m_models);
        std::size_t total = 0;
        for (const auto& item : stream.items) {
            QVERIFY(item.roland.has_value());
            QVERIFY(item.roland->isDataSet());
            // decode -> encode reproduces the exact original bytes, checksum included.
            QCOMPARE(item.roland->encode(), item.raw);
            QCOMPARE(item.roland->deviceId().displayNumber(), 17);
            QCOMPARE(item.roland->modelId(), xp60::modelId()); // single-byte 6A confirmed on real data
            total += item.raw.size();
        }
        QCOMPARE(total, m_data.size()); // the file is nothing but these messages
    }

    void fileStructureMatchesTheDocumentedLayout()
    {
        const auto stream = parseSysExStream(m_data, m_models);
        std::map<std::pair<Byte, std::size_t>, int> groups;
        std::size_t oversized = 0;
        for (const auto& item : stream.items) {
            groups[{item.roland->address().bytes()[0], item.roland->data().size()}]++;
            oversized += item.roland->data().size() > 128;
        }
        // A helper because a braced map key confuses the QCOMPARE macro.
        const auto count = [&groups](Byte top, std::size_t size) { return groups[std::make_pair(top, size)]; };
        // User Performance area (Phase 8; counted, not decoded).
        QCOMPARE(count(0x10, 66), 32);   // Performance Common x 32
        QCOMPARE(count(0x10, 25), 512);  // Performance Parts 16 x 32
        QCOMPARE(count(0x10, 58), 128);
        QCOMPARE(count(0x10, 12), 2);
        // User Patch area: sizes equal the documented block sizes.
        QCOMPARE(count(0x11, 73), 128);  // Patch Common, 00 00 00 49
        QCOMPARE(count(0x11, 129), 512); // Patch Tone, 00 00 01 01, four per patch

        // Observation, not a defect: the file carries whole 129-byte Tone
        // blocks in single DT1s, above the 128-byte packet limit the MIDI
        // Implementation states for splitting. Recorded in
        // docs/protocol/ROLAND_XP60_PROTOCOL_FACTS.md; XP60Studio still splits
        // at 128 when sending, which is always legal.
        QCOMPARE(oversized, std::size_t(512));
    }

    void userPatchBlockAddressesEqualTheFetchPlan()
    {
        const auto stream = parseSysExStream(m_data, m_models);
        std::vector<std::pair<RolandAddress, std::size_t>> firstPatch;
        for (const auto& item : stream.items) {
            const auto bytes = item.roland->address().bytes();
            if (bytes[0] == 0x11 && bytes[1] == 0x00) {
                firstPatch.emplace_back(item.roland->address(), item.roland->data().size());
            }
        }
        const auto plan = Xp60PatchLayout::fetchPlan(*Xp60PatchLayout::userPatchAddress(1));
        QCOMPARE(firstPatch.size(), plan.size());
        for (std::size_t i = 0; i < plan.size(); ++i) {
            QCOMPARE(firstPatch[i].first, plan[i].address);
            QCOMPARE(firstPatch[i].second, std::size_t(plan[i].size.value()));
        }
    }

    void all128UserPatchesDecodeWithoutIssues()
    {
        const auto image = imageFromStream(parseSysExStream(m_data, m_models));
        QCOMPARE(image.overlappingWriteCount(), std::size_t(0));

        int decoded = 0;
        std::set<std::string> outOfRange;
        for (int n = 1; n <= Xp60PatchLayout::kUserPatchCount; ++n) {
            const auto base = *Xp60PatchLayout::userPatchAddress(n);
            const auto result = Xp60PatchCodec::decode(image, base);
            QVERIFY2(result.missing.empty(), qPrintable(QStringLiteral("USER:%1 %2").arg(n).arg(
                                                 QString::fromStdString(result.describe()))));
            QVERIFY2(result.ok(), qPrintable(QStringLiteral("USER:%1 %2").arg(n).arg(
                                      QString::fromStdString(result.describe()))));
            for (const auto& issue : result.issues) {
                outOfRange.insert(issue.issue.parameterId);
            }
            ++decoded;
        }
        QCOMPARE(decoded, 128);
        // Every one of 128 x 584 = 74 752 parameter values from real patches
        // lies inside the ranges transcribed from the Parameter Address Map.
        // A failure here means a transcribed range is too narrow.
        if (!outOfRange.empty()) {
            QStringList names;
            for (const auto& id : outOfRange) {
                names << QString::fromStdString(id);
            }
            QFAIL(qPrintable("parameters outside their documented range: " + names.join(QStringLiteral(", "))));
        }
    }

    void everyUserPatchRoundTripsByteExact()
    {
        const auto image = imageFromStream(parseSysExStream(m_data, m_models));
        for (int n = 1; n <= Xp60PatchLayout::kUserPatchCount; ++n) {
            const auto base = *Xp60PatchLayout::userPatchAddress(n);
            const auto patch = Xp60PatchCodec::decode(image, base).patch;
            QVERIFY(patch.has_value());

            // encode(decode(bytes)) == bytes for all five blocks.
            const auto again = Xp60PatchCodec::encodeToImage(*patch, base);
            for (const auto& block : Xp60PatchLayout::blocks()) {
                const auto address = *base.plus(block.offset);
                QVERIFY2(again.read(address, block.size) == image.read(address, block.size),
                         qPrintable(QStringLiteral("USER:%1 %2 differs").arg(n).arg(
                             QString::fromUtf8(block.name.data(), qsizetype(block.name.size())))));
            }
            // decode(encode(patch)) == patch
            const auto reDecoded = Xp60PatchCodec::decode(again, base);
            QVERIFY(reDecoded.ok());
            QVERIFY(*reDecoded.patch == *patch);
        }
    }

    void transmittingAPatchReproducesItsBytes()
    {
        const auto image = imageFromStream(parseSysExStream(m_data, m_models));
        const auto base = *Xp60PatchLayout::userPatchAddress(1);
        const auto patch = *Xp60PatchCodec::decode(image, base).patch;

        // Sending splits Tones into 128 + 1 rather than the file's single 129,
        // which is a different but equally valid packetisation of the same bytes.
        const auto messages = Xp60PatchCodec::encodeToDataSets(patch, RolandDeviceId::factoryDefault(), xp60::modelId(), base);
        QCOMPARE(messages.size(), std::size_t(9));
        MemoryImage replay;
        for (const auto& message : messages) {
            QVERIFY(message.data().size() <= 128);
            QVERIFY(replay.addDataSet(message));
        }
        for (const auto& block : Xp60PatchLayout::blocks()) {
            const auto address = *base.plus(block.offset);
            QCOMPARE(replay.read(address, block.size).value(), image.read(address, block.size).value());
        }
    }

    void decodedNamesAndParametersAreMusicallySensible()
    {
        const auto image = imageFromStream(parseSysExStream(m_data, m_models));
        const auto names = Xp60PatchLayout::readUserPatchNames(image);
        QCOMPARE(names.size(), std::size_t(128));
        for (const auto& entry : names) {
            QVERIFY2(entry.name.has_value(), qPrintable(QStringLiteral("USER:%1 has no readable name").arg(entry.userNumber)));
        }
        // Spot checks against the names as stored in the file.
        QCOMPARE(names[3].name->text(), std::string("Jimmee Dee"));
        QCOMPARE(names[33].name->text(), std::string("Singil Piper"));
        QCOMPARE(names[40].name->text(), std::string("64voicePiano"));

        // The name read through the typed model agrees with the raw name read.
        const auto patch = *Xp60PatchCodec::decode(image, *Xp60PatchLayout::userPatchAddress(4)).patch;
        QCOMPARE(patch.name().text(), std::string("Jimmee Dee"));

        // Every patch has at least one enabled Tone and a decodable structure.
        int totalEnabled = 0;
        for (int n = 1; n <= 128; ++n) {
            const auto p = *Xp60PatchCodec::decode(image, *Xp60PatchLayout::userPatchAddress(n)).patch;
            QVERIFY2(p.enabledToneCount() >= 1, qPrintable(QStringLiteral("USER:%1 has no enabled Tone").arg(n)));
            totalEnabled += p.enabledToneCount();
            for (const auto tone : ToneIndex::all()) {
                // Wave group type and gain always resolve to a documented label.
                QVERIFY(p.wave(tone).groupTypeLabel != std::string_view("?"));
                QVERIFY(p.wave(tone).gainLabel != std::string_view("?"));
            }
            QVERIFY(!p.displayText(CommonParameter::ReverbType).empty());
            QVERIFY(!p.summary().empty());
        }
        QVERIFY(totalEnabled >= 128);
    }
};

QTEST_APPLESS_MAIN(GoldenFixtureTest)
#include "tst_golden_fixture.moc"
