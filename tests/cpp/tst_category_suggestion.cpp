// Phase 9 — categorization assistance.
//
// The only honest form of it this project can offer: the user's own labels,
// carried along measured similarity. There is no category byte in the XP-60
// Parameter Address Map, a Patch name is text somebody typed, and structure
// alone cannot decide between a pad and a bass. So a suggestion here is always
// a statement about the user's library, with the neighbours it came from.

#include "services/CategorySuggestion.h"

#include "library/SyxImport.h"

#include <QFile>
#include <QSqlDatabase>
#include <QTest>

#include <algorithm>

using namespace xp60studio;
using library::LibraryDatabase;
using library::LibraryEntry;
using library::PatchUserMetadata;
using services::CategorySuggester;
using services::CategorySuggestion;
using services::CategorySuggestionOptions;

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

class TestCategorySuggestion : public QObject
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
        library::SyxImportOptions options;
        options.sourceName = "user-bank-amal.syx";
        const auto entries = library::importSyxStream(m_fixture, options).entries;
        QCOMPARE(entries.size(), std::size_t(128));
        const auto ids = m_db.insertAll(entries);
        QVERIFY2(ids.has_value(), qPrintable(m_db.lastError()));
        m_ids = *ids;
    }

    // --- refusals ---------------------------------------------------------

    void anUncategorisedLibraryTeachesNothing()
    {
        const auto suggestion = CategorySuggester::suggestFor(m_db, m_ids[0]);
        QVERIFY(!suggestion.available());
        QVERIFY(suggestion.category.empty());
        QVERIFY(suggestion.reason.find("Nothing in your library is categorised") != std::string::npos);
        QCOMPARE(suggestion.summary(), suggestion.reason);
    }

    void aClosedLibrarySaysSoRatherThanSuggestingNothing()
    {
        const auto patch = m_db.loadEntry(m_ids[0])->patch();
        LibraryDatabase closed;
        const auto suggestion = CategorySuggester::suggestFor(closed, patch);
        QVERIFY(!suggestion.available());
        QCOMPARE(suggestion.reason, std::string("The library is not open."));
    }

    void aMissingPatchIsReportedNotIgnored()
    {
        categorise(m_ids[0], "Pad");
        const auto suggestion = CategorySuggester::suggestFor(m_db, 999999);
        QVERIFY(!suggestion.available());
        QVERIFY(suggestion.reason.find("could not be read") != std::string::npos);
    }

    void nothingCloseEnoughIsARefusalWithItsThreshold()
    {
        // Categorise one Patch and ask about a completely different one.
        categorise(m_ids[0], "Pad");
        CategorySuggestionOptions options;
        options.minimumPercent = 99;
        const auto suggestion = CategorySuggester::suggestFor(m_db, m_ids[63], options);

        QVERIFY(!suggestion.available());
        QVERIFY(suggestion.evidence.empty());
        QVERIFY(suggestion.reason.find("at least 99%") != std::string::npos);
    }

    // --- suggestions ------------------------------------------------------

    void carriesACategoryAcrossAnExactDuplicate()
    {
        // Users 1, 3 and 43 are the same sound ("ALL RIGHT.."). File two of
        // them and the third should inherit the label.
        categorise(m_ids[0], "Pad");
        categorise(m_ids[2], "Pad");

        const auto suggestion = CategorySuggester::suggestFor(m_db, m_ids[42]);
        QVERIFY(suggestion.available());
        QCOMPARE(suggestion.category, std::string("Pad"));
        QCOMPARE(suggestion.agreeingNeighbours, 2);
        QCOMPARE(suggestion.consideredNeighbours, 2);
        QCOMPARE(suggestion.confidencePercent, 100);
        QCOMPARE(suggestion.evidence.size(), std::size_t(2));
        for (const auto& evidence : suggestion.evidence) {
            QCOMPARE(evidence.percent, 100);
            QCOMPARE(evidence.category, std::string("Pad"));
        }
        QVERIFY(suggestion.summary().find("\"Pad\"") != std::string::npos);
    }

    void theSubjectNeverVotesForItself()
    {
        categorise(m_ids[0], "Pad");
        // Asking about the very Patch that carries the only label must not come
        // back "Pad, because it is exactly like itself".
        const auto suggestion = CategorySuggester::suggestFor(m_db, m_ids[0]);
        QVERIFY(!suggestion.available());
        for (const auto& evidence : suggestion.evidence) {
            QVERIFY(evidence.id != m_ids[0]);
        }
    }

    void reportsTheDissentAlongsideTheWinner()
    {
        // Three identical Patches, two filed one way and one the other.
        categorise(m_ids[0], "Pad");
        categorise(m_ids[2], "Pad");
        categorise(m_ids[42], "Brass");
        // A fourth Patch that is a copy of the first, asked about fresh.
        const auto patch = m_db.loadEntry(m_ids[0])->patch();

        CategorySuggestionOptions options;
        options.neighbourCount = 3;
        const auto suggestion = CategorySuggester::suggestFor(m_db, patch, options);

        QVERIFY(suggestion.available());
        QCOMPARE(suggestion.category, std::string("Pad"));
        QCOMPARE(suggestion.agreeingNeighbours, 2);
        QCOMPARE(suggestion.consideredNeighbours, 3);
        QCOMPARE(suggestion.confidencePercent, 67);

        // The disagreeing neighbour is in the evidence, not filtered out.
        const bool dissentShown = std::any_of(
            suggestion.evidence.begin(), suggestion.evidence.end(),
            [](const services::CategoryEvidence& e) { return e.category == "Brass"; });
        QVERIFY2(dissentShown, "a suggestion that hides its dissent cannot be checked");
    }

    void aTieIsADisagreementNotACoinToss()
    {
        categorise(m_ids[0], "Pad");
        categorise(m_ids[2], "Brass");
        const auto patch = m_db.loadEntry(m_ids[42])->patch();

        const auto suggestion = CategorySuggester::suggestFor(m_db, patch);
        QVERIFY(!suggestion.available());
        QVERIFY(suggestion.reason.find("more than one category") != std::string::npos);
        // The evidence is still returned, so the user can settle it themselves.
        QCOMPARE(suggestion.evidence.size(), std::size_t(2));
    }

    void tooLittleAgreementRefusesRatherThanGuessing()
    {
        // Five neighbours, split 2/2/1: the winner carries only 40%.
        categorise(m_ids[0], "Pad");
        categorise(m_ids[2], "Pad");
        categorise(m_ids[42], "Brass");
        categorise(m_ids[4], "Brass");
        categorise(m_ids[91], "Lead");

        CategorySuggestionOptions options;
        options.minimumPercent = 0;  // everything is a neighbour
        options.neighbourCount = 5;
        options.minimumAgreementPercent = 60;
        const auto patch = m_db.loadEntry(m_ids[10])->patch();
        const auto suggestion = CategorySuggester::suggestFor(m_db, patch, options);

        QVERIFY(!suggestion.available());
        QVERIFY(!suggestion.evidence.empty());
        QVERIFY(suggestion.reason.find("too little agreement") != std::string::npos
                || suggestion.reason.find("more than one category") != std::string::npos);
    }

    void aSuggestionIsNeverWrittenBack()
    {
        categorise(m_ids[0], "Pad");
        categorise(m_ids[2], "Pad");
        const auto suggestion = CategorySuggester::suggestFor(m_db, m_ids[42]);
        QVERIFY(suggestion.available());

        // Applying it is the user's act, not the suggester's.
        const auto record = m_db.record(m_ids[42]);
        QVERIFY(record);
        QVERIFY2(record->userMetadata.category.empty(),
                 "the suggester must not file the Patch on the user's behalf");
    }

    void neighbourCountBoundsTheVote()
    {
        categorise(m_ids[0], "Pad");
        categorise(m_ids[2], "Pad");
        categorise(m_ids[42], "Pad");

        CategorySuggestionOptions options;
        options.neighbourCount = 2;
        const auto patch = m_db.loadEntry(m_ids[0])->patch();
        const auto suggestion = CategorySuggester::suggestFor(m_db, patch, options);

        QCOMPARE(suggestion.consideredNeighbours, 2);
        QCOMPARE(suggestion.evidence.size(), std::size_t(2));
        QVERIFY(suggestion.available());
    }

    void evidenceIsOrderedMostSimilarFirst()
    {
        for (std::size_t i = 0; i < 20; ++i) {
            categorise(m_ids[i], "Pad");
        }
        CategorySuggestionOptions options;
        options.minimumPercent = 0;
        options.neighbourCount = 10;
        const auto patch = m_db.loadEntry(m_ids[40])->patch();
        const auto suggestion = CategorySuggester::suggestFor(m_db, patch, options);

        QCOMPARE(suggestion.evidence.size(), std::size_t(10));
        QVERIFY(std::is_sorted(suggestion.evidence.begin(), suggestion.evidence.end(),
                               [](const services::CategoryEvidence& a,
                                  const services::CategoryEvidence& b) {
                                   return a.percent > b.percent;
                               }));
    }

private:
    void categorise(std::int64_t id, const std::string& category)
    {
        PatchUserMetadata metadata;
        metadata.category = category;
        QVERIFY(m_db.updateUserMetadata(id, metadata));
    }

    roland::ByteVector m_fixture;
    LibraryDatabase m_db;
    std::vector<std::int64_t> m_ids;
};

QTEST_MAIN(TestCategorySuggestion)
#include "tst_category_suggestion.moc"
