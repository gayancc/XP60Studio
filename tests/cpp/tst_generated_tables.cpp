#include "xpmodel/generated/Xp60PatchTables.h"

#include <QtTest>

using namespace xp60studio;
using namespace xp60studio::xpmodel;
using namespace xp60studio::xpmodel::xp60tables;

// Checks the generated tables against facts stated in
// docs/protocol/XP60_PATCH_PARAMETER_MAP.md. Values here are typed by hand
// from the document so a generator bug cannot silently agree with itself.
class GeneratedTablesTest : public QObject
{
    Q_OBJECT

private slots:
    // The category rules match in order, and "LFO1 Waveform" contains the word
    // "Wave". Filed under Wave — as it was until this test existed — copying a
    // Tone's wave silently changed its LFO shapes, and copying LFO 1 left its
    // own shape behind. The parameter's own block position settles it: offset
    // 0x2D sits between the Controller rows and LFO1 Key Trigger.
    void everyLfoParameterIsFiledUnderItsOwnLfo()
    {
        int lfo1 = 0;
        int lfo2 = 0;
        for (const auto& parameter : patchToneTable().parameters()) {
            if (parameter.id.starts_with("tone.lfo1_")) {
                QCOMPARE(parameter.category, std::string_view("LFO1"));
                ++lfo1;
            } else if (parameter.id.starts_with("tone.lfo2_")) {
                QCOMPARE(parameter.category, std::string_view("LFO2"));
                ++lfo2;
            } else if (parameter.category == "Wave") {
                // Whatever else lives under Wave, no LFO does.
                QVERIFY(!parameter.id.starts_with("tone.lfo"));
            }
        }
        QCOMPARE(lfo1, 8);
        QCOMPARE(lfo2, 8);
    }

    void tablesAreCompleteContiguousAndValid()
    {
        for (const auto* table : {&patchCommonTable(), &patchToneTable()}) {
            QVERIFY2(table->validate().empty(), qPrintable(QString::fromUtf8(table->blockName().data())));
            QVERIFY(table->isComplete());
            QVERIFY(table->reservedRanges().empty());
            QCOMPARE(table->describedByteCount(), table->blockSize());
            for (const auto& p : table->parameters()) {
                QCOMPARE(p.status, xp60::VerificationStatus::DocumentationDerived);
                QVERIFY(!p.sourceNote.empty());
                QVERIFY(!p.category.empty());
            }
        }
        QCOMPARE(patchCommonTable().blockSize(), 73u);   // 00 00 00 49
        QCOMPARE(patchToneTable().blockSize(), 129u);    // 00 00 01 01
        QCOMPARE(patchCommonTable().size(), std::size_t(CommonParameter::Count));
        QCOMPARE(patchToneTable().size(), std::size_t(ToneParameter::Count));
        QCOMPARE(std::size_t(CommonParameter::Count), std::size_t(72));
        QCOMPARE(std::size_t(ToneParameter::Count), std::size_t(128));
        QVERIFY(!kSourceDigest.empty());
    }

    void toneOffsetsAndSpan()
    {
        // Roland 10 00, 12 00, 14 00, 16 00 in 7-bit address notation.
        QCOMPARE(kToneOffsets[0], 0x10u * 128);
        QCOMPARE(kToneOffsets[1], 0x12u * 128);
        QCOMPARE(kToneOffsets[2], 0x14u * 128);
        QCOMPARE(kToneOffsets[3], 0x16u * 128);
        QCOMPARE(kPatchSpan, 0x16u * 128 + 129);
    }

    void commonSpotChecks()
    {
        const auto& name1 = descriptor(CommonParameter::PatchName1);
        QCOMPARE(name1.offset, 0u);
        QCOMPARE(name1.encoding, ParameterEncoding::Ascii);
        QCOMPARE(name1.rawMin, 32);
        QCOMPARE(name1.rawMax, 127); // Roland prints 32..127
        QCOMPARE(std::string(name1.id), std::string("common.name.1"));

        const auto& efxType = descriptor(CommonParameter::EfxType);
        QCOMPARE(efxType.offset, 0x0Cu);
        QCOMPARE(efxType.rawMax, 39);
        QCOMPARE(efxType.toDisplay(0), 1);   // display 1..40
        QCOMPARE(efxType.toDisplay(39), 40);

        const auto& tempo = descriptor(CommonParameter::PatchTempo);
        QCOMPARE(tempo.offset, 0x2Cu);
        QCOMPARE(tempo.encoding, ParameterEncoding::Nibble);
        QCOMPARE(int(tempo.byteCount), 2);
        QCOMPARE(tempo.rawMin, 20);
        QCOMPARE(tempo.rawMax, 250);
        QCOMPARE(tempo.toDisplay(120), 120);

        QCOMPARE(descriptor(CommonParameter::PatchLevel).offset, 0x2Eu); // right after the 2-byte tempo
        QCOMPARE(descriptor(CommonParameter::PatchPan).displayStyle, DisplayStyle::Pan);
        QCOMPARE(descriptor(CommonParameter::PatchPan).formatDisplay(0), std::string("L64"));
        QCOMPARE(descriptor(CommonParameter::PatchPan).formatDisplay(64), std::string("0"));
        QCOMPARE(descriptor(CommonParameter::PatchPan).formatDisplay(127), std::string("63R"));

        const auto& bendDown = descriptor(CommonParameter::BendRangeDown);
        QCOMPARE(bendDown.offset, 0x32u);
        QCOMPARE(bendDown.displayScale, -1);
        QCOMPARE(bendDown.toDisplay(48), -48);
        QCOMPARE(bendDown.fromDisplay(-12).value(), 12);

        const auto& depth1 = descriptor(CommonParameter::EfxControlDepth1);
        QCOMPARE(depth1.rawMax, 126);
        QCOMPARE(depth1.toDisplay(0), -63);
        QCOMPARE(depth1.toDisplay(126), 63);
        QCOMPARE(depth1.formatDisplay(70), std::string("+7"));

        QCOMPARE(descriptor(CommonParameter::ReverbHfDamp).label(17).value(), std::string_view("BYPASS"));
        QCOMPARE(descriptor(CommonParameter::ReverbHfDamp).label(0).value(), std::string_view("200"));
        QCOMPARE(descriptor(CommonParameter::StretchTuneDepth).label(0).value(), std::string_view("OFF"));
        QCOMPARE(descriptor(CommonParameter::StretchTuneDepth).label(3).value(), std::string_view("3"));
        QCOMPARE(descriptor(CommonParameter::Booster12).label(3).value(), std::string_view("+18"));
        QCOMPARE(descriptor(CommonParameter::OctaveShift).toDisplay(0), -3);
        QCOMPARE(descriptor(CommonParameter::StructureType12).toDisplay(9), 10);
        QCOMPARE(descriptor(CommonParameter::PatchControlSource3).label(15).value(), std::string_view("PLAYMATE"));
        QCOMPARE(descriptor(CommonParameter::EfxOutputAssign).label(2).value(), std::string_view("<OUTPUT-2>"));

        const auto& clock = descriptor(CommonParameter::ClockSource);
        QCOMPARE(clock.offset, 0x48u);
        QCOMPARE(clock.label(1).value(), std::string_view("SEQUENCER"));
        QCOMPARE(clock.endOffset(), 73u);
    }

    void toneSpotChecks()
    {
        QCOMPARE(descriptor(ToneParameter::ToneSwitch).offset, 0u);
        QCOMPARE(descriptor(ToneParameter::WaveGroupType).label(1).value(), std::string_view("<PCM>"));

        const auto& waveNumber = descriptor(ToneParameter::WaveNumber);
        QCOMPARE(waveNumber.offset, 0x03u);
        QCOMPARE(waveNumber.encoding, ParameterEncoding::Nibble);
        QCOMPARE(waveNumber.rawMax, 254);
        QCOMPARE(waveNumber.toDisplay(0), 1);     // 001..255
        QCOMPARE(waveNumber.toDisplay(254), 255);
        QCOMPARE(descriptor(ToneParameter::WaveGain).offset, 0x05u); // after the 2-byte wave number
        QCOMPARE(descriptor(ToneParameter::WaveGain).label(0).value(), std::string_view("-6"));

        QCOMPARE(descriptor(ToneParameter::ToneDelayMode).label(4).value(), std::string_view("<TAP-SYNC>"));
        QCOMPARE(descriptor(ToneParameter::VelocityRangeLower).rawMin, 1);
        QCOMPARE(descriptor(ToneParameter::KeyboardRangeLower).displayStyle, DisplayStyle::NoteName);
        QCOMPARE(descriptor(ToneParameter::KeyboardRangeLower).formatDisplay(0), std::string("C-1"));
        QCOMPARE(descriptor(ToneParameter::KeyboardRangeUpper).formatDisplay(127), std::string("G9"));
        QCOMPARE(descriptor(ToneParameter::KeyboardRangeUpper).formatDisplay(60), std::string("C4"));

        QCOMPARE(descriptor(ToneParameter::Controller1Destination1).offset, 0x15u);
        QCOMPARE(descriptor(ToneParameter::Controller1Destination1).label(18).value(), std::string_view("L2R"));
        QCOMPARE(descriptor(ToneParameter::Controller3Depth4).offset, 0x2Cu);

        QCOMPARE(descriptor(ToneParameter::Lfo1Waveform).offset, 0x2Du);
        QCOMPARE(descriptor(ToneParameter::Lfo1Waveform).label(5).value(), std::string_view("S&H"));
        QCOMPARE(descriptor(ToneParameter::Lfo1Offset).label(0).value(), std::string_view("-100"));
        QCOMPARE(descriptor(ToneParameter::Lfo2ExternalSync).label(2).value(), std::string_view("<TAP>"));

        const auto& coarse = descriptor(ToneParameter::CoarseTune);
        QCOMPARE(coarse.offset, 0x3Du);
        QCOMPARE(coarse.rawMax, 96);
        QCOMPARE(coarse.toDisplay(48), 0);
        QCOMPARE(coarse.toDisplay(0), -48);
        QCOMPARE(descriptor(ToneParameter::FineTune).toDisplay(100), 50);
        QCOMPARE(descriptor(ToneParameter::RandomPitchDepth).label(30).value(), std::string_view("1200"));
        QCOMPARE(descriptor(ToneParameter::PitchKeyfollow).label(15).value(), std::string_view("+200"));
        QCOMPARE(descriptor(ToneParameter::PitchEnvelopeDepth).toDisplay(24), 12);

        // -100..+150 over raw 0..125 is a step of 2.
        const auto& velSens = descriptor(ToneParameter::PitchEnvelopeVelocitySens);
        QCOMPARE(velSens.rawMax, 125);
        QCOMPARE(velSens.displayScale, 2);
        QCOMPARE(velSens.toDisplay(0), -100);
        QCOMPARE(velSens.toDisplay(125), 150);
        QCOMPARE(velSens.toDisplay(50), 0);
        QCOMPARE(velSens.fromDisplay(150).value(), 125);
        QVERIFY(!velSens.fromDisplay(1).has_value()); // odd values are not representable

        QCOMPARE(descriptor(ToneParameter::PitchEnvelopeTimeKeyfollow).label(7).value(), std::string_view("0"));
        QCOMPARE(descriptor(ToneParameter::FilterType).offset, 0x50u);
        QCOMPARE(descriptor(ToneParameter::FilterType).label(4).value(), std::string_view("PKG"));
        QCOMPARE(descriptor(ToneParameter::FilterEnvelopeVelocityCurve).toDisplay(6), 7);
        QCOMPARE(descriptor(ToneParameter::ToneLevel).offset, 0x65u);
        QCOMPARE(descriptor(ToneParameter::BiasDirection).label(2).value(), std::string_view("LOW&UP"));
        QCOMPARE(descriptor(ToneParameter::BiasPosition).displayStyle, DisplayStyle::NoteName);
        QCOMPARE(descriptor(ToneParameter::LevelEnvelopeLevel3).offset, 0x74u);
        QCOMPARE(descriptor(ToneParameter::LevelLfo1Depth).offset, 0x75u); // no Level Envelope Level 4
        QCOMPARE(descriptor(ToneParameter::TonePan).offset, 0x77u);
        QCOMPARE(descriptor(ToneParameter::TonePan).formatDisplay(64), std::string("0"));
        QCOMPARE(descriptor(ToneParameter::RandomPanDepth).rawMax, 63);
        const auto& altPan = descriptor(ToneParameter::AlternatePanDepth);
        QCOMPARE(altPan.rawMin, 1);
        QCOMPARE(altPan.displayStyle, DisplayStyle::Pan);
        QCOMPARE(altPan.formatDisplay(1), std::string("L63"));
        QCOMPARE(altPan.formatDisplay(127), std::string("63R"));
        QCOMPARE(descriptor(ToneParameter::OutputAssign).offset, 0x7Du);
        QCOMPARE(descriptor(ToneParameter::OutputAssign).label(3).value(), std::string_view("<OUTPUT-2>"));
        const auto& reverbSend = descriptor(ToneParameter::ReverbSendLevel);
        QCOMPARE(reverbSend.offset, 128u); // Roland 01 00
        QCOMPARE(reverbSend.endOffset(), 129u);
    }

    void enumeratorsMatchTableOrder()
    {
        const auto common = patchCommonTable().parameters();
        for (std::size_t i = 0; i < common.size(); ++i) {
            QCOMPARE(&descriptor(static_cast<CommonParameter>(i)), &common[i]);
        }
        const auto tone = patchToneTable().parameters();
        for (std::size_t i = 0; i < tone.size(); ++i) {
            QCOMPARE(&descriptor(static_cast<ToneParameter>(i)), &tone[i]);
        }
        // Offsets strictly increase by byte count (contiguous blocks).
        std::uint32_t cursor = 0;
        for (const auto& p : tone) {
            QCOMPARE(p.offset, cursor);
            cursor += p.byteCount;
        }
        QCOMPARE(cursor, 129u);
    }
};

QTEST_APPLESS_MAIN(GeneratedTablesTest)
#include "tst_generated_tables.moc"
