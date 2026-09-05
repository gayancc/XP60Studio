#include "xpmodel/Xp60WaveIdentifier.h"

#include <QtTest>

using namespace xp60studio::xpmodel;

// Every expectation here is hardware evidence, not a reading of the manual.
// The XP-60 was driven from its own front panel on 2026-09-04 and the resulting
// Tone bytes captured; see docs/PHASE_4_WAVE_BROWSER.md, "Bank boundary
// evidence from the front panel".
class WaveIdentifierTest : public QObject
{
    Q_OBJECT

private slots:
    void bankSizesMatchTheInstrument()
    {
        // INT-B 193 was selectable on the panel and read raw 192; INT-A 255 is
        // carried by 302 references in User memory reaching display 255.
        QCOMPARE(waveBankSize(InternalWaveBank::IntA), 255);
        QCOMPARE(waveBankSize(InternalWaveBank::IntB), 193);
        QCOMPARE(QString::fromUtf8(waveBankLabel(InternalWaveBank::IntA).data()), QStringLiteral("INT-A"));
        QCOMPARE(QString::fromUtf8(waveBankLabel(InternalWaveBank::IntB).data()), QStringLiteral("INT-B"));
    }

    void panelSelectionsEncodeToTheCapturedBytes()
    {
        // | Panel      | type | id | number raw |
        // | INT-A 001  |   0  |  1 |     0      |
        // | INT-A 225  |   0  |  1 |   224      |
        // | INT-B 001  |   0  |  2 |     0      |
        // | INT-B 193  |   0  |  2 |   192      |
        const auto intA1 = encodeWave({InternalWaveBank::IntA, 1});
        QVERIFY(intA1.has_value());
        QCOMPARE(intA1->groupTypeRaw, 0);
        QCOMPARE(intA1->groupIdRaw, 1);
        QCOMPARE(intA1->numberRaw, 0);

        const auto intA225 = encodeWave({InternalWaveBank::IntA, 225});
        QVERIFY(intA225.has_value());
        QCOMPARE(intA225->groupIdRaw, 1);
        QCOMPARE(intA225->numberRaw, 224);

        const auto intB1 = encodeWave({InternalWaveBank::IntB, 1});
        QVERIFY(intB1.has_value());
        QCOMPARE(intB1->groupIdRaw, 2);
        QCOMPARE(intB1->numberRaw, 0);

        const auto intB193 = encodeWave({InternalWaveBank::IntB, 193});
        QVERIFY(intB193.has_value());
        QCOMPARE(intB193->groupTypeRaw, 0);
        QCOMPARE(intB193->groupIdRaw, 2);
        QCOMPARE(intB193->numberRaw, 192);
    }

    void decodeIsTheInverseOfEncode()
    {
        for (const auto bank : {InternalWaveBank::IntA, InternalWaveBank::IntB}) {
            for (int number = 1; number <= waveBankSize(bank); ++number) {
                const auto encoded = encodeWave({bank, number});
                QVERIFY(encoded.has_value());
                const auto decoded = decodeWave(encoded->groupTypeRaw, encoded->groupIdRaw, encoded->numberRaw);
                QVERIFY(decoded.has_value());
                QCOMPARE(decoded->bank, bank);
                QCOMPARE(decoded->displayNumber, number);
            }
        }
    }

    void numbersOutsideABankAreRefused()
    {
        // Nothing is clamped: an out-of-range request is a caller error, not a
        // value to be quietly moved to the nearest legal one.
        QVERIFY(!encodeWave({InternalWaveBank::IntA, 0}).has_value());
        QVERIFY(!encodeWave({InternalWaveBank::IntA, 256}).has_value());
        QVERIFY(!encodeWave({InternalWaveBank::IntB, 0}).has_value());
        QVERIFY(!encodeWave({InternalWaveBank::IntB, 194}).has_value());
        // INT-B is the smaller bank; a number legal in INT-A is not legal here.
        QVERIFY(encodeWave({InternalWaveBank::IntA, 200}).has_value());
        QVERIFY(!encodeWave({InternalWaveBank::IntB, 200}).has_value());
    }

    void expansionReferencesAreNotInternalWaves()
    {
        // 61 of the 512 references surveyed in User memory were EXP, across
        // group IDs 1, 5, 7 and 18. They are preserved, never resolved to an
        // internal bank, and never silently treated as INT.
        QVERIFY(!decodeWave(2, 1, 100).has_value());
        QVERIFY(!decodeWave(2, 7, 148).has_value());
        QVERIFY(!decodeWave(2, 18, 50).has_value());
        QVERIFY(!internalWaveBank(2, 1).has_value());
    }

    void unknownInternalGroupIdsAreRefused()
    {
        // Navigating from EXP to INT-B was observed passing Wave Group ID
        // through raw 0, a group no bank claims. Mapping must refuse it rather
        // than assume it cannot occur.
        QVERIFY(!internalWaveBank(0, 0).has_value());
        QVERIFY(!decodeWave(0, 0, 0).has_value());
        // Only 1 and 2 exist: no INT reference in 128 User Patches used another.
        QVERIFY(!internalWaveBank(0, 3).has_value());
        QVERIFY(internalWaveBank(0, 1).has_value());
        QVERIFY(internalWaveBank(0, 2).has_value());
    }

    void rawNumbersBeyondABankAreRefusedOnDecode()
    {
        // raw 192 is INT-B's last wave; raw 193 is past the end of the bank
        // even though it is a legal 7-bit value.
        QVERIFY(decodeWave(0, 2, 192).has_value());
        QVERIFY(!decodeWave(0, 2, 193).has_value());
        QVERIFY(decodeWave(0, 1, 254).has_value());
        QVERIFY(!decodeWave(0, 1, 255).has_value());
    }

    void labelsRoundTrip()
    {
        QCOMPARE(internalWaveBankFromLabel("INT-A").value(), InternalWaveBank::IntA);
        QCOMPARE(internalWaveBankFromLabel("INT-B").value(), InternalWaveBank::IntB);
        QVERIFY(!internalWaveBankFromLabel("INT-C").has_value());
        QVERIFY(!internalWaveBankFromLabel("EXP").has_value());
        QVERIFY(!internalWaveBankFromLabel("").has_value());
    }
};

QTEST_APPLESS_MAIN(WaveIdentifierTest)
#include "tst_wave_identifier.moc"
