#include "roland/HexFormat.h"
#include "xp60/Xp60Device.h"
#include "xpmodel/Xp60Patch.h"
#include "xpmodel/Xp60PatchCodec.h"

#include <QtTest>

#include <random>

using namespace xp60studio;
using namespace xp60studio::roland;
using namespace xp60studio::xpmodel;

namespace {

// Synthetic Patch bytes with every parameter at a legal value. Not a
// hardware fixture: it proves structure and round-trip integrity, not that
// the XP-60 produced these bytes.
ByteVector syntheticBlock(const ParameterTable& table, std::mt19937& rng)
{
    ByteVector bytes(table.blockSize(), 0);
    for (const auto& p : table.parameters()) {
        std::uniform_int_distribution<int> dist(p.rawMin, p.rawMax);
        BlockCodec::writeRaw(p, dist(rng), bytes);
    }
    return bytes;
}

MemoryImage syntheticPatchImage(const RolandAddress& base, unsigned seed, const char* name = "Warm Orchest")
{
    std::mt19937 rng(seed);
    MemoryImage image;
    for (const auto& block : Xp60PatchLayout::blocks()) {
        auto bytes = syntheticBlock(*block.table, rng);
        if (!block.tone) {
            const auto n = PatchName::fromText(name).value().bytes();
            std::copy(n.begin(), n.end(), bytes.begin());
        }
        image.write(*base.plus(block.offset), bytes);
    }
    return image;
}

const RolandAddress kTemp = Xp60PatchLayout::temporaryPatchAddress();

} // namespace

class Xp60PatchTest : public QObject
{
    Q_OBJECT

private slots:
    void layoutBlocksAndFetchPlan()
    {
        QVERIFY(Xp60PatchLayout::isComplete());
        QCOMPARE(Xp60PatchLayout::patchCommonSize(), 73u);
        QCOMPARE(Xp60PatchLayout::toneSize(), 129u);
        QCOMPARE(Xp60PatchLayout::toneOffset(ToneIndex::tone1()), 2048u);
        QCOMPARE(Xp60PatchLayout::toneOffset(ToneIndex::tone4()), 2816u);
        QCOMPARE(Xp60PatchLayout::patchSpan(), 2945u);

        const auto blocks = Xp60PatchLayout::blocks();
        QCOMPARE(blocks.size(), std::size_t(5));
        QVERIFY(!blocks[0].tone.has_value());
        QCOMPARE(blocks[3].tone->number(), 3);

        const auto plan = Xp60PatchLayout::fetchPlan(kTemp);
        QCOMPARE(plan.size(), std::size_t(5));
        QCOMPARE(QString::fromStdString(plan[0].address.toHexString()), QStringLiteral("03 00 00 00"));
        QCOMPARE(QString::fromStdString(plan[0].size.toHexString()), QStringLiteral("00 00 00 49"));
        QCOMPARE(QString::fromStdString(plan[1].address.toHexString()), QStringLiteral("03 00 10 00"));
        QCOMPARE(QString::fromStdString(plan[1].size.toHexString()), QStringLiteral("00 00 01 01"));
        QCOMPARE(QString::fromStdString(plan[2].address.toHexString()), QStringLiteral("03 00 12 00"));
        QCOMPARE(QString::fromStdString(plan[3].address.toHexString()), QStringLiteral("03 00 14 00"));
        QCOMPARE(QString::fromStdString(plan[4].address.toHexString()), QStringLiteral("03 00 16 00"));

        // User patch 128 lives at 11 7F 00 00; its Tone 4 at 11 7F 16 00.
        const auto user128 = Xp60PatchLayout::fetchPlan(*Xp60PatchLayout::userPatchAddress(128));
        QCOMPARE(QString::fromStdString(user128[4].address.toHexString()), QStringLiteral("11 7F 16 00"));

        QVERIFY(!ToneIndex::fromNumber(0).has_value());
        QVERIFY(!ToneIndex::fromNumber(5).has_value());
        QCOMPARE(ToneIndex::fromNumber(2)->index(), std::size_t(1));
    }

    void decodeEncodeRoundTripIsByteExact()
    {
        for (unsigned seed = 1; seed <= 25; ++seed) {
            const auto image = syntheticPatchImage(kTemp, seed);
            const auto decoded = Xp60PatchCodec::decode(image, kTemp);
            QVERIFY2(decoded.ok(), decoded.describe().c_str());
            QVERIFY(decoded.issues.empty());
            QVERIFY(decoded.missing.empty());

            const auto again = Xp60PatchCodec::encodeToImage(*decoded.patch, kTemp);
            for (const auto& block : Xp60PatchLayout::blocks()) {
                const auto address = *kTemp.plus(block.offset);
                QCOMPARE(again.read(address, block.size).value(), image.read(address, block.size).value());
            }
            QCOMPARE(again.byteCount(), std::size_t(73 + 4 * 129));
            QCOMPARE(again.ranges().size(), std::size_t(5)); // blocks are not contiguous in address space

            // decode(encode(patch)) == patch
            const auto twice = Xp60PatchCodec::decode(again, kTemp);
            QVERIFY(twice.ok());
            QVERIFY(*twice.patch == *decoded.patch);
        }
    }

    void typedAccessAndViews()
    {
        auto image = syntheticPatchImage(kTemp, 7, "Piano 1");
        // Force known values into a few fields.
        auto common = image.read(kTemp, 73).value();
        BlockCodec::writeRaw(xp60tables::descriptor(CommonParameter::StructureType12), 2, common);
        BlockCodec::writeRaw(xp60tables::descriptor(CommonParameter::ReverbType), 4, common);
        BlockCodec::writeRaw(xp60tables::descriptor(CommonParameter::PatchTempo), 138, common);
        BlockCodec::writeRaw(xp60tables::descriptor(CommonParameter::EfxType), 0, common);
        BlockCodec::writeRaw(xp60tables::descriptor(CommonParameter::StructureType34), 0, common);
        image.write(kTemp, common);
        auto tone2 = image.read(*kTemp.plus(Xp60PatchLayout::toneOffset(ToneIndex::tone2())), 129).value();
        BlockCodec::writeRaw(xp60tables::descriptor(ToneParameter::ToneSwitch), 1, tone2);
        BlockCodec::writeRaw(xp60tables::descriptor(ToneParameter::WaveGroupType), 2, tone2);
        BlockCodec::writeRaw(xp60tables::descriptor(ToneParameter::WaveGroupId), 4, tone2);
        BlockCodec::writeRaw(xp60tables::descriptor(ToneParameter::WaveNumber), 138, tone2);
        BlockCodec::writeRaw(xp60tables::descriptor(ToneParameter::WaveGain), 3, tone2);
        BlockCodec::writeRaw(xp60tables::descriptor(ToneParameter::CoarseTune), 60, tone2);
        BlockCodec::writeRaw(xp60tables::descriptor(ToneParameter::PitchEnvelopeLevel1), 0, tone2);
        BlockCodec::writeRaw(xp60tables::descriptor(ToneParameter::PitchEnvelopeTime4), 100, tone2);
        BlockCodec::writeRaw(xp60tables::descriptor(ToneParameter::LevelEnvelopeLevel3), 127, tone2);
        BlockCodec::writeRaw(xp60tables::descriptor(ToneParameter::TonePan), 10, tone2);
        image.write(*kTemp.plus(Xp60PatchLayout::toneOffset(ToneIndex::tone2())), tone2);
        for (const auto tone : {ToneIndex::tone1(), ToneIndex::tone3(), ToneIndex::tone4()}) {
            auto bytes = image.read(*kTemp.plus(Xp60PatchLayout::toneOffset(tone)), 129).value();
            BlockCodec::writeRaw(xp60tables::descriptor(ToneParameter::ToneSwitch), 0, bytes);
            image.write(*kTemp.plus(Xp60PatchLayout::toneOffset(tone)), bytes);
        }

        auto decoded = Xp60PatchCodec::decode(image, kTemp);
        QVERIFY(decoded.ok());
        auto& patch = *decoded.patch;

        QCOMPARE(patch.name().text(), std::string("Piano 1"));
        QCOMPARE(patch.raw(CommonParameter::StructureType12), 2);
        QCOMPARE(patch.display(CommonParameter::StructureType12), 3);
        QCOMPARE(patch.displayText(CommonParameter::ReverbType), std::string("HALL1"));
        QCOMPARE(patch.raw(CommonParameter::PatchTempo), 138);
        QVERIFY(!patch.toneEnabled(ToneIndex::tone1()));
        QVERIFY(patch.toneEnabled(ToneIndex::tone2()));
        QCOMPARE(patch.enabledToneCount(), 1);

        const auto wave = patch.wave(ToneIndex::tone2());
        QCOMPARE(wave.groupTypeLabel, std::string_view("EXP"));
        QCOMPARE(wave.groupId, 4);
        QCOMPARE(wave.numberRaw, 138);
        QCOMPARE(wave.numberDisplay, 139);
        QCOMPARE(wave.gainLabel, std::string_view("+12"));

        QCOMPARE(patch.display(ToneIndex::tone2(), ToneParameter::CoarseTune), 12);
        QCOMPARE(patch.displayText(ToneIndex::tone2(), ToneParameter::CoarseTune), std::string("+12"));
        QCOMPARE(patch.displayText(ToneIndex::tone2(), ToneParameter::TonePan), std::string("L54"));

        const auto pitchEnv = patch.pitchEnvelope(ToneIndex::tone2());
        QCOMPARE(pitchEnv.levelCount, 4);
        QCOMPARE(pitchEnv.levelRaw[0], 0);
        QCOMPARE(pitchEnv.levelDisplay[0], -63);
        QCOMPARE(pitchEnv.timeRaw[3], 100);
        const auto levelEnv = patch.levelEnvelope(ToneIndex::tone2());
        QCOMPARE(levelEnv.levelCount, 3);
        QCOMPARE(levelEnv.levelRaw[2], 127);
        QCOMPARE(patch.filterEnvelope(ToneIndex::tone2()).name, std::string_view("Filter Envelope"));

        QVERIFY(patch.summary().find("'Piano 1'") != std::string::npos);
        QVERIFY(patch.summary().find("tones 2") != std::string::npos);
        QVERIFY(patch.summary().find("structure 3/1") != std::string::npos);
        QVERIFY(patch.summary().find("reverb HALL1") != std::string::npos);

        // Setters respect documented ranges and never clamp.
        QVERIFY(patch.setRaw(CommonParameter::EfxType, 39));
        QVERIFY(!patch.setRaw(CommonParameter::EfxType, 40));
        QCOMPARE(patch.raw(CommonParameter::EfxType), 39);
        QVERIFY(patch.setRaw(ToneIndex::tone1(), ToneParameter::ToneSwitch, 1));
        QCOMPARE(patch.enabledToneCount(), 2);
        QVERIFY(patch.setName(PatchName::fromText("XP60STUDIO").value()));
        QCOMPARE(patch.name().text(), std::string("XP60STUDIO"));
        QCOMPARE(patch.common().text("common.name."), std::string("XP60STUDIO"));

        // Edits flow into the encoded bytes.
        const auto encoded = Xp60PatchCodec::encodeToImage(patch, kTemp);
        const auto expectedName = PatchName::fromText("XP60STUDIO")->bytes();
        QCOMPARE(encoded.read(kTemp, 12).value(), ByteVector(expectedName.begin(), expectedName.end()));
        QCOMPARE(encoded.byteAt(*kTemp.plus(0x0C)).value(), Byte(39));
    }

    void outOfRangeBytesAreWarningsAndSurvive()
    {
        auto image = syntheticPatchImage(kTemp, 3);
        auto common = image.read(kTemp, 73).value();
        common[0x0C] = 45; // EFX Type raw 45 > documented 39
        image.write(kTemp, common);
        const auto decoded = Xp60PatchCodec::decode(image, kTemp);
        QVERIFY(decoded.ok());
        QVERIFY(decoded.hasWarnings());
        QCOMPARE(decoded.errorCount(), std::size_t(0));
        QCOMPARE(decoded.issues.size(), std::size_t(1));
        QCOMPARE(decoded.issues[0].block, std::string("Patch Common"));
        QCOMPARE(decoded.issues[0].issue.kind, BlockIssueKind::OutOfRange);
        QCOMPARE(decoded.patch->raw(CommonParameter::EfxType), 45);
        QCOMPARE(Xp60PatchCodec::encodeToImage(*decoded.patch, kTemp).byteAt(*kTemp.plus(0x0C)).value(), Byte(45));
        QVERIFY(decoded.describe().find("OutOfRange") != std::string::npos);
    }

    void missingBlocksAreReportedPrecisely()
    {
        auto image = syntheticPatchImage(kTemp, 4);
        MemoryImage partial;
        // Copy everything except Tone 3, and only half of Tone 4.
        for (const auto& block : Xp60PatchLayout::blocks()) {
            const auto address = *kTemp.plus(block.offset);
            if (block.tone && block.tone->number() == 3) {
                continue;
            }
            auto bytes = image.read(address, block.size).value();
            if (block.tone && block.tone->number() == 4) {
                bytes.resize(60);
            }
            partial.write(address, bytes);
        }
        const auto decoded = Xp60PatchCodec::decode(partial, kTemp);
        QVERIFY(!decoded.ok());
        QCOMPARE(decoded.missing.size(), std::size_t(2));
        QCOMPARE(decoded.missing[0].block, std::string("Tone 3"));
        QCOMPARE(decoded.missing[0].coverage.covered, 0u);
        QCOMPARE(decoded.missing[1].block, std::string("Tone 4"));
        QCOMPARE(decoded.missing[1].coverage.covered, 60u);
        QCOMPARE(decoded.missing[1].coverage.requested, 129u);
        QCOMPARE(QString::fromStdString(decoded.missing[1].coverage.firstMissing->toHexString()), QStringLiteral("03 00 16 3C"));
        QCOMPARE(decoded.errorCount(), std::size_t(2));
        QVERIFY(decoded.describe().find("Tone 3: 0 / 129") != std::string::npos);
    }

    void structuralErrorsBlockDecoding()
    {
        auto image = syntheticPatchImage(kTemp, 5);
        auto tone1 = image.read(*kTemp.plus(2048), 129).value();
        tone1[0x04] = 0x1F; // wave number low nibble byte with bit 4 set
        image.write(*kTemp.plus(2048), tone1);
        const auto decoded = Xp60PatchCodec::decode(image, kTemp);
        QVERIFY(!decoded.ok());
        QVERIFY(decoded.missing.empty());
        QCOMPARE(decoded.errorCount(), std::size_t(1));
        QCOMPARE(decoded.issues[0].block, std::string("Tone 1"));
        QCOMPARE(decoded.issues[0].issue.kind, BlockIssueKind::InvalidNibbleByte);
    }

    void encodeToDataSetsRespectsThePacketRule()
    {
        const auto image = syntheticPatchImage(kTemp, 9);
        const auto patch = *Xp60PatchCodec::decode(image, kTemp).patch;
        const auto messages = Xp60PatchCodec::encodeToDataSets(patch, RolandDeviceId::factoryDefault(), xp60::modelId(), kTemp);
        // Common 73 -> 1 packet; each Tone 129 -> 128 + 1.
        QCOMPARE(messages.size(), std::size_t(1 + 4 * 2));
        for (const auto& m : messages) {
            QVERIFY(m.isDataSet());
            QVERIFY(m.data().size() <= 128);
            QCOMPARE(m.modelId(), xp60::modelId());
        }
        QCOMPARE(QString::fromStdString(messages[0].address().toHexString()), QStringLiteral("03 00 00 00"));
        QCOMPARE(messages[0].data().size(), std::size_t(73));
        QCOMPARE(QString::fromStdString(messages[1].address().toHexString()), QStringLiteral("03 00 10 00"));
        QCOMPARE(messages[1].data().size(), std::size_t(128));
        QCOMPARE(QString::fromStdString(messages[2].address().toHexString()), QStringLiteral("03 00 11 00"));
        QCOMPARE(messages[2].data().size(), std::size_t(1));
        QCOMPARE(QString::fromStdString(messages[8].address().toHexString()), QStringLiteral("03 00 17 00"));

        // Replaying the DT1s reproduces the image.
        MemoryImage replay;
        for (const auto& m : messages) {
            QVERIFY(replay.addDataSet(m));
        }
        for (const auto& block : Xp60PatchLayout::blocks()) {
            const auto address = *kTemp.plus(block.offset);
            QCOMPARE(replay.read(address, block.size).value(), image.read(address, block.size).value());
        }
        // A larger packet allowance keeps whole blocks together.
        QCOMPARE(Xp60PatchCodec::encodeToDataSets(patch, RolandDeviceId::factoryDefault(), xp60::modelId(), kTemp, 256).size(),
                 std::size_t(5));
    }
};

QTEST_APPLESS_MAIN(Xp60PatchTest)
#include "tst_xp60_patch.moc"
