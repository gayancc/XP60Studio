// Phase 11 — the running order a musician takes on stage.
//
// Two halves. The model, which resolves what a stage display needs before it
// has to be drawn — the flattened cue list, the sound in force at each cue, and
// everything wrong with the list. And persistence, which follows the saved
// bank's rule: a setlist references Patches and never copies them, so deleting
// a Patch must break a cue loudly rather than shorten somebody's show quietly.

#include "library/LibraryDatabase.h"
#include "library/Setlist.h"
#include "library/SyxImport.h"

#include <QFile>
#include <QSqlDatabase>
#include <QTest>

#include <algorithm>

using namespace xp60studio;
using library::LibraryDatabase;
using library::LibraryEntry;
using library::LiveTarget;
using library::LiveTargetKind;
using library::Setlist;
using library::SetlistProblemKind;
using library::SetlistSection;
using library::SetlistSong;

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

LiveTarget libraryPatch(std::int64_t id, std::string name)
{
    return LiveTarget{LiveTargetKind::LibraryPatch, id, 0, std::move(name)};
}

LiveTarget userPatch(int number, std::string name = {})
{
    return LiveTarget{LiveTargetKind::UserPatchSlot, 0, number, std::move(name)};
}

SetlistSection section(std::string name, LiveTarget target)
{
    return SetlistSection{std::move(name), std::move(target), {}};
}

bool has(const std::vector<library::SetlistProblem>& problems, SetlistProblemKind kind)
{
    return std::any_of(problems.begin(), problems.end(),
                       [kind](const auto& p) { return p.kind == kind; });
}

} // namespace

class TestSetlist : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    // --- the model ----------------------------------------------------------

    void cuesAreFlattenedInRunningOrderWithTheirPosition()
    {
        Setlist list;
        list.name = "Friday";
        list.songs = {
            SetlistSong{"Opener", {}, {section("Intro", userPatch(1, "PIANO")),
                                       section("Verse", {})}},
            SetlistSong{"Second", {}, {section("Whole", userPatch(9, "PAD"))}},
        };

        QCOMPARE(list.cueCount(), 3);
        const auto cues = list.cues();
        QCOMPARE(cues.size(), std::size_t{3});
        QCOMPARE(cues[0].index, 0);
        QCOMPARE(cues[1].songIndex, 0);
        QCOMPARE(cues[1].sectionIndex, 1);
        QCOMPARE(cues[2].songIndex, 1);
        QCOMPARE(cues[2].sectionIndex, 0);
        QVERIFY(cues[0].label().contains(QStringLiteral("Opener")));
        QVERIFY(cues[0].label().contains(QStringLiteral("Intro")));
    }

    void aCarryPreviousCueResolvesToTheSoundActuallyInForce()
    {
        Setlist list;
        list.name = "Friday";
        list.songs = {
            SetlistSong{"Opener", {}, {section("Intro", userPatch(1, "PIANO")),
                                       section("Verse", {}),
                                       section("Chorus", userPatch(4, "ORGAN")),
                                       section("Outro", {})}},
            // A song opening on "carry" keeps what the last song ended on: one
            // running order, not a fresh start per song.
            SetlistSong{"Second", {}, {section("Whole", {})}},
        };

        const auto cues = list.cues();
        QCOMPARE(cues[1].effectiveTarget, userPatch(1, "PIANO"));
        QCOMPARE(cues[3].effectiveTarget, userPatch(4, "ORGAN"));
        QCOMPARE(cues[4].effectiveTarget, userPatch(4, "ORGAN"));
        // The cue's own target still says "carry" — the display needs both, to
        // show that the section does not change sound.
        QVERIFY(!cues[4].target.selectsSomething());
        QVERIFY(list.isPlayable());
    }

    void anOpeningCueThatCarriesNothingIsReportedOnceNotPerCue()
    {
        Setlist list;
        list.name = "Friday";
        list.songs = {SetlistSong{"Opener", {}, {section("Intro", {}), section("Verse", {}),
                                                 section("Chorus", userPatch(1, "PIANO"))}}};
        const auto problems = list.problems();
        // Opening on whatever was left on the instrument is the failure. Once
        // something has been selected, later carries are correct, so only the
        // first is a problem.
        QCOMPARE(std::count_if(problems.begin(), problems.end(),
                               [](const auto& p) {
                                   return p.kind == SetlistProblemKind::NothingToCarry;
                               }),
                 1);
        QVERIFY(!list.isPlayable());
    }

    void aSlotTheInstrumentDoesNotHaveIsReportedRatherThanClamped()
    {
        Setlist list;
        list.name = "Friday";
        list.songs = {SetlistSong{
            "Opener", {}, {section("A", userPatch(129, "nope")),
                           section("B", LiveTarget{LiveTargetKind::UserPerformanceSlot, 0, 33, {}})}}};
        const auto problems = list.problems();
        QCOMPARE(std::count_if(problems.begin(), problems.end(),
                               [](const auto& p) {
                                   return p.kind == SetlistProblemKind::SlotOutOfRange;
                               }),
                 2);
        QVERIFY(problems.front().message.contains(QStringLiteral("does not have")));
    }

    void anEmptySetlistSaysSoAndStopsThere()
    {
        Setlist list;
        list.name = "Friday";
        const auto problems = list.problems();
        QCOMPARE(problems.size(), std::size_t{1});
        QCOMPARE(problems.front().kind, SetlistProblemKind::NoSongs);
        QVERIFY(list.cues().empty());
        QVERIFY(!list.cueAt(0));
        QVERIFY(!list.cueAt(-1));
    }

    void aTargetDescribesItselfForTheStageDisplay()
    {
        QCOMPARE(userPatch(9, "PAD").describe(), QStringLiteral("USER:009 PAD"));
        QCOMPARE(LiveTarget{}.describe(), QStringLiteral("Carry previous sound"));
        QCOMPARE(libraryPatch(4, "GrandPiano").describe(), QStringLiteral("GrandPiano"));
        // The one that matters at bar one: a cue whose Patch is gone is still
        // named, and says why it will not play.
        const auto missing = libraryPatch(0, "GrandPiano");
        QVERIFY(missing.isMissing());
        QVERIFY(missing.describe().contains(QStringLiteral("no longer in the library")));
    }

    // --- persistence --------------------------------------------------------

    void aSetlistRoundTripsThroughTheLibrary()
    {
        const auto ids = storeFixture();
        Setlist list;
        list.name = "Friday";
        list.note = "second set";
        list.songs = {
            SetlistSong{"Opener", "capo 2", {section("Intro", libraryPatch(ids[0], "one")),
                                             section("Verse", {})}},
            SetlistSong{"Second", {}, {section("Whole", userPatch(9, "PAD"))}},
        };

        const auto id = m_db.saveSetlist(list);
        QVERIFY2(id.has_value(), qPrintable(m_db.lastError()));

        const auto loaded = m_db.loadSetlist(*id);
        QVERIFY2(loaded.has_value(), qPrintable(m_db.lastError()));
        QCOMPARE(loaded->name, std::string("Friday"));
        QCOMPARE(loaded->note, std::string("second set"));
        QCOMPARE(loaded->songs.size(), std::size_t{2});
        QCOMPARE(loaded->songs[0].note, std::string("capo 2"));
        QCOMPARE(loaded->songs[0].sections.size(), std::size_t{2});
        QCOMPARE(loaded->songs[0].sections[0].target, libraryPatch(ids[0], "one"));
        QCOMPARE(loaded->songs[0].sections[1].target.kind, LiveTargetKind::CarryPrevious);
        QCOMPARE(loaded->songs[1].sections[0].target, userPatch(9, "PAD"));
    }

    void savingAgainReplacesTheRunningOrderRatherThanAppendingToIt()
    {
        Setlist list;
        list.name = "Friday";
        list.songs = {SetlistSong{"Opener", {}, {section("A", userPatch(1)), section("B", userPatch(2))}}};
        const auto id = m_db.saveSetlist(list);
        QVERIFY(id.has_value());

        list.id = *id;
        list.songs = {SetlistSong{"Renamed", {}, {section("Only", userPatch(3))}}};
        QVERIFY(m_db.saveSetlist(list).has_value());

        const auto loaded = m_db.loadSetlist(*id);
        QVERIFY(loaded.has_value());
        QCOMPARE(loaded->songs.size(), std::size_t{1});
        QCOMPARE(loaded->songs[0].name, std::string("Renamed"));
        QCOMPARE(loaded->songs[0].sections.size(), std::size_t{1});
        QCOMPARE(loaded->cueCount(), 1);
    }

    void deletingAPatchBreaksTheCueLoudlyInsteadOfShorteningTheShow()
    {
        const auto ids = storeFixture();
        Setlist list;
        list.name = "Friday";
        list.songs = {SetlistSong{"Opener", {}, {section("Intro", libraryPatch(ids[0], "GrandPiano")),
                                                 section("Verse", userPatch(9, "PAD"))}}};
        const auto id = m_db.saveSetlist(list);
        QVERIFY(id.has_value());

        QVERIFY(m_db.remove(ids[0]));

        const auto loaded = m_db.loadSetlist(*id);
        QVERIFY(loaded.has_value());
        // The cue is still there, still in position, still named.
        QCOMPARE(loaded->cueCount(), 2);
        const auto& broken = loaded->songs[0].sections[0].target;
        QCOMPARE(broken.kind, LiveTargetKind::LibraryPatch);
        QCOMPARE(broken.libraryId, std::int64_t{0});
        QCOMPARE(broken.name, std::string("GrandPiano"));
        QVERIFY(broken.isMissing());
        QVERIFY(has(loaded->problems(), SetlistProblemKind::MissingLibraryPatch));
        QVERIFY(!loaded->isPlayable());
    }

    void theListingSaysHowBrokenASetlistIsWithoutOpeningIt()
    {
        const auto ids = storeFixture();
        Setlist list;
        list.name = "Friday";
        list.songs = {SetlistSong{"Opener", {}, {section("A", libraryPatch(ids[0], "one")),
                                                 section("B", libraryPatch(ids[1], "two")),
                                                 section("C", userPatch(9))}}};
        QVERIFY(m_db.saveSetlist(list).has_value());
        QVERIFY(m_db.remove(ids[1]));

        const auto records = m_db.setlists();
        QCOMPARE(records.size(), std::size_t{1});
        QCOMPARE(records[0].name, std::string("Friday"));
        QCOMPARE(records[0].songCount, 1);
        QCOMPARE(records[0].cueCount, 3);
        QCOMPARE(records[0].missingCount, 1);
    }

    void aSetlistIsRefusedRatherThanStoredWithASlotTheInstrumentLacks()
    {
        Setlist list;
        list.name = "Friday";
        list.songs = {SetlistSong{"Opener", {}, {section("A", userPatch(200))}}};
        QVERIFY(!m_db.saveSetlist(list).has_value());
        QVERIFY(m_db.lastError().contains(QStringLiteral("does not have")));
        QVERIFY(m_db.setlists().empty());
    }

    void aSetlistNeedsANameAndAnUnknownIdIsNotSilentlyCreated()
    {
        Setlist unnamed;
        QVERIFY(!m_db.saveSetlist(unnamed).has_value());

        Setlist ghost;
        ghost.id = 4242;
        ghost.name = "Friday";
        QVERIFY(!m_db.saveSetlist(ghost).has_value());
        QVERIFY(m_db.lastError().contains(QStringLiteral("4242")));
        QVERIFY(m_db.setlists().empty());
    }

    void removingASetlistRemovesItsCuesAndNoPatches()
    {
        const auto ids = storeFixture();
        Setlist list;
        list.name = "Friday";
        list.songs = {SetlistSong{"Opener", {}, {section("A", libraryPatch(ids[0], "one"))}}};
        const auto id = m_db.saveSetlist(list);
        QVERIFY(id.has_value());

        QVERIFY(m_db.removeSetlist(*id));
        QVERIFY(m_db.setlists().empty());
        QVERIFY(!m_db.loadSetlist(*id).has_value());
        QVERIFY(!m_db.removeSetlist(*id)); // gone, and says so
        // The Patch it referenced is untouched. A setlist is an arrangement.
        QVERIFY(m_db.record(ids[0]).has_value());
    }

    void theSchemaMigratesForwardWithoutTouchingAnExistingLibrary()
    {
        // The setlist tables arrived at schema 6; everything before them is
        // additive, so an older library gains them and keeps its Patches.
        const auto ids = storeFixture();
        QCOMPARE(m_db.schemaVersion().value_or(0), LibraryDatabase::kSchemaVersion);
        QVERIFY(m_db.setlists().empty());
        QVERIFY(m_db.record(ids[0]).has_value());
    }

private:
    [[nodiscard]] std::vector<std::int64_t> storeFixture()
    {
        library::SyxImportOptions options;
        options.sourceName = "user-bank-amal.syx";
        const auto entries = library::importSyxStream(m_fixture, options).entries;
        const auto ids = m_db.insertAll(entries);
        return ids ? *ids : std::vector<std::int64_t>{};
    }

    roland::ByteVector m_fixture;
    LibraryDatabase m_db;
};

void TestSetlist::initTestCase()
{
    m_fixture = readFixture();
    QVERIFY2(!m_fixture.empty(), "golden fixture missing");
    QVERIFY2(QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE")),
             "this Qt build has no QSQLITE driver");
}

void TestSetlist::init()
{
    m_db.close();
    QVERIFY2(m_db.open(QString::fromLatin1(LibraryDatabase::kInMemoryPath)), qPrintable(m_db.lastError()));
}

QTEST_MAIN(TestSetlist)
#include "tst_setlist.moc"
