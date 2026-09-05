// The XP-60 patch-selection hierarchy: SUBGROUP -> BANK -> NUMBER, and the
// linear User number 001-128 the protocol actually carries.
//
// This mapping is load-bearing for the Bank Builder: every destination the
// musician touches is named by the panel coordinates, and every destination
// the instrument is told about is named by the linear number. If the bijection
// is wrong anywhere, a Patch quietly lands somewhere the user did not choose,
// so the round trip is checked exhaustively rather than at a few samples.

#include "xpmodel/Xp60BankLocation.h"

#include <QTest>

#include <set>

using xp60studio::xpmodel::Xp60BankLocation;

class TestBankLocation : public QObject
{
    Q_OBJECT

private slots:
    void mapsThePanelAnchorsTheManualNames();
    void roundTripsEveryUserNumber();
    void coversAllOneHundredAndTwentyEightExactlyOnce();
    void refusesCoordinatesOutsideThePanel();
    void formatsBothIdentities();
    void parsesPanelLabels();
};

void TestBankLocation::mapsThePanelAnchorsTheManualNames()
{
    // The four corners of each subgroup, which is what a user can check
    // against the instrument without counting.
    struct Anchor
    {
        const char* label;
        int userNumber;
    };
    const Anchor anchors[] = {
        {"A11", 1},
        {"A18", 8},
        {"A21", 9},
        {"A88", 64},
        {"B11", 65},
        {"B18", 72},
        {"B21", 73},
        {"B35", 85},
        {"B88", 128},
    };
    for (const auto& anchor : anchors) {
        const auto location = Xp60BankLocation::fromPanelLabel(anchor.label);
        QVERIFY2(location.has_value(), anchor.label);
        QCOMPARE(location->userNumber(), anchor.userNumber);
        QCOMPARE(location->slotIndex(), anchor.userNumber - 1);
    }

    // The example from the product brief: A, BANK 3, NUMBER 5 is Patch 021.
    const auto a35 = Xp60BankLocation::fromPanel(0, 3, 5);
    QVERIFY(a35.has_value());
    QCOMPARE(a35->userNumber(), 21);
    QCOMPARE(QString::fromStdString(a35->panelLabel()), QStringLiteral("A35"));
    QCOMPARE(QString::fromStdString(a35->linearLabel()), QStringLiteral("021"));
}

void TestBankLocation::roundTripsEveryUserNumber()
{
    for (int userNumber = 1; userNumber <= Xp60BankLocation::kUserPatchCount; ++userNumber) {
        const auto location = Xp60BankLocation::fromUserNumber(userNumber);
        QVERIFY(location.has_value());
        QCOMPARE(location->userNumber(), userNumber);

        // Panel coordinates -> location -> panel coordinates.
        const auto again = Xp60BankLocation::fromPanel(location->subgroup(), location->bank(), location->number());
        QVERIFY(again.has_value());
        QCOMPARE(*again, *location);

        // And through the printed label the UI shows.
        const auto parsed = Xp60BankLocation::fromPanelLabel(location->panelLabel());
        QVERIFY(parsed.has_value());
        QCOMPARE(*parsed, *location);

        // Subgroup A is 001-064, subgroup B is 065-128.
        QCOMPARE(location->subgroup(), userNumber <= 64 ? 0 : 1);
    }
}

void TestBankLocation::coversAllOneHundredAndTwentyEightExactlyOnce()
{
    std::set<int> seen;
    for (int subgroup = 0; subgroup < Xp60BankLocation::kSubgroupCount; ++subgroup) {
        for (int bank = 1; bank <= Xp60BankLocation::kBanksPerSubgroup; ++bank) {
            for (int number = 1; number <= Xp60BankLocation::kNumbersPerBank; ++number) {
                const auto location = Xp60BankLocation::fromPanel(subgroup, bank, number);
                QVERIFY(location.has_value());
                const auto inserted = seen.insert(location->userNumber());
                QVERIFY2(inserted.second, qPrintable(QStringLiteral("duplicate user number for %1")
                                                         .arg(QString::fromStdString(location->panelLabel()))));
            }
        }
    }
    QCOMPARE(static_cast<int>(seen.size()), Xp60BankLocation::kUserPatchCount);
    QCOMPARE(*seen.begin(), 1);
    QCOMPARE(*seen.rbegin(), Xp60BankLocation::kUserPatchCount);
}

void TestBankLocation::refusesCoordinatesOutsideThePanel()
{
    // Nothing is clamped: an out-of-range coordinate has no location at all,
    // so it can never resolve to a destination the user did not name.
    QVERIFY(!Xp60BankLocation::fromPanel(-1, 1, 1).has_value());
    QVERIFY(!Xp60BankLocation::fromPanel(2, 1, 1).has_value());
    QVERIFY(!Xp60BankLocation::fromPanel(0, 0, 1).has_value());
    QVERIFY(!Xp60BankLocation::fromPanel(0, 9, 1).has_value());
    QVERIFY(!Xp60BankLocation::fromPanel(0, 1, 0).has_value());
    QVERIFY(!Xp60BankLocation::fromPanel(0, 1, 9).has_value());

    QVERIFY(!Xp60BankLocation::fromUserNumber(0).has_value());
    QVERIFY(!Xp60BankLocation::fromUserNumber(129).has_value());
    QVERIFY(!Xp60BankLocation::fromSlotIndex(-1).has_value());
    QVERIFY(!Xp60BankLocation::fromSlotIndex(128).has_value());
}

void TestBankLocation::formatsBothIdentities()
{
    const auto first = Xp60BankLocation::fromUserNumber(1);
    QVERIFY(first.has_value());
    QCOMPARE(QString::fromStdString(first->panelLabel()), QStringLiteral("A11"));
    QCOMPARE(QString::fromStdString(first->linearLabel()), QStringLiteral("001"));
    QCOMPARE(QString::fromStdString(first->subgroupLabel()), QStringLiteral("A"));
    QCOMPARE(QString::fromStdString(first->spokenLabel()), QString::fromUtf8("A · BANK 1 · 1"));

    const auto last = Xp60BankLocation::fromUserNumber(128);
    QVERIFY(last.has_value());
    QCOMPARE(QString::fromStdString(last->panelLabel()), QStringLiteral("B88"));
    QCOMPARE(QString::fromStdString(last->linearLabel()), QStringLiteral("128"));

    // The linear identity is always three digits, so a column of them lines up.
    QCOMPARE(QString::fromStdString(Xp60BankLocation::linearLabelFor(0)), QStringLiteral("001"));
    QCOMPARE(QString::fromStdString(Xp60BankLocation::linearLabelFor(63)), QStringLiteral("064"));
    QCOMPARE(QString::fromStdString(Xp60BankLocation::linearLabelFor(64)), QStringLiteral("065"));
}

void TestBankLocation::parsesPanelLabels()
{
    QVERIFY(Xp60BankLocation::fromPanelLabel("a35").has_value());
    QCOMPARE(*Xp60BankLocation::fromPanelLabel("a35"), *Xp60BankLocation::fromPanelLabel("A35"));

    QVERIFY(!Xp60BankLocation::fromPanelLabel("").has_value());
    QVERIFY(!Xp60BankLocation::fromPanelLabel("A3").has_value());
    QVERIFY(!Xp60BankLocation::fromPanelLabel("A355").has_value());
    QVERIFY(!Xp60BankLocation::fromPanelLabel("C35").has_value());
    QVERIFY(!Xp60BankLocation::fromPanelLabel("A05").has_value());
    QVERIFY(!Xp60BankLocation::fromPanelLabel("A95").has_value());
    QVERIFY(!Xp60BankLocation::fromPanelLabel("AX5").has_value());
}

QTEST_APPLESS_MAIN(TestBankLocation)
#include "tst_bank_location.moc"
