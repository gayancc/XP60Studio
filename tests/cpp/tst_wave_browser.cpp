#include "library/ExpansionBoardCatalog.h"
#include "library/ExpansionProfile.h"
#include "presentation/WaveBrowserModel.h"
#include <QAbstractItemModelTester>
#include <QtTest>
#include <set>

using xp60studio::presentation::WaveBrowserModel;
namespace library = xp60studio::library;
class WaveBrowserTest : public QObject
{
    Q_OBJECT
private slots:
    // ── Expansion waves ──────────────────────────────────────────────────
    //
    // The browser offers what *this* instrument can play: internal waves
    // always, and expansion waves only from boards the musician declared and
    // whose Waveform List this project holds. So a wave picked here is one the
    // Patch will actually sound.
    void expansionWavesAppearOnlyForDeclaredBoards()
    {
        library::ExpansionProfile profile;
        WaveBrowserModel model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        model.setExpansionProfile(&profile);

        // Nothing declared: internal only, and the Expansion tab is empty.
        QCOMPARE(model.expansionCount(), 0);
        QCOMPARE(model.count(), 448);
        model.setSourceFilter(3);
        QCOMPARE(model.count(), 0);

        // Declaring SR-JV80-01 Pop adds exactly its 154 waves.
        QVERIFY(profile.setBoard(1, "SR-JV80-01 Pop", 1));
        model.expansionProfileChanged();
        QCOMPARE(model.expansionCount(), library::srJv80WaveCount(1));
        QCOMPARE(model.count(), library::srJv80WaveCount(1));
        QCOMPARE(model.data(model.index(0), WaveBrowserModel::NameRole).toString(), QStringLiteral("Grand sft 1A"));
        QCOMPARE(model.data(model.index(16), WaveBrowserModel::NameRole).toString(), QStringLiteral("Clav 2A"));
        QCOMPARE(model.data(model.index(16), WaveBrowserModel::NumberRole).toInt(), 17);
        QCOMPARE(model.data(model.index(16), WaveBrowserModel::WaveGroupRole).toInt(), 1);
        QVERIFY(model.data(model.index(16), WaveBrowserModel::ExpansionRole).toBool());
        // Rows are labelled with the slot the instrument shows, not the board.
        QCOMPARE(model.data(model.index(16), WaveBrowserModel::BankRole).toString(), QStringLiteral("EXP-A"));
        QCOMPARE(model.data(model.index(16), WaveBrowserModel::BoardRole).toString(),
                 QStringLiteral("SR-JV80-01 Pop"));

        // A board with no wave group yet, and one whose list this project does
        // not hold, both contribute nothing — there is nothing to name.
        QVERIFY(profile.setBoard(2, "The unlabelled one", std::nullopt));
        QVERIFY(profile.setBoard(3, "SR-JV80-14 Asia", 14));
        model.expansionProfileChanged();
        QCOMPARE(model.expansionCount(), library::srJv80WaveCount(1));

        // Searching finds a board's waves by the board's name.
        model.setSourceFilter(0);
        model.setQuery(QStringLiteral("pop clav"));
        QVERIFY(model.count() > 0);
        for (int row = 0; row < model.count(); ++row) {
            QVERIFY(model.data(model.index(row), WaveBrowserModel::ExpansionRole).toBool());
            QVERIFY(model.data(model.index(row), WaveBrowserModel::NameRole).toString().contains(QStringLiteral("Clav")));
        }
        model.setQuery(QString());

        // Clearing the board takes its waves away again.
        QVERIFY(profile.clearSlot(1));
        model.expansionProfileChanged();
        QCOMPARE(model.expansionCount(), 0);
        QCOMPARE(model.count(), 448);
    }

    void completeCatalog()
    {
        WaveBrowserModel model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QCOMPARE(model.count(), 448);
        std::set<QString> keys;
        for (int row = 0; row < model.count(); ++row)
            QVERIFY(keys.insert(model.data(model.index(row), WaveBrowserModel::KeyRole).toString()).second);
        QCOMPARE(model.data(model.index(0), WaveBrowserModel::NameRole).toString(), "Ac Piano1 A");
        QCOMPARE(model.data(model.index(254), WaveBrowserModel::NameRole).toString(), "Vox Noise");
        QCOMPARE(model.data(model.index(255), WaveBrowserModel::NameRole).toString(), "Kalimba");
        QCOMPARE(model.data(model.index(447), WaveBrowserModel::NameRole).toString(), "DC");
        QVERIFY(!model.data({}, Qt::DisplayRole).isValid());
    }
    void filtersAndSelection()
    {
        WaveBrowserModel model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        model.setSourceFilter(1);
        QCOMPARE(model.count(), 255);
        model.setSourceFilter(2);
        QCOMPARE(model.count(), 193);
        model.setQuery("  inT-b   001 kalIMba  ");
        QCOMPARE(model.count(), 1);
        model.selectRow(0);
        QCOMPARE(model.selected().value("name").toString(), "Kalimba");
        model.setQuery("");
        QCOMPARE(model.selectedRow(), 0);
        model.setSourceFilter(0);
        QCOMPARE(model.selectedRow(), 255);
        model.selectRow(-1);
        model.selectRow(448);
        QCOMPARE(model.selectedRow(), 255);
        model.setSourceFilter(3);
        QCOMPARE(model.count(), 0);
        QVERIFY(model.selected().isEmpty());
        QCOMPARE(model.selectedRow(), -1);
        model.setSourceFilter(4);
        QCOMPARE(model.sourceFilter(), 3);
        model.setSourceFilter(0);
        model.setQuery("no such waveform");
        QCOMPARE(model.count(), 0);
    }
};
QTEST_GUILESS_MAIN(WaveBrowserTest)
#include "tst_wave_browser.moc"
