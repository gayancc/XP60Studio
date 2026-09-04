#include "presentation/WaveBrowserModel.h"
#include <QAbstractItemModelTester>
#include <QtTest>
#include <set>

using xp60studio::presentation::WaveBrowserModel;
class WaveBrowserTest : public QObject
{
    Q_OBJECT
private slots:
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
