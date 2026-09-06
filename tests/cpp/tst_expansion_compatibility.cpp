// Expansion intelligence: will this Patch play on *my* XP-60?
//
// The question turns entirely on Wave Expansion Boards. A Tone pointing at a
// wave from a board that is not installed has nothing to sound, and the point of
// this analysis is to say so before the musician finds out on stage.
//
// The hard part is not the analysis, it is knowing when to keep quiet. Which
// board a Wave Group ID denotes is not documented by Roland. XP60Studio infers
// it — the ID is taken to be the SR-JV80 catalogue number, which fits every
// group the fixture uses — but an inference is not a report from the
// instrument, and it says nothing about which boards are actually fitted. So the
// verdict stays three-valued, and most of what is checked here is that "missing"
// is only ever said when the musician has told us enough for it to be true.
//
// Everything runs over tests/fixtures/xp60/user-bank-amal.syx, a real XP-60 User
// bank, whose 512 Tones include 192 real expansion references.

#include "library/ExpansionBoardCatalog.h"
#include "library/ExpansionProfile.h"
#include "library/PatchCompatibility.h"
#include "library/LibraryDatabase.h"
#include "library/SyxImport.h"
#include "presentation/ExpansionViewModel.h"
#include "presentation/PatchEditorViewModel.h"
#include "midi/LoopbackMidiTransport.h"
#include "services/DeviceSession.h"
#include "services/PatchWorkspace.h"
#include "xpmodel/Xp60WaveIdentifier.h"

#include <QFile>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QTest>

using namespace xp60studio;
using library::ExpansionProfile;
using library::PatchCompatibilityReport;
using library::ToneCompatibility;
using xpmodel::ToneIndex;
using xpmodel::Xp60Patch;

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

class TestExpansionCompatibility : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // The model
    void expansionReferencesAreCarriedRawAndNeverResolvedToABoard();

    // The profile
    void aSlotWithNoNameProvidesNothing();
    void aBoardWithNoKnownGroupAnswersForNothingButIsNotAbsent();

    // The verdict
    void aPatchOfInternalWavesPlaysAnywhere();
    void anEmptyProfileMakesEveryExpansionVerdictUnknown();
    void aDeclaredBoardMakesItsGroupAvailable();
    void anUndeclaredGroupIsMissingOnceTheProfileIsComplete();
    void oneUnknownBoardHoldsBackEveryMissingVerdict();
    void aDisabledToneIsCountedButDoesNotMakeAPatchUnplayable();

    // The bank
    void reportsWhatAWholeBankNeeds();

    // Persistence and the manager
    void theProfileSurvivesBeingSavedAndReopened();
    void learnsAWaveGroupFromThePatchOnScreen();
    void refusesToLearnFromAnAmbiguousPatch();
    void theWaveBrowserNoteSeparatesTheTwoGaps();

    // The SR-JV80 catalogue, and the limits of the inference behind it.
    void everyGroupTheFixtureUsesIsARealBoardNumber();
    void namesABoardForAGroupWithoutClaimingItIsInstalled();
    void pickingABoardFillsInItsGroupAndLearningStillOverridesIt();
    void namesTheWaveItselfWhereRolandsListIsHeld();
    void everyFixtureReferenceToABoardWeHoldResolvesToARealWave();
    void assigningAnExpansionWaveWritesTheThreeBytesTogether();

    // The three explicit ways out, and the absence of a fourth.
    void offersThreeWaysOutOfAMissingWaveAndReplacesNothingItself();
    void keepingAToneAnywayChangesNothingAndIsForgottenWithThePatch();

private:
    std::vector<Xp60Patch> m_patches;
    // A Patch from the fixture that uses expansion group `group`, or the first
    // Patch when none does.
    [[nodiscard]] const Xp60Patch& patchUsingGroup(int group) const;
    [[nodiscard]] const Xp60Patch& internalOnlyPatch() const;
};

void TestExpansionCompatibility::initTestCase()
{
    const auto bytes = readFixture();
    QVERIFY2(!bytes.empty(), "golden fixture missing");
    for (const auto& entry : library::importSyxStream(bytes).entries) {
        m_patches.push_back(entry.patch());
    }
    QCOMPARE(m_patches.size(), std::size_t{128});
}

const Xp60Patch& TestExpansionCompatibility::patchUsingGroup(int group) const
{
    for (const auto& patch : m_patches) {
        for (const auto tone : ToneIndex::all()) {
            const auto wave = patch.wave(tone);
            const auto expansion = xpmodel::expansionWave(wave.groupTypeRaw, wave.groupId, wave.numberRaw);
            if (expansion && expansion->groupIdRaw == group) {
                return patch;
            }
        }
    }
    return m_patches.front();
}

const Xp60Patch& TestExpansionCompatibility::internalOnlyPatch() const
{
    for (const auto& patch : m_patches) {
        bool internal = true;
        for (const auto tone : ToneIndex::all()) {
            const auto wave = patch.wave(tone);
            if (xpmodel::expansionWave(wave.groupTypeRaw, wave.groupId, wave.numberRaw)) {
                internal = false;
                break;
            }
        }
        if (internal) {
            return patch;
        }
    }
    return m_patches.front();
}

// ---------------------------------------------------------------------------

void TestExpansionCompatibility::expansionReferencesAreCarriedRawAndNeverResolvedToABoard()
{
    // Group type 2 is EXP; the pair comes back exactly as the Tone carries it.
    const auto reference = xpmodel::expansionWave(xpmodel::kExpansionWaveGroupTypeRaw, 5, 41);
    QVERIFY(reference.has_value());
    QCOMPARE(reference->groupIdRaw, 5);
    QCOMPARE(reference->numberRaw, 41);

    // Internal waves and Roland's `<PCM>` group type are not expansion waves.
    QVERIFY(!xpmodel::expansionWave(xpmodel::kInternalWaveGroupTypeRaw, 1, 0).has_value());
    QVERIFY(!xpmodel::expansionWave(1, 1, 0).has_value());
    // Values a Tone could never carry are refused rather than passed through.
    QVERIFY(!xpmodel::expansionWave(xpmodel::kExpansionWaveGroupTypeRaw, 128, 0).has_value());
    QVERIFY(!xpmodel::expansionWave(xpmodel::kExpansionWaveGroupTypeRaw, 5, 255).has_value());
}

void TestExpansionCompatibility::aSlotWithNoNameProvidesNothing()
{
    ExpansionProfile profile;
    QVERIFY(profile.isEmpty());
    QCOMPARE(profile.installedCount(), 0);
    QCOMPARE(QString::fromStdString(library::slotLabel(1)), QStringLiteral("EXP-A"));
    QCOMPARE(QString::fromStdString(library::slotLabel(4)), QStringLiteral("EXP-D"));
    QVERIFY(library::slotLabel(5).empty());

    QVERIFY(profile.setBoard(1, "SR-JV80-05 World", 5));
    QVERIFY(profile.providesGroup(5));
    QCOMPARE(profile.installedCount(), 1);

    // Clearing the slot forgets the group too: an empty slot cannot go on
    // claiming to answer for a wave.
    QVERIFY(profile.clearSlot(1));
    QVERIFY(!profile.providesGroup(5));
    QVERIFY(profile.isEmpty());

    // A group outside the documented 0..127 field could never appear in a Tone.
    QVERIFY(!profile.setBoard(1, "Impossible", 128));
    QVERIFY(!profile.setBoard(9, "No such slot", 5));
}

void TestExpansionCompatibility::aBoardWithNoKnownGroupAnswersForNothingButIsNotAbsent()
{
    ExpansionProfile profile;
    QVERIFY(profile.setBoard(2, "The orchestral one", std::nullopt));

    QVERIFY(!profile.isEmpty());          // something is installed...
    QVERIFY(profile.anyGroupUnknown());   // ...but we cannot say what it answers for
    QVERIFY(!profile.providesGroup(5));

    // Learning the group is how it becomes usable, and it can only be learned
    // for a slot that has a board in it.
    QVERIFY(!profile.setWaveGroup(3, 7));
    QVERIFY(profile.setWaveGroup(2, 7));
    QVERIFY(profile.providesGroup(7));
    QVERIFY(!profile.anyGroupUnknown());
    QCOMPARE(profile.slotProviding(7).value(), 2);
}

// ---------------------------------------------------------------------------

void TestExpansionCompatibility::aPatchOfInternalWavesPlaysAnywhere()
{
    const auto report = library::analysePatch(internalOnlyPatch(), ExpansionProfile{});
    QVERIFY(!report.usesExpansion());
    QVERIFY(report.playable());
    QVERIFY(!report.undecided());
    QVERIFY(report.requiredGroups.empty());
    QVERIFY2(QString::fromStdString(report.summary()).contains(QStringLiteral("any XP-60")),
             qPrintable(QString::fromStdString(report.summary())));
}

// The rule that matters most: with nothing declared, "missing" is an invention.
void TestExpansionCompatibility::anEmptyProfileMakesEveryExpansionVerdictUnknown()
{
    const auto report = library::analysePatch(patchUsingGroup(5), ExpansionProfile{});

    QVERIFY(report.usesExpansion());
    QVERIFY(report.requiredGroups.count(5) > 0);
    QVERIFY2(report.missingGroups.empty(), "nothing can be missing until the musician says what they own");
    QVERIFY(report.undecided());
    QVERIFY2(report.playable(), "an undecided Patch is not reported as unplayable");
    bool sawUnknown = false;
    for (const auto& tone : report.tones) {
        if (tone.status == ToneCompatibility::ExpansionUnknown) {
            sawUnknown = true;
            QVERIFY(tone.waveGroupId.has_value());
        }
    }
    QVERIFY(sawUnknown);
    QVERIFY(QString::fromStdString(report.summary()).contains(QStringLiteral("cannot tell")));
}

void TestExpansionCompatibility::aDeclaredBoardMakesItsGroupAvailable()
{
    ExpansionProfile profile;
    QVERIFY(profile.setBoard(1, "World", 5));

    const auto report = library::analysePatch(patchUsingGroup(5), profile);
    QVERIFY(report.requiredGroups.count(5) > 0);

    bool sawAvailable = false;
    for (const auto& tone : report.tones) {
        if (tone.waveGroupId && *tone.waveGroupId == 5) {
            QCOMPARE(tone.status, ToneCompatibility::ExpansionAvailable);
            QCOMPARE(tone.providedBySlot.value(), 1);
            sawAvailable = true;
        }
    }
    QVERIFY(sawAvailable);
}

void TestExpansionCompatibility::anUndeclaredGroupIsMissingOnceTheProfileIsComplete()
{
    ExpansionProfile profile;
    // A complete profile: one board, and we know what it answers for. Anything
    // else is genuinely absent.
    QVERIFY(profile.setBoard(1, "World", 5));
    QVERIFY(!profile.anyGroupUnknown());

    const auto report = library::analysePatch(patchUsingGroup(7), profile);
    QVERIFY(report.requiredGroups.count(7) > 0);
    QVERIFY(report.missingGroups.count(7) > 0);
    QVERIFY(!report.undecided());

    bool sawMissingEnabled = false;
    for (const auto& tone : report.tones) {
        if (tone.waveGroupId && *tone.waveGroupId == 7) {
            QCOMPARE(tone.status, ToneCompatibility::ExpansionMissing);
            QVERIFY(!tone.providedBySlot.has_value());
            sawMissingEnabled = sawMissingEnabled || tone.enabled;
        }
    }
    QVERIFY(sawMissingEnabled);
    QVERIFY(!report.playable());
    QVERIFY(QString::fromStdString(report.summary()).contains(QStringLiteral("not installed")));
}

// One board whose group nobody knows could be the very board a Patch wants, so
// it holds back every "missing" verdict rather than only its own.
void TestExpansionCompatibility::oneUnknownBoardHoldsBackEveryMissingVerdict()
{
    ExpansionProfile profile;
    QVERIFY(profile.setBoard(1, "World", 5));
    QVERIFY(profile.setBoard(2, "Something I have not identified yet", std::nullopt));

    const auto report = library::analysePatch(patchUsingGroup(7), profile);
    QVERIFY(report.requiredGroups.count(7) > 0);
    QVERIFY2(report.missingGroups.empty(), "an unidentified board might be the one this Patch wants");
    QVERIFY(report.undecided());
    QVERIFY(report.playable());
}

void TestExpansionCompatibility::aDisabledToneIsCountedButDoesNotMakeAPatchUnplayable()
{
    // Build the case rather than hunting the fixture for it: a Patch whose only
    // expansion Tone is switched off.
    auto patch = patchUsingGroup(7);
    ExpansionProfile profile;
    QVERIFY(profile.setBoard(1, "World", 5));

    for (const auto tone : ToneIndex::all()) {
        const auto wave = patch.wave(tone);
        if (xpmodel::expansionWave(wave.groupTypeRaw, wave.groupId, wave.numberRaw)) {
            QVERIFY(patch.setRaw(tone, xpmodel::ToneParameter::ToneSwitch, 0));
        }
    }

    const auto report = library::analysePatch(patch, profile);
    // The requirement is still recorded — switching the Tone on later is a
    // normal edit and the musician deserves to know what it would need...
    QVERIFY(report.usesExpansion());
    QVERIFY(!report.missingGroups.empty());
    // ...but the Patch plays as it stands, so it is not called unplayable.
    QCOMPARE(report.missingEnabledTones(), 0);
    QVERIFY(report.playable());
    QVERIFY2(QString::fromStdString(report.summary()).contains(QStringLiteral("switched-off")),
             qPrintable(QString::fromStdString(report.summary())));
}

// ---------------------------------------------------------------------------

void TestExpansionCompatibility::reportsWhatAWholeBankNeeds()
{
    const auto groups = library::requiredExpansionGroups(m_patches);

    // Measured from the fixture: 192 of its 512 Tones are expansion references,
    // across exactly these five groups. Every one is a real SR-JV80 board
    // number — 97 is Experience III — which is what supports reading a group as
    // its board (ROLAND_XP60_PROTOCOL_FACTS.md §7).
    const std::set<int> expected{1, 5, 7, 14, 97};
    QCOMPARE(groups, expected);

    // Owning some of them is not owning all of them, and the analysis says which.
    ExpansionProfile profile;
    QVERIFY(profile.setBoard(1, "Pop", 1));
    QVERIFY(profile.setBoard(2, "World", 5));

    int unplayable = 0;
    std::set<int> missingAcrossBank;
    for (const auto& patch : m_patches) {
        const auto report = library::analysePatch(patch, profile);
        if (!report.playable()) {
            ++unplayable;
        }
        missingAcrossBank.insert(report.missingGroups.begin(), report.missingGroups.end());
    }
    const std::set<int> expectedMissing{7, 14, 97};
    QCOMPARE(missingAcrossBank, expectedMissing);
    QVERIFY2(unplayable > 0, "this bank genuinely needs boards that are not declared");
    QVERIFY2(unplayable < static_cast<int>(m_patches.size()), "and plenty of it plays regardless");
}

// ---------------------------------------------------------------------------
// Persistence and the Expansion Manager
// ---------------------------------------------------------------------------

void TestExpansionCompatibility::theProfileSurvivesBeingSavedAndReopened()
{
    QVERIFY2(QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE")), "this Qt build has no QSQLITE driver");
    library::LibraryDatabase db;
    QVERIFY2(db.open(QString::fromLatin1(library::LibraryDatabase::kInMemoryPath)), qPrintable(db.lastError()));

    ExpansionProfile profile;
    QVERIFY(profile.setBoard(1, "SR-JV80-05 World", 5));
    // Installed, but its wave group is not yet known — the state that must
    // survive a round trip rather than being defaulted to a plausible number.
    QVERIFY(profile.setBoard(3, "The one I have not identified", std::nullopt));
    QVERIFY(db.saveExpansionProfile(profile));

    const auto loaded = db.loadExpansionProfile();
    QVERIFY2(loaded.has_value(), qPrintable(db.lastError()));
    QCOMPARE(loaded->installedCount(), 2);
    QCOMPARE(QString::fromStdString(loaded->board(1).name), QStringLiteral("SR-JV80-05 World"));
    QCOMPARE(loaded->board(1).waveGroupId.value(), 5);
    QVERIFY(!loaded->board(3).name.empty());
    QVERIFY2(!loaded->board(3).waveGroupId.has_value(), "an unknown group stays unknown");
    QVERIFY(loaded->anyGroupUnknown());
    QVERIFY(loaded->board(2).name.empty());

    // Saving replaces the configuration rather than accumulating one.
    ExpansionProfile fewer;
    QVERIFY(fewer.setBoard(4, "Only this", 7));
    QVERIFY(db.saveExpansionProfile(fewer));
    const auto again = db.loadExpansionProfile();
    QVERIFY(again.has_value());
    QCOMPARE(again->installedCount(), 1);
    QCOMPARE(again->slotProviding(7).value(), 4);
}

// The honest alternative to a lookup table this project cannot write: read the
// group out of a Patch the musician made on their own instrument.
void TestExpansionCompatibility::learnsAWaveGroupFromThePatchOnScreen()
{
    services::PatchWorkspace workspace;
    presentation::ExpansionViewModel manager;
    manager.setWorkspace(&workspace);

    QVERIFY(manager.setBoard(2, QStringLiteral("The one with the sitar on it"), -1));
    QVERIFY(manager.anyGroupUnknown());
    // Nothing to learn from before a Patch is on screen, and it says why.
    QVERIFY(!manager.learnFromCurrentPatch(2));
    QVERIFY(manager.learnAdvice(2).contains(QStringLiteral("Fetch a Patch")));

    // The musician selected a wave from that board and fetched the Patch. Only
    // group 14 is unaccounted for in it.
    workspace.adopt(patchUsingGroup(14), services::PatchOrigin::temporary());
    QCOMPARE(manager.learnableGroups().size(), 1);
    QVERIFY(manager.learnAdvice(2).contains(QStringLiteral("wave group 14")));

    QVERIFY(manager.learnFromCurrentPatch(2));
    QCOMPARE(manager.profile().slotProviding(14).value(), 2);
    QVERIFY(!manager.anyGroupUnknown());

    // And the Patch that taught it now reports as playable rather than unknown.
    const auto view = manager.currentPatch();
    QVERIFY(view.value(QStringLiteral("usesExpansion")).toBool());
    QVERIFY(!view.value(QStringLiteral("undecided")).toBool());
    QVERIFY(view.value(QStringLiteral("playable")).toBool());

    // A slot with no board in it cannot learn anything.
    QVERIFY(!manager.learnFromCurrentPatch(4));
    QVERIFY(manager.learnAdvice(4).contains(QStringLiteral("Name the board")));
}

// Picking the first of several candidates would make every later verdict rest
// on a coin toss, so an ambiguous Patch teaches nothing.
void TestExpansionCompatibility::refusesToLearnFromAnAmbiguousPatch()
{
    services::PatchWorkspace workspace;
    presentation::ExpansionViewModel manager;
    manager.setWorkspace(&workspace);
    QVERIFY(manager.setBoard(1, QStringLiteral("Unidentified"), -1));

    // Find a fixture Patch whose Tones span two different expansion groups.
    const Xp60Patch* ambiguous = nullptr;
    for (const auto& patch : m_patches) {
        std::set<int> groups;
        for (const auto tone : ToneIndex::all()) {
            const auto wave = patch.wave(tone);
            if (const auto expansion = xpmodel::expansionWave(wave.groupTypeRaw, wave.groupId, wave.numberRaw)) {
                groups.insert(expansion->groupIdRaw);
            }
        }
        if (groups.size() > 1) {
            ambiguous = &patch;
            break;
        }
    }
    if (!ambiguous) {
        QSKIP("no fixture Patch spans two expansion groups");
    }

    workspace.adopt(*ambiguous, services::PatchOrigin::temporary());
    QVERIFY(manager.learnableGroups().size() > 1);
    QVERIFY2(!manager.learnFromCurrentPatch(1), "two candidates means nothing unambiguous to learn");
    QVERIFY(!manager.profile().board(1).waveGroupId.has_value());
    QVERIFY(manager.learnAdvice(1).contains(QStringLiteral("cannot tell which one")));
}

// The Wave Browser's Expansion tab reports *this* instrument: which of the
// musician's declared boards XP60Studio holds Roland's Waveform List for, and
// which it does not. Board-by-board, because the answer differs per board and a
// blanket "no expansion names" would now be false.
void TestExpansionCompatibility::theWaveBrowserNoteSeparatesTheTwoGaps()
{
    presentation::ExpansionViewModel manager;

    const auto empty = manager.browserNote();
    QVERIFY(empty.contains(QStringLiteral("No expansion boards declared")));

    // A board whose list is held: named, with Roland's own count.
    QVERIFY(manager.declareBoard(1, 1));
    const auto pop = manager.browserNote();
    QVERIFY(pop.contains(QStringLiteral("SR-JV80-01 Pop")));
    QVERIFY(pop.contains(QString::number(library::srJv80WaveCount(1))));
    QVERIFY(!pop.contains(QStringLiteral("Not on:")));

    // A board whose list is not held, and one whose group is not known yet:
    // both are things XP60Studio cannot name waves for, and it says which.
    QVERIFY(manager.declareBoard(2, 14));
    QVERIFY(manager.setBoard(3, QStringLiteral("The unlabelled one"), -1));
    const auto mixed = manager.browserNote();
    QVERIFY(mixed.contains(QStringLiteral("SR-JV80-01 Pop")));
    QVERIFY(mixed.contains(QStringLiteral("Not on:")));
    QVERIFY(mixed.contains(QStringLiteral("SR-JV80-14 Asia")));
    QVERIFY(mixed.contains(QStringLiteral("not known yet")));
    // EXP-D is empty and must not be listed as something the musician owns.
    QVERIFY(!mixed.contains(QStringLiteral("EXP-D")));
}

// "Never silently replace missing waves" is the rule this test exists to hold.
// The editor offers Find Replacement, Disable Tone and Keep Anyway; the first
// only opens the browser, and none of the three points a Tone at a wave the
// musician did not choose.
void TestExpansionCompatibility::offersThreeWaysOutOfAMissingWaveAndReplacesNothingItself()
{
    // No hardware and no transfer: the workflow under test is entirely local,
    // and none of its three actions is allowed to reach the instrument.
    services::DeviceSession session(std::make_unique<midi::LoopbackMidiTransport>());
    services::PatchWorkspace workspace;
    presentation::PatchEditorViewModel editor(session, workspace);

    library::ExpansionProfile profile;
    editor.setExpansionProfile(&profile);

    // A Patch needing a group, on an instrument with a board that answers for
    // something else — so "missing" is sayable.
    const auto& patch = patchUsingGroup(14);
    workspace.adopt(patch, services::PatchOrigin::temporary());
    QVERIFY(profile.setBoard(1, "something else", 3));
    editor.expansionProfileChanged();

    const int needing = editor.tonesNeedingAttention();
    QVERIFY2(needing > 0, "this fixture Patch needs a board the profile does not provide");

    int missingTone = 0;
    for (const auto& entry : editor.toneCompatibility()) {
        const auto map = entry.toMap();
        if (map.value(QStringLiteral("needsAttention")).toBool()) {
            missingTone = map.value(QStringLiteral("toneNumber")).toInt();
            QCOMPARE(map.value(QStringLiteral("status")).toString(), QStringLiteral("expansion-missing"));
            break;
        }
    }
    QVERIFY(missingTone > 0);

    // Find Replacement asks the screen to open the browser and touches nothing.
    QSignalSpy requested(&editor, &presentation::PatchEditorViewModel::replacementRequested);
    const auto before = workspace.working();
    QVERIFY(editor.findReplacementFor(missingTone));
    QCOMPARE(requested.count(), 1);
    QCOMPARE(requested.first().first().toInt(), missingTone);
    QCOMPARE(editor.selectedTone(), missingTone);
    QVERIFY2(workspace.working() == before, "asking to browse must not change the Patch");
    QVERIFY(!workspace.modified());
    QCOMPARE(editor.tonesNeedingAttention(), needing);

    // Disable Tone is an ordinary, undoable edit that leaves the wave alone.
    QVERIFY(editor.disableTone(missingTone));
    QVERIFY(workspace.modified());
    const auto toneIndex = ToneIndex::fromNumber(missingTone).value();
    QCOMPARE(workspace.working().wave(toneIndex).numberRaw, before.wave(toneIndex).numberRaw);
    QCOMPARE(workspace.working().wave(toneIndex).groupId, before.wave(toneIndex).groupId);
    QVERIFY(!workspace.working().toneEnabled(toneIndex));
    // The prompt is resolved, but the verdict is not rewritten: turning the
    // Tone back on is an ordinary edit and the board would still be missing, so
    // the row keeps saying so.
    QCOMPARE(editor.tonesNeedingAttention(), needing - 1);
    for (const auto& entry : editor.toneCompatibility()) {
        const auto map = entry.toMap();
        if (map.value(QStringLiteral("toneNumber")).toInt() == missingTone) {
            QCOMPARE(map.value(QStringLiteral("status")).toString(), QStringLiteral("expansion-missing"));
            QVERIFY(!map.value(QStringLiteral("enabled")).toBool());
            QVERIFY(!map.value(QStringLiteral("needsAttention")).toBool());
            QVERIFY2(!map.value(QStringLiteral("kept")).toBool(), "disabling is not dismissing");
        }
    }
    QVERIFY(workspace.canUndo());
    workspace.undo();
    QVERIFY(workspace.working().toneEnabled(toneIndex));
}

void TestExpansionCompatibility::keepingAToneAnywayChangesNothingAndIsForgottenWithThePatch()
{
    // No hardware and no transfer: the workflow under test is entirely local,
    // and none of its three actions is allowed to reach the instrument.
    services::DeviceSession session(std::make_unique<midi::LoopbackMidiTransport>());
    services::PatchWorkspace workspace;
    presentation::PatchEditorViewModel editor(session, workspace);
    library::ExpansionProfile profile;
    editor.setExpansionProfile(&profile);
    QVERIFY(profile.setBoard(1, "something else", 3));
    editor.expansionProfileChanged();

    workspace.adopt(patchUsingGroup(14), services::PatchOrigin::temporary());
    const int needing = editor.tonesNeedingAttention();
    QVERIFY(needing > 0);

    int toneNumber = 0;
    for (const auto& entry : editor.toneCompatibility()) {
        if (entry.toMap().value(QStringLiteral("needsAttention")).toBool()) {
            toneNumber = entry.toMap().value(QStringLiteral("toneNumber")).toInt();
            break;
        }
    }

    const auto before = workspace.working();
    editor.keepToneAnyway(toneNumber);
    // The prompt is gone; the Patch is byte for byte what it was, and the
    // verdict underneath is unchanged. Dismissing a warning is not fixing it.
    QCOMPARE(editor.tonesNeedingAttention(), needing - 1);
    QVERIFY(workspace.working() == before);
    QVERIFY(!workspace.modified());
    for (const auto& entry : editor.toneCompatibility()) {
        const auto map = entry.toMap();
        if (map.value(QStringLiteral("toneNumber")).toInt() == toneNumber) {
            QVERIFY(map.value(QStringLiteral("kept")).toBool());
            QCOMPARE(map.value(QStringLiteral("status")).toString(), QStringLiteral("expansion-missing"));
        }
    }

    // Changing one's mind is allowed.
    editor.reconsiderTone(toneNumber);
    QCOMPARE(editor.tonesNeedingAttention(), needing);
    editor.keepToneAnyway(toneNumber);

    // A different Patch is a different question, so the dismissal does not
    // follow the musician to it.
    workspace.adopt(patchUsingGroup(14), services::PatchOrigin::library(7));
    QCOMPARE(editor.tonesNeedingAttention(), needing);
}

// ---------------------------------------------------------------------------
// The SR-JV80 catalogue
//
// XP60Studio reads a Wave Group ID as the SR-JV80 board of that number. Roland
// documents no such mapping, so it is an inference — but every group in real
// user data is a real board number, and the boards' contents match the Patches
// using them (ROLAND_XP60_PROTOCOL_FACTS.md §7).
// ---------------------------------------------------------------------------

void TestExpansionCompatibility::everyGroupTheFixtureUsesIsARealBoardNumber()
{
    const auto groups = library::requiredExpansionGroups(m_patches);
    QVERIFY(!groups.empty());
    for (const int group : groups) {
        QVERIFY2(library::isKnownSrJv80Board(group),
                 qPrintable(QStringLiteral("group %1 is not an SR-JV80 board number").arg(group)));
    }

    // The two the evidence rests hardest on, and the one that used to be
    // mistaken for proof that the whole mapping was wrong.
    QCOMPARE(QString::fromStdString(*library::srJv80BoardName(5)), QStringLiteral("SR-JV80-05 World"));
    QCOMPARE(QString::fromStdString(*library::srJv80BoardName(14)), QStringLiteral("SR-JV80-14 Asia"));
    QCOMPARE(QString::fromStdString(*library::srJv80BoardName(97)), QStringLiteral("SR-JV80-97 Experience III"));

    // The series runs 01..19 and 96..99, which is why the field is 0..127 wide.
    QVERIFY(library::isKnownSrJv80Board(1));
    QVERIFY(library::isKnownSrJv80Board(19));
    QVERIFY(library::isKnownSrJv80Board(96));
    QVERIFY(library::isKnownSrJv80Board(99));
    QVERIFY(!library::isKnownSrJv80Board(20));
    QVERIFY(!library::isKnownSrJv80Board(0));
    QVERIFY(!library::srJv80BoardName(42).has_value());
}

// Naming what a Patch asks for is not the same as saying the instrument has it.
void TestExpansionCompatibility::namesABoardForAGroupWithoutClaimingItIsInstalled()
{
    // The group number leads; the board name is the inference resting on it.
    QCOMPARE(QString::fromStdString(library::describeWaveGroup(14)),
             QStringLiteral("wave group 14 (SR-JV80-14 Asia)"));
    // A group no board carries is not an error — the musician's instrument is
    // the authority, not this table.
    QCOMPARE(QString::fromStdString(library::describeWaveGroup(42)), QStringLiteral("wave group 42"));

    // Knowing the board's name changes no verdict: with nothing declared, a
    // Patch needing group 14 is still undecided, not missing and not playable.
    const auto report = library::analysePatch(patchUsingGroup(14), ExpansionProfile{});
    QVERIFY(report.usesExpansion());
    QVERIFY(report.undecided());
    QVERIFY(report.missingGroups.empty());
    // ...but the summary can now say which board it is asking for.
    QVERIFY(QString::fromStdString(report.summary()).contains(QStringLiteral("SR-JV80-14 Asia")));
}

void TestExpansionCompatibility::pickingABoardFillsInItsGroupAndLearningStillOverridesIt()
{
    services::PatchWorkspace workspace;
    presentation::ExpansionViewModel manager;
    manager.setWorkspace(&workspace);

    QVERIFY(!manager.knownBoards().isEmpty());
    QCOMPARE(manager.knownBoards().first().toMap().value(QStringLiteral("number")).toInt(), 1);

    // Choosing SR-JV80-14 records both its name and the group it is inferred to
    // answer for, so the musician does not have to know the number.
    QVERIFY(manager.declareBoard(2, 14));
    QCOMPARE(QString::fromStdString(manager.profile().board(2).name), QStringLiteral("SR-JV80-14 Asia"));
    QCOMPARE(manager.profile().board(2).waveGroupId.value(), 14);
    QVERIFY(!manager.anyGroupUnknown());

    // A number no board carries is refused rather than invented.
    QVERIFY(!manager.declareBoard(3, 42));
    QVERIFY(manager.profile().board(3).name.empty());

    // The inference is a default, not a fact about this instrument: evidence
    // from the musician's own XP-60 overrides it without argument.
    workspace.adopt(patchUsingGroup(97), services::PatchOrigin::temporary());
    QVERIFY(manager.declareBoard(1, 5));
    QCOMPARE(manager.profile().board(1).waveGroupId.value(), 5);
    QVERIFY(manager.learnFromCurrentPatch(1));
    QCOMPARE(manager.profile().board(1).waveGroupId.value(), 97);
    // ...and the name the catalogue supplied is left alone, because renaming
    // the musician's board is not this class's business.
    QCOMPARE(QString::fromStdString(manager.profile().board(1).name), QStringLiteral("SR-JV80-05 World"));

    // A musician can also just say their board answers to something else.
    QVERIFY(manager.setWaveGroup(2, 42));
    QCOMPARE(manager.profile().board(2).waveGroupId.value(), 42);
}

// Roland's own per-board Waveform Lists live in docs/XP60-References/SR-JV80/
// and are generated into the catalogue. Where one is held, a Tone's wave can be
// named; where it is not, the number stands alone rather than a guess.
void TestExpansionCompatibility::namesTheWaveItselfWhereRolandsListIsHeld()
{
    // Counts are Roland's own, from the Waveform List PDFs.
    QVERIFY(library::hasSrJv80WaveList(1));
    QCOMPARE(library::srJv80WaveCount(1), 154);
    QVERIFY(library::hasSrJv80WaveList(2));
    QCOMPARE(library::srJv80WaveCount(2), 174);

    // Roland numbers waves from 1; a Tone's raw byte is one less.
    QCOMPARE(QString::fromUtf8(library::srJv80WaveName(1, 17)->data(),
                               static_cast<qsizetype>(library::srJv80WaveName(1, 17)->size())),
             QStringLiteral("Clav 2A"));
    QCOMPARE(QString::fromUtf8(library::srJv80WaveName(2, 17)->data(),
                               static_cast<qsizetype>(library::srJv80WaveName(2, 17)->size())),
             QStringLiteral("Cb Sect Lp"));

    // Off the end of a list is not a name, and neither is a board with no list.
    QVERIFY(!library::srJv80WaveName(1, 155).has_value());
    QVERIFY(!library::srJv80WaveName(1, 0).has_value());
    QVERIFY(!library::hasSrJv80WaveList(14));
    QCOMPARE(library::srJv80WaveCount(14), 0);
    QVERIFY(!library::srJv80WaveName(14, 3).has_value());

    // The description degrades one step at a time as knowledge runs out, and
    // never fills a gap with something plausible.
    QCOMPARE(QString::fromStdString(library::describeExpansionWave(1, 16)),
             QString::fromUtf8("wave 17 \u201cClav 2A\u201d on SR-JV80-01 Pop"));
    QCOMPARE(QString::fromStdString(library::describeExpansionWave(14, 2)),
             QStringLiteral("wave 3 on SR-JV80-14 Asia"));
    QCOMPARE(QString::fromStdString(library::describeExpansionWave(42, 2)),
             QStringLiteral("wave 3 of wave group 42"));
}

// The strongest check available without hardware, and it uses only Roland
// documents: every Tone in a real user bank that points at a board whose
// Waveform List we hold must land on a wave that list actually has. If the
// group-to-board reading were wrong, or the wave numbering off by one, this
// would fail.
void TestExpansionCompatibility::everyFixtureReferenceToABoardWeHoldResolvesToARealWave()
{
    int checked = 0;
    for (const auto& patch : m_patches) {
        for (const auto tone : ToneIndex::all()) {
            const auto wave = patch.wave(tone);
            const auto expansion = xpmodel::expansionWave(wave.groupTypeRaw, wave.groupId, wave.numberRaw);
            if (!expansion || !library::hasSrJv80WaveList(expansion->groupIdRaw)) {
                continue;
            }
            ++checked;
            const auto name = library::srJv80WaveName(expansion->groupIdRaw, expansion->numberRaw + 1);
            QVERIFY2(name.has_value(),
                     qPrintable(QStringLiteral("group %1 wave %2 is outside SR-JV80-%3's %4 waves")
                                    .arg(expansion->groupIdRaw)
                                    .arg(expansion->numberRaw + 1)
                                    .arg(expansion->groupIdRaw, 2, 10, QLatin1Char('0'))
                                    .arg(library::srJv80WaveCount(expansion->groupIdRaw))));
            QVERIFY(!name->empty());
        }
    }
    QVERIFY2(checked > 0, "the fixture uses a board whose Waveform List this project holds");

    // Spot-checks a musician could confirm by eye: the Patch named for four
    // Clavs uses four Clav waves, and the one named for four organs uses the
    // four numbered 60's Organ waves. Coincidence does not produce that.
    std::set<QString> clavs;
    std::set<QString> organs;
    for (const auto& patch : m_patches) {
        const auto title = QString::fromStdString(patch.name().displayText()).trimmed();
        for (const auto tone : ToneIndex::all()) {
            const auto wave = patch.wave(tone);
            const auto expansion = xpmodel::expansionWave(wave.groupTypeRaw, wave.groupId, wave.numberRaw);
            if (!expansion || expansion->groupIdRaw != 1) {
                continue;
            }
            const auto name = library::srJv80WaveName(1, expansion->numberRaw + 1);
            if (!name) {
                continue;
            }
            const auto text = QString::fromUtf8(name->data(), static_cast<qsizetype>(name->size()));
            if (title == QLatin1String("Clav 1 x4")) {
                clavs.insert(text);
            }
            if (title == QLatin1String("60s Organ x4")) {
                organs.insert(text);
            }
        }
    }
    QCOMPARE(clavs.size(), std::size_t{4});
    for (const auto& n : clavs) {
        QVERIFY2(n.startsWith(QStringLiteral("Clav")), qPrintable(n));
    }
    QCOMPARE(organs.size(), std::size_t{4});
    for (const auto& n : organs) {
        QVERIFY2(n.contains(QStringLiteral("Organ")), qPrintable(n));
    }
}

// Find Replacement now has somewhere to land: an expansion wave can be chosen
// and assigned, not merely named. The three bytes move as one undo step, and a
// number the board does not have is refused rather than clamped.
void TestExpansionCompatibility::assigningAnExpansionWaveWritesTheThreeBytesTogether()
{
    services::DeviceSession session(std::make_unique<midi::LoopbackMidiTransport>());
    services::PatchWorkspace workspace;
    presentation::PatchEditorViewModel editor(session, workspace);
    library::ExpansionProfile profile;
    editor.setExpansionProfile(&profile);
    QVERIFY(profile.setBoard(1, "SR-JV80-01 Pop", 1));
    editor.expansionProfileChanged();

    workspace.adopt(internalOnlyPatch(), services::PatchOrigin::temporary());
    const auto tone = ToneIndex::tone2();

    QVERIFY(editor.useExpansionWaveInTone(2, 1, 17));
    const auto wave = workspace.working().wave(tone);
    QCOMPARE(wave.groupTypeRaw, xpmodel::kExpansionWaveGroupTypeRaw);
    QCOMPARE(wave.groupId, 1);
    // Roland prints from 1; the Tone carries one less.
    QCOMPARE(wave.numberRaw, 16);
    const auto reference = xpmodel::expansionWave(wave.groupTypeRaw, wave.groupId, wave.numberRaw);
    QVERIFY(reference.has_value());
    QCOMPARE(QString::fromUtf8(library::srJv80WaveName(1, reference->numberRaw + 1)->data(),
                               static_cast<qsizetype>(library::srJv80WaveName(1, reference->numberRaw + 1)->size())),
             QStringLiteral("Clav 2A"));

    // One undo step puts all three bytes back.
    QVERIFY(workspace.canUndo());
    workspace.undo();
    QCOMPARE(workspace.working().wave(tone).groupTypeRaw, internalOnlyPatch().wave(tone).groupTypeRaw);
    QCOMPARE(workspace.working().wave(tone).numberRaw, internalOnlyPatch().wave(tone).numberRaw);

    // Past the end of Roland's list for that board: refused, nothing written.
    const auto before = workspace.working();
    QVERIFY(!editor.useExpansionWaveInTone(2, 1, library::srJv80WaveCount(1) + 1));
    QVERIFY(!editor.useExpansionWaveInTone(2, 1, 0));
    QVERIFY(workspace.working() == before);

    // A group the field cannot hold is refused too.
    QVERIFY(!editor.useExpansionWaveInTone(2, 128, 1));
    QVERIFY(!editor.useExpansionWaveInTone(9, 1, 1));

    // A board with no Waveform List here can still be addressed — the field
    // limits are all that can honestly be enforced — but nothing in the UI
    // offers one, because the browser lists only boards it can name.
    QVERIFY(!library::hasSrJv80WaveList(14));
    QVERIFY(editor.useExpansionWaveInTone(2, 14, 200));
    QCOMPARE(workspace.working().wave(tone).groupId, 14);
    QCOMPARE(workspace.working().wave(tone).numberRaw, 199);
}

QTEST_MAIN(TestExpansionCompatibility)
#include "tst_expansion_compatibility.moc"
