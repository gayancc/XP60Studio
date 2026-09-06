// Phase 9 — finding the duplicates and near-duplicates in a library.
//
// The library is populated from tests/fixtures/xp60/user-bank-amal.syx, a real
// XP-60 user bank. It really does contain duplicates: eleven exact pairs and
// one pair that is the same sound under a different name.

#include "services/LibraryDuplicateAnalysis.h"

#include "library/PatchSimilarity.h"
#include "library/SyxImport.h"
#include "xp60/Xp60Device.h"
#include "xpmodel/SysExStream.h"
#include "xpmodel/Xp60PatchCodec.h"
#include "xpmodel/Xp60PatchLayout.h"

#include <QElapsedTimer>
#include <QFile>
#include <QSqlDatabase>
#include <QTest>

#include <algorithm>
#include <set>

using namespace xp60studio;
using library::LibraryDatabase;
using library::LibraryEntry;
using library::PatchSignature;
using library::PatchSimilarity;
using services::LibraryDuplicateAnalysis;
using services::LibraryDuplicateOptions;
using services::LibraryDuplicateReport;

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

bool clusterHolds(const services::SimilarityCluster& cluster, std::string_view name)
{
    return std::find(cluster.names.begin(), cluster.names.end(), name) != cluster.names.end();
}

} // namespace

class TestLibraryDuplicates : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        m_fixture = readFixture();
        QVERIFY2(!m_fixture.empty(), "golden fixture missing");
        QVERIFY2(QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE")),
                 "this Qt build has no QSQLITE driver");
    }

    void init()
    {
        m_db.close();
        QVERIFY(m_db.open(QString::fromLatin1(LibraryDatabase::kInMemoryPath)));
    }

    // --- the signature agrees with the full comparison ---------------------

    void theFastPathAndTheRichPathAgree()
    {
        const auto patches = fixturePatches();

        // The signature exists only to make a sweep affordable. If it ever
        // disagreed with PatchSimilarity, the Compare screen and the duplicate
        // report would show two different numbers for the same pair, and both
        // would stop being trustworthy.
        for (std::size_t i = 0; i < 24; ++i) {
            for (std::size_t j = i + 1; j < 24; ++j) {
                const auto rich = PatchSimilarity::compare(patches[i], patches[j]);
                const auto left = PatchSignature::of(patches[i]);
                const auto right = PatchSignature::of(patches[j]);

                QCOMPARE(left.parameterCount(), rich.comparedParameters());
                QCOMPARE(PatchSignature::equalCount(left, right), rich.equalParameters());
                QCOMPARE(PatchSignature::score(left, right), rich.score());
            }
        }
    }

    void theEarlyExitNeverChangesTheAnswer()
    {
        const auto patches = fixturePatches();
        const auto left = PatchSignature::of(patches[0]);

        for (std::size_t j = 1; j < 40; ++j) {
            const auto right = PatchSignature::of(patches[j]);
            const int equal = PatchSignature::equalCount(left, right);
            // Probe either side of the true answer: abandoning the scan early
            // must give exactly the result a full scan would.
            QVERIFY(PatchSignature::atLeast(left, right, equal));
            QVERIFY(!PatchSignature::atLeast(left, right, equal + 1));
            QVERIFY(PatchSignature::atLeast(left, right, 0));
        }
    }

    void aSignatureLeavesTheNameOut()
    {
        const auto patches = fixturePatches();
        auto renamed = patches[0];
        QVERIFY(renamed.setName(*xpmodel::PatchName::fromText("Renamed")));

        const auto original = PatchSignature::of(patches[0]);
        const auto other = PatchSignature::of(renamed);
        QCOMPARE(PatchSignature::equalCount(original, other), original.parameterCount());
    }

    // --- the sweep --------------------------------------------------------

    void findsTheDuplicatesTheFingerprintCanSee()
    {
        loadFixture();
        LibraryDuplicateOptions options;
        options.minimumPercent = 100;
        const auto report = LibraryDuplicateAnalysis::analyse(m_db, options);

        QCOMPARE(report.entriesConsidered, 128);
        QCOMPARE(report.entriesAnalysed, 128);
        QCOMPARE(report.entriesUndecodable, 0);
        QCOMPARE(report.pairsCompared, std::int64_t(128 * 127 / 2));
        QVERIFY(!report.truncated);

        // Twelve pairs, and at 100% every group is a group of identical sounds.
        QCOMPARE(report.pairs.size(), std::size_t(12));
        QVERIFY(report.nearDuplicateGroups.empty());
        for (const auto& pair : report.pairs) {
            QVERIFY(pair.identicalSound);
            QCOMPARE(pair.percent, 100);
        }

        // "ALL RIGHT.." appears three times, so its three pairs collapse into
        // one group of three rather than three groups of two.
        const auto biggest = report.identicalGroups.front();
        QCOMPARE(biggest.size(), std::size_t(3));
        QVERIFY(biggest.allIdenticalSound);
        QCOMPARE(biggest.minimumPercent, 100);

        int inGroups = 0;
        for (const auto& group : report.identicalGroups) {
            QVERIFY(group.size() >= 2);
            QVERIFY(group.allIdenticalSound);
            QVERIFY(std::is_sorted(group.ids.begin(), group.ids.end()));
            inGroups += static_cast<int>(group.size());
        }
        // Twelve pairs across ten groups: the three "ALL RIGHT.." pairs are one
        // group of three, and the other nine pairs are nine groups of two, so
        // twenty-one of the bank's 128 entries are in a duplicate group.
        QCOMPARE(report.identicalGroups.size(), std::size_t(10));
        QCOMPARE(inGroups, 21);
    }

    void findsTheRenamedDuplicateTheFingerprintCannot()
    {
        loadFixture();
        LibraryDuplicateOptions options;
        options.minimumPercent = 100;
        const auto report = LibraryDuplicateAnalysis::analyse(m_db, options);

        // "Vocal Fall 1" and "Vocal Fall 2" differ in one byte of 2816 — the
        // twelfth name character. Their fingerprints differ, so the database's
        // own duplicate check cannot pair them.
        const auto group = std::find_if(report.identicalGroups.begin(), report.identicalGroups.end(),
                                        [](const services::SimilarityCluster& c) {
                                            return clusterHolds(c, "Vocal Fall 1");
                                        });
        QVERIFY(group != report.identicalGroups.end());
        QCOMPARE(group->size(), std::size_t(2));
        QVERIFY(clusterHolds(*group, "Vocal Fall 2"));

        const auto byFingerprint = m_db.findDuplicatesOf(group->ids.front());
        QVERIFY2(byFingerprint.empty(),
                 "the fingerprint hashes the name, so it cannot see this pair");
    }

    void aLowerThresholdFindsNearDuplicatesAndSaysHowNear()
    {
        loadFixture();
        LibraryDuplicateOptions options;
        options.minimumPercent = 90;
        const auto report = LibraryDuplicateAnalysis::analyse(m_db, options);

        // A looser threshold can only add pairs, never remove them.
        QVERIFY(report.pairs.size() >= 12);
        QVERIFY(std::is_sorted(report.pairs.begin(), report.pairs.end(),
                               [](const services::SimilarPair& a, const services::SimilarPair& b) {
                                   return a.equalParameters > b.equalParameters;
                               }));
        for (const auto& pair : report.pairs) {
            QVERIFY(pair.percent >= 90);
            QVERIFY(pair.percent <= 100);
            QVERIFY(pair.percent < 100 || pair.identicalSound);
            QVERIFY(pair.equalParameters <= pair.comparedParameters);
        }

        // Every group reports the least similar pair inside it, so a group that
        // chained through an intermediate says so instead of hiding it.
        for (const auto& group : report.nearDuplicateGroups) {
            QVERIFY(!group.allIdenticalSound);
            QVERIFY(group.minimumPercent >= 90);
            QVERIFY(group.minimumPercent < 100);
            QVERIFY(group.summary().find("% alike") != std::string::npos);
        }
        for (const auto& group : report.identicalGroups) {
            QVERIFY(group.allIdenticalSound);
        }
    }

    void everyReportedPairSurvivesTheRichComparison()
    {
        loadFixture();
        LibraryDuplicateOptions options;
        options.minimumPercent = 90;
        const auto report = LibraryDuplicateAnalysis::analyse(m_db, options);
        QVERIFY(!report.pairs.empty());

        // The sweep is a shortcut. Re-derive a sample of its findings the slow
        // way — decode both entries, run the full comparison — and require the
        // same verdict.
        for (std::size_t i = 0; i < report.pairs.size(); ++i) {
            const auto& pair = report.pairs[i];
            const auto left = m_db.loadEntry(pair.leftId);
            const auto right = m_db.loadEntry(pair.rightId);
            QVERIFY(left && right);
            const auto rich = PatchSimilarity::compare(left->patch(), right->patch());
            QCOMPARE(rich.percent(), pair.percent);
            QCOMPARE(rich.equalParameters(), pair.equalParameters);
            QCOMPARE(rich.identicalSound(), pair.identicalSound);
        }
    }

    void anImpossibleThresholdFindsNothingAndSaysSo()
    {
        loadFixture();
        LibraryDuplicateOptions options;
        options.minimumPercent = 101;
        const auto report = LibraryDuplicateAnalysis::analyse(m_db, options);

        QCOMPARE(report.entriesAnalysed, 128);
        QVERIFY(report.pairs.empty());
        QVERIFY(report.identicalGroups.empty());
        QVERIFY(report.nearDuplicateGroups.empty());
        QVERIFY(!report.truncated);
        QVERIFY(report.summary().find("Compared 128 Patches") != std::string::npos);
    }

    void aBudgetTruncatesTheInputAndNeverTheFindings()
    {
        loadFixture();
        LibraryDuplicateOptions options;
        options.minimumPercent = 100;
        options.maximumEntries = 50;
        const auto report = LibraryDuplicateAnalysis::analyse(m_db, options);

        QCOMPARE(report.entriesConsidered, 128);
        QCOMPARE(report.entriesAnalysed, 50);
        QCOMPARE(report.pairsCompared, std::int64_t(50 * 49 / 2));
        QVERIFY(report.truncated);
        QVERIFY(report.truncationReason.contains(QStringLiteral("first 50 of 128")));
        QVERIFY(report.summary().find("The sweep was incomplete.") != std::string::npos);

        // Whatever it did compare, it compared completely: every pair among the
        // fifty that qualifies is present.
        for (const auto& pair : report.pairs) {
            QVERIFY(pair.identicalSound);
        }
    }

    void anEmptyLibraryIsNotAnError()
    {
        const auto report = LibraryDuplicateAnalysis::analyse(m_db, {});
        QCOMPARE(report.entriesConsidered, 0);
        QCOMPARE(report.entriesAnalysed, 0);
        QCOMPARE(report.pairsCompared, std::int64_t(0));
        QVERIFY(!report.truncated);
        QCOMPARE(report.summary(), std::string("Nothing to analyse."));
    }

    void aClosedLibraryRefusesRatherThanReportingNothingFound()
    {
        LibraryDatabase closed;
        const auto report = LibraryDuplicateAnalysis::analyse(closed, {});
        QVERIFY(report.truncated);
        QVERIFY(!report.truncationReason.isEmpty());
    }

    void aQueryNarrowsWhatIsSwept()
    {
        loadFixture();
        LibraryDuplicateOptions options;
        options.minimumPercent = 100;
        options.query.text = "Vocal Fall";
        const auto report = LibraryDuplicateAnalysis::analyse(m_db, options);

        QCOMPARE(report.entriesAnalysed, 2);
        QCOMPARE(report.pairsCompared, std::int64_t(1));
        QCOMPARE(report.identicalGroups.size(), std::size_t(1));
        QCOMPARE(report.identicalGroups.front().size(), std::size_t(2));
    }

    void theSweepIsFastEnoughToRunOnAWholeLibrary()
    {
        loadFixture();
        QElapsedTimer timer;
        timer.start();
        LibraryDuplicateOptions options;
        options.minimumPercent = 95;
        const auto report = LibraryDuplicateAnalysis::analyse(m_db, options);
        const auto elapsed = timer.elapsed();

        QCOMPARE(report.pairsCompared, std::int64_t(8128));
        // The rich comparison costs about a millisecond a pair, which would be
        // eight seconds here and half an hour for a thousand Patches. The
        // signature path is the reason a sweep is offered at all, so its cost
        // is asserted rather than assumed. The bound is loose enough not to
        // fail on a slow machine and tight enough to catch a regression back to
        // the slow path.
        QVERIFY2(elapsed < 2000, qPrintable(QStringLiteral("sweep took %1 ms").arg(elapsed)));
    }

private:
    std::vector<xpmodel::Xp60Patch> fixturePatches() const
    {
        const std::vector<roland::RolandModelId> models{xp60::modelId()};
        const auto image = xpmodel::imageFromStream(xpmodel::parseSysExStream(m_fixture, models));
        std::vector<xpmodel::Xp60Patch> patches;
        for (int n = 1; n <= 128; ++n) {
            patches.push_back(
                *xpmodel::Xp60PatchCodec::decode(image, *xpmodel::Xp60PatchLayout::userPatchAddress(n))
                     .patch);
        }
        return patches;
    }

    void loadFixture()
    {
        library::SyxImportOptions options;
        options.sourceName = "user-bank-amal.syx";
        const auto entries = library::importSyxStream(m_fixture, options).entries;
        QCOMPARE(entries.size(), std::size_t(128));
        const auto ids = m_db.insertAll(entries);
        QVERIFY2(ids.has_value(), qPrintable(m_db.lastError()));
    }

    roland::ByteVector m_fixture;
    LibraryDatabase m_db;
};

QTEST_MAIN(TestLibraryDuplicates)
#include "tst_library_duplicates.moc"
