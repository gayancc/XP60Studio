#include "support/FakeXp60.h"

#include "presentation/PatchEditorViewModel.h"
#include "xpmodel/Xp60PatchLayout.h"
#include "presentation/ToneViewModel.h"
#include "services/PatchTransfer.h"

#include <QSignalSpy>
#include <QVariantMap>
#include <QtTest>

#include <chrono>
#include <memory>

using namespace xp60studio;
using namespace xp60studio::presentation;
using namespace xp60studio::roland;
using namespace xp60studio::testsupport;
using namespace xp60studio::xpmodel;
using namespace std::chrono_literals;

namespace {

const RolandAddress kTemp = temporaryPatchAddress();

struct Fixture
{
    midi::LoopbackMidiTransport* transport = nullptr;
    std::unique_ptr<services::DeviceSession> session;
    std::unique_ptr<services::PatchTransfer> transfer;
    std::unique_ptr<PatchEditorViewModel> editor;
    std::unique_ptr<FakeXp60> device;
    protocol::TimePoint now{std::chrono::duration_cast<protocol::Clock::duration>(1000ms)};

    explicit Fixture(int patchInTemporaryArea = 4)
    {
        auto loopback = std::make_unique<midi::LoopbackMidiTransport>();
        loopback->addInput("in-1", "XP-60 IN");
        loopback->addOutput("out-1", "XP-60 OUT");
        transport = loopback.get();
        session = std::make_unique<services::DeviceSession>(std::move(loopback));
        session->setAutomaticTimeoutPolling(false);
        session->setClocks([this] { return now; }, {});
        auto pacing = session->pacing();
        pacing.interMessageDelay = 0ms;
        session->setPacing(pacing);
        transfer = std::make_unique<services::PatchTransfer>(*session);
        editor = std::make_unique<PatchEditorViewModel>(*session, transfer.get());
        device = std::make_unique<FakeXp60>(temporaryAreaWith(patchInTemporaryArea));
        session->connectEndpoints("in-1", "out-1");
    }

    void pump(int rounds = 40)
    {
        for (int i = 0; i < rounds; ++i) {
            QCoreApplication::processEvents();
            const auto replies = device->exchange(*transport);
            for (const auto& reply : replies) {
                const auto bytes = reply.encode();
                transport->injectIncoming(midi::MidiByteSpan(bytes.data(), bytes.size()));
            }
            QCoreApplication::processEvents();
            if (replies.empty() && transport->sentMessages().empty() && session->pendingSendCount() == 0) {
                return;
            }
        }
    }

    void pumpUntil(services::PatchTransfer::State target, int rounds = 40)
    {
        for (int i = 0; i < rounds && transfer->state() != target; ++i) {
            QCoreApplication::processEvents();
            const auto replies = device->exchange(*transport);
            for (const auto& reply : replies) {
                const auto bytes = reply.encode();
                transport->injectIncoming(midi::MidiByteSpan(bytes.data(), bytes.size()));
            }
            QCoreApplication::processEvents();
        }
    }

    // Loads the temporary Patch into the editor, the way the Devices screen does.
    void loadPatch()
    {
        QVERIFY(session->fetchTemporaryPatch());
        pump();
        QCOMPARE(session->patchFetch().state, services::DeviceSession::PatchFetchState::Completed);
        QVERIFY(editor->hasPatch());
    }

    [[nodiscard]] ToneViewModel* tone(int number) const
    {
        return qobject_cast<ToneViewModel*>(editor->tones().at(number - 1).value<QObject*>());
    }
};

QVariantMap at(const QVariantList& list, int index)
{
    return list.at(index).toMap();
}

} // namespace

class PatchEditorTest : public QObject
{
    Q_OBJECT

private slots:
    // -- Empty state ---------------------------------------------------------

    void withoutAPatchTheScreenSaysSoAndNothingIsEditable()
    {
        Fixture f;
        QVERIFY(!f.editor->hasPatch());
        QCOMPARE(f.editor->stateBadgeText(), QStringLiteral("NO PATCH"));
        QCOMPARE(f.editor->stateBadgeTone(), QStringLiteral("neutral"));
        QVERIFY(f.editor->emptyStateMessage().contains(QStringLiteral("Devices")));
        QVERIFY(f.editor->patchName().isEmpty());
        QCOMPARE(f.editor->enabledToneCount(), 0);
        QVERIFY(!f.editor->envelopeAvailable());
        QVERIFY(f.editor->envelopePoints().isEmpty());
        QVERIFY(f.editor->toneSettings().isEmpty());
        QVERIFY(!f.editor->canWrite());
        QVERIFY(!f.editor->canArmWrite());

        // Four Tone cards exist regardless, so the layout does not jump.
        QCOMPARE(f.editor->tones().size(), 4);
        for (int number = 1; number <= 4; ++number) {
            QCOMPARE(f.tone(number)->toneNumber(), number);
            QVERIFY(!f.tone(number)->enabled());
        }
    }

    // -- Adopting a fetched patch -------------------------------------------

    void aFetchedPatchBecomesTheOriginalAndTheWorkingCopy()
    {
        Fixture f;
        QSignalSpy spy(f.editor.get(), &PatchEditorViewModel::patchChanged);
        f.loadPatch();
        QVERIFY(spy.count() > 0);

        const auto expected = patchFrom(f.device->memory(), kTemp);
        QCOMPARE(f.editor->patchName(), QString::fromStdString(expected.name().displayText()));
        QCOMPARE(f.editor->locationText(), QStringLiteral("TEMPORARY PATCH"));
        QVERIFY(f.editor->sourceText().contains(QStringLiteral("temporary")));
        QVERIFY(!f.editor->modified());
        QCOMPARE(f.editor->stateBadgeText(), QStringLiteral("ON XP-60"));
        QCOMPARE(f.editor->stateBadgeTone(), QStringLiteral("success"));
        QCOMPARE(f.editor->differenceSummary(), QStringLiteral("No local changes"));
        QCOMPARE(f.editor->enabledToneCount(), expected.enabledToneCount());
        QVERIFY(!f.editor->canUndo());
        QVERIFY(!f.editor->canRedo());
    }

    // -- Tone cards ----------------------------------------------------------

    void toneCardsReadTheirValuesFromTheWorkingPatch()
    {
        Fixture f;
        f.loadPatch();
        const auto patch = patchFrom(f.device->memory(), kTemp);

        for (const auto index : ToneIndex::all()) {
            auto* card = f.tone(index.number());
            QCOMPARE(card->enabled(), patch.toneEnabled(index));
            QCOMPARE(card->level(), patch.raw(index, ToneParameter::ToneLevel));
            QCOMPARE(card->pan(), patch.raw(index, ToneParameter::TonePan));
            // Coarse Tune is exposed in semitones, the unit the Octave control
            // steps in, not as the Roland raw byte.
            QCOMPARE(card->coarseTune(), patch.display(index, ToneParameter::CoarseTune));
            QCOMPARE(card->levelText(),
                     QString::fromStdString(patch.displayText(index, ToneParameter::ToneLevel)));
            // The mixer card writes centre pan as "C" (as the mockup does) and
            // defers to the parameter table everywhere else.
            const auto pan = QString::fromStdString(patch.displayText(index, ToneParameter::TonePan));
            QCOMPARE(card->panText(), pan == QStringLiteral("0") ? QStringLiteral("C") : pan);
            // Five points: the origin plus four stages.
            QCOMPARE(card->miniEnvelope().size(), 5);
        }
    }

    void panReadsAsLeftCentreRightAndLevelStaysARawValue()
    {
        Fixture f;
        f.loadPatch();
        auto* card = f.tone(1);

        card->setPan(64);
        QCOMPARE(card->pan(), 64);
        QCOMPARE(card->panText(), QStringLiteral("C"));
        card->setPan(0);
        QCOMPARE(card->panText(), QStringLiteral("L64"));
        card->setPan(127);
        QCOMPARE(card->panText(), QStringLiteral("63R"));

        // Deviation from the mockup, which shows "-2.3 dB": the Parameter
        // Address Map gives Tone Level as a raw 0..127 with no dB conversion,
        // so the raw value is shown rather than an invented one.
        card->setLevel(100);
        QCOMPARE(card->levelText(), QStringLiteral("100"));
        QVERIFY(!card->levelText().contains(QStringLiteral("dB")));
    }

    void editingAToneWritesThroughToThePatchAndMarksItModified()
    {
        Fixture f;
        f.loadPatch();
        auto* card = f.tone(2);
        const int before = card->level();
        const int target = before == 100 ? 90 : 100;

        QSignalSpy spy(card, &ToneViewModel::changed);
        card->setLevel(target);
        QVERIFY(spy.count() > 0);
        QCOMPARE(card->level(), target);
        QCOMPARE(f.editor->patch().raw(ToneIndex::tone2(), ToneParameter::ToneLevel), target);
        QVERIFY(f.editor->modified());
        QCOMPARE(f.editor->stateBadgeText(), QStringLiteral("MODIFIED"));
        QCOMPARE(f.editor->stateBadgeTone(), QStringLiteral("warning"));
        QVERIFY(f.editor->canUndo());
        // The summary counts differences per block; Xp60PatchDiff::describe()
        // is what names individual parameters.
        QCOMPARE(f.editor->differenceSummary(), QStringLiteral("1 parameter differs (Tone 2 1)"));
    }

    void aValueOutsideTheDocumentedRangeIsRefusedAndCostsNoUndoStep()
    {
        Fixture f;
        f.loadPatch();
        auto* card = f.tone(1);
        const int before = card->level();

        card->setLevel(999);
        QCOMPARE(card->level(), before);
        QVERIFY(!f.editor->modified());
        QVERIFY(!f.editor->canUndo());
    }

    void theOctaveControlMovesCoarseTuneByTwelveSemitones()
    {
        Fixture f;
        f.loadPatch();
        auto* card = f.tone(1);
        const int coarse = card->coarseTune();
        card->nudgeOctave(1);
        QCOMPARE(card->coarseTune(), coarse + 12);
        card->nudgeOctave(-1);
        QCOMPARE(card->coarseTune(), coarse);
    }

    void waveTextNamesTheIdentifierBecauseTheWaveformListIsNotTranscribed()
    {
        Fixture f;
        f.loadPatch();
        const auto patch = patchFrom(f.device->memory(), kTemp);
        auto* card = f.tone(1);
        const auto wave = patch.wave(ToneIndex::tone1());
        // Identifier, never an invented name.
        QVERIFY(card->waveText().contains(QString::number(wave.numberDisplay)));
        QVERIFY(!card->waveSourceText().isEmpty());
    }

    // -- Audition: solo/mute never touch patch data --------------------------

    void soloAndMuteAreAuditionStateAndLeaveThePatchUntouched()
    {
        Fixture f;
        f.loadPatch();
        const auto before = f.editor->patch();

        f.tone(1)->setSolo(true);
        QVERIFY(f.tone(1)->audible());
        QVERIFY(!f.tone(2)->audible());
        QVERIFY(!f.tone(3)->audible());

        f.tone(1)->setSolo(false);
        f.tone(3)->setMute(true);
        QVERIFY(f.tone(1)->audible());
        QVERIFY(!f.tone(3)->audible());

        QVERIFY(f.editor->patch() == before);
        QVERIFY(!f.editor->modified());
        QVERIFY(!f.editor->canUndo());
    }

    // -- Sections and envelopes ---------------------------------------------

    void eachSectionSelectsItsOwnEnvelope()
    {
        Fixture f;
        f.loadPatch();
        QCOMPARE(f.editor->sectionNames().size(), 5);

        f.editor->setSection(PatchEditorViewModel::Sound);
        QVERIFY(f.editor->envelopeAvailable());
        QVERIFY(f.editor->envelopeTitle().contains(QStringLiteral("PITCH")));

        f.editor->setSection(PatchEditorViewModel::Filter);
        QVERIFY(f.editor->envelopeTitle().contains(QStringLiteral("FILTER")));

        f.editor->setSection(PatchEditorViewModel::Amp);
        QVERIFY(f.editor->envelopeTitle().contains(QStringLiteral("LEVEL")));

        f.editor->setSection(PatchEditorViewModel::Motion);
        QVERIFY(!f.editor->envelopeAvailable());
        QVERIFY(f.editor->envelopePoints().isEmpty());
    }

    void theLevelEnvelopeHasFourTimesButOnlyThreeLevels()
    {
        Fixture f;
        f.loadPatch();
        f.editor->setSection(PatchEditorViewModel::Amp);
        const auto stages = f.editor->envelopeStages();
        QCOMPARE(stages.size(), 4);
        for (int i = 0; i < 4; ++i) {
            QCOMPARE(at(stages, i).value(QStringLiteral("hasLevel")).toBool(), i < 3);
        }
        const auto points = f.editor->envelopePoints();
        QCOMPARE(points.size(), 5);
        QCOMPARE(at(points, 4).value(QStringLiteral("hasLevel")).toBool(), false);
    }

    void envelopeUnitsAreStatedAsRawValuesRatherThanInventedSecondsOrDecibels()
    {
        Fixture f;
        const auto note = f.editor->envelopeUnitNote();
        QVERIFY(note.contains(QStringLiteral("0-127")));
        QVERIFY(note.contains(QStringLiteral("seconds")));
        QVERIFY(note.contains(QStringLiteral("dB")));
    }

    void draggingAnEnvelopePointWritesTheStageTimeAndLevel()
    {
        Fixture f;
        f.loadPatch();
        f.editor->setSection(PatchEditorViewModel::Filter);
        f.editor->setSelectedTone(1);

        // Point 1 is stage 0; drive its level to the top of the range.
        f.editor->moveEnvelopePoint(1, 0.25, 1.0);
        const int level = f.editor->patch().raw(ToneIndex::tone1(), ToneParameter::FilterEnvelopeLevel1);
        QCOMPARE(level, 126);
        QVERIFY(f.editor->modified());
    }

    void anEnvelopeStageCanAlsoBeSetExactly()
    {
        Fixture f;
        f.loadPatch();
        f.editor->setSection(PatchEditorViewModel::Amp);
        f.editor->setEnvelopeStageRaw(0, /*isLevel=*/false, 42);
        QCOMPARE(f.editor->patch().raw(ToneIndex::tone1(), ToneParameter::LevelEnvelopeTime1), 42);
        // The Level envelope has no fourth level, so that write is refused.
        f.editor->setEnvelopeStageRaw(3, /*isLevel=*/true, 10);
        QCOMPARE(f.editor->patch().raw(ToneIndex::tone1(), ToneParameter::LevelEnvelopeLevel3),
                 patchFrom(f.device->memory(), kTemp).raw(ToneIndex::tone1(), ToneParameter::LevelEnvelopeLevel3));
    }

    void theEnvelopeFollowsTheSelectedTone()
    {
        Fixture f;
        f.loadPatch();
        f.editor->setSection(PatchEditorViewModel::Filter);
        QSignalSpy spy(f.editor.get(), &PatchEditorViewModel::envelopeChanged);
        f.editor->setSelectedTone(3);
        QCOMPARE(f.editor->selectedTone(), 3);
        QVERIFY(spy.count() > 0);
        QVERIFY(f.editor->envelopeTitle().contains(QStringLiteral("TONE 3")));

        f.editor->setSelectedTone(9); // not a Tone: ignored
        QCOMPARE(f.editor->selectedTone(), 3);
    }

    // -- Ranges --------------------------------------------------------------

    void keyAndVelocityRangesStayOrdered()
    {
        Fixture f;
        f.loadPatch();
        f.editor->setSelectedTone(1);
        f.editor->setKeyRangeLower(40);
        f.editor->setKeyRangeUpper(80);
        QCOMPARE(f.editor->keyRangeLower(), 40);
        QCOMPARE(f.editor->keyRangeUpper(), 80);

        // Pushing the lower bound past the upper one clamps instead of
        // producing a range the instrument would reject.
        f.editor->setKeyRangeLower(100);
        QCOMPARE(f.editor->keyRangeLower(), 80);

        f.editor->setVelocityLower(30);
        f.editor->setVelocityUpper(20);
        QCOMPARE(f.editor->velocityUpper(), 30);

        QVERIFY(!f.editor->keyRangeLowerText().isEmpty());
        QVERIFY(!f.editor->keyRangeUpperText().isEmpty());
    }

    // -- Contextual settings -------------------------------------------------

    void toneSettingsExposeDocumentedRangesAndWriteBack()
    {
        Fixture f;
        f.loadPatch();
        const auto rows = f.editor->toneSettings();
        QCOMPARE(rows.size(), 5);
        const auto first = at(rows, 0);
        QVERIFY(!first.value(QStringLiteral("parameterId")).toString().isEmpty());
        QVERIFY(first.contains(QStringLiteral("minimum")));
        QVERIFY(first.contains(QStringLiteral("maximum")));

        const auto id = first.value(QStringLiteral("parameterId")).toString();
        const int minimum = first.value(QStringLiteral("minimum")).toInt();
        const int maximum = first.value(QStringLiteral("maximum")).toInt();
        const int raw = first.value(QStringLiteral("raw")).toInt();
        const int target = raw == maximum ? minimum : maximum;
        f.editor->setToneSetting(id, target);
        QCOMPARE(at(f.editor->toneSettings(), 0).value(QStringLiteral("raw")).toInt(), target);

        // An unknown identifier changes nothing.
        f.editor->setToneSetting(QStringLiteral("not-a-parameter"), 0);
        QCOMPARE(at(f.editor->toneSettings(), 0).value(QStringLiteral("raw")).toInt(), target);
    }

    // -- Undo / redo / revert -------------------------------------------------

    void undoAndRedoWalkTheEditHistory()
    {
        Fixture f;
        f.loadPatch();
        const auto original = f.editor->patch();

        f.tone(1)->setLevel(10);
        f.tone(1)->setLevel(20);
        QCOMPARE(f.tone(1)->level(), 20);
        QVERIFY(f.editor->canUndo());

        f.editor->undo();
        QCOMPARE(f.tone(1)->level(), 10);
        f.editor->undo();
        QVERIFY(f.editor->patch() == original);
        QVERIFY(!f.editor->modified());
        QVERIFY(f.editor->canRedo());

        f.editor->redo();
        QCOMPARE(f.tone(1)->level(), 10);
        f.editor->redo();
        QCOMPARE(f.tone(1)->level(), 20);
        QVERIFY(!f.editor->canRedo());
    }

    void aNewEditClearsTheRedoBranch()
    {
        Fixture f;
        f.loadPatch();
        f.tone(1)->setLevel(10);
        f.editor->undo();
        QVERIFY(f.editor->canRedo());
        f.tone(2)->setLevel(11);
        QVERIFY(!f.editor->canRedo());
    }

    void revertReturnsToTheFetchedPatchAndIsItselfUndoable()
    {
        Fixture f;
        f.loadPatch();
        const auto original = f.editor->patch();
        f.tone(1)->setLevel(7);
        f.editor->revertToOriginal();
        QVERIFY(f.editor->patch() == original);
        QVERIFY(!f.editor->modified());
        f.editor->undo();
        QCOMPARE(f.tone(1)->level(), 7);
    }

    void theUndoHistoryIsBounded()
    {
        Fixture f;
        f.loadPatch();
        for (int i = 1; i <= 80; ++i) {
            f.tone(1)->setLevel(i % 128);
        }
        int steps = 0;
        while (f.editor->canUndo() && steps < 200) {
            f.editor->undo();
            ++steps;
        }
        QCOMPARE(steps, 64);
    }

    // -- A/B comparison -------------------------------------------------------

    void comparingShowsTheOriginalAndFreezesEditing()
    {
        Fixture f;
        f.loadPatch();
        const auto originalName = f.editor->patchName();
        f.tone(1)->setLevel(3);
        f.editor->setPatchName(QStringLiteral("Edited"));
        QCOMPARE(f.editor->patchName(), QStringLiteral("Edited"));

        f.editor->setComparing(true);
        QCOMPARE(f.editor->stateBadgeText(), QStringLiteral("A · ORIGINAL"));
        QCOMPARE(f.editor->patchName(), originalName);

        // Edits are refused while the A side is on screen.
        f.tone(2)->setLevel(5);
        f.editor->setPatchName(QStringLiteral("Nope"));
        f.editor->setComparing(false);
        QCOMPARE(f.editor->patchName(), QStringLiteral("Edited"));
        QCOMPARE(f.tone(1)->level(), 3);
    }

    void patchNameEditingRejectsCharactersTheXp60CannotStore()
    {
        Fixture f;
        f.loadPatch();
        const auto before = f.editor->patchName();
        f.editor->setPatchName(QStringLiteral("café"));
        QCOMPARE(f.editor->patchName(), before);
        f.editor->setPatchName(QStringLiteral("Warm Pad"));
        QCOMPARE(f.editor->patchName().trimmed(), QStringLiteral("Warm Pad"));
    }

    // -- Write path -----------------------------------------------------------

    void writingIsRefusedUntilArmedAndArmingNeedsAVerifiedRead()
    {
        Fixture f;
        QVERIFY(!f.editor->canArmWrite());
        QVERIFY(!f.editor->canWrite());
        QVERIFY(f.editor->writeMessage().contains(QStringLiteral("Arm")));

        f.loadPatch();
        QVERIFY(f.editor->canArmWrite());
        QVERIFY(!f.editor->canWrite());

        f.editor->armWrite();
        QVERIFY(f.editor->writeArmed());
        QVERIFY(f.editor->canWrite());

        f.editor->disarmWrite();
        QVERIFY(!f.editor->writeArmed());
        QVERIFY(!f.editor->canWrite());

        // Disarmed, asking to write sends nothing.
        const auto sentBefore = f.device->dataSetsReceived();
        f.editor->writeToDevice();
        f.pump();
        QCOMPARE(f.device->dataSetsReceived(), sentBefore);
    }

    void anEditedPatchWritesAndVerifies()
    {
        Fixture f;
        f.loadPatch();
        f.tone(1)->setLevel(64);
        f.editor->armWrite();
        f.editor->writeToDevice();
        f.pumpUntil(services::PatchTransfer::State::Verified);

        QCOMPARE(f.transfer->state(), services::PatchTransfer::State::Verified);
        QCOMPARE(f.editor->writeTone(), QStringLiteral("success"));
        QCOMPARE(patchFrom(f.device->memory(), kTemp).raw(ToneIndex::tone1(), ToneParameter::ToneLevel), 64);
    }

    void aWriteThatDoesNotTakeIsReportedAsAMismatch()
    {
        Fixture f;
        f.loadPatch();
        f.tone(1)->setLevel(64);
                const auto level = *kTemp.plus(Xp60PatchLayout::toneOffset(ToneIndex::tone1())
                                       + xp60tables::descriptor(ToneParameter::ToneLevel).offset);
        f.device->corruptOnWrite(level, 0x01);
        f.editor->armWrite();
        f.editor->writeToDevice();
        f.pumpUntil(services::PatchTransfer::State::Mismatch);

        QCOMPARE(f.transfer->state(), services::PatchTransfer::State::Mismatch);
        QCOMPARE(f.editor->writeTone(), QStringLiteral("error"));
    }

    // -- Signal flow ----------------------------------------------------------

    void theSignalFlowReadsFromThePatchAndSaysWhenALabelIsUnknown()
    {
        Fixture f;
        f.loadPatch();
        QVERIFY(!f.editor->structureText().isEmpty());
        // The EFX type list is not transcribed, so the index is shown as such.
        QVERIFY(f.editor->mfxText().startsWith(QStringLiteral("Type ")));
        QVERIFY(f.editor->chorusText().startsWith(QStringLiteral("Level ")));
        QVERIFY(!f.editor->reverbText().isEmpty());
        QVERIFY(f.editor->outputText().startsWith(QStringLiteral("Level ")));
    }

    // -- Refetch --------------------------------------------------------------

    void afetchReplacesTheOriginalAndClearsTheHistory()
    {
        Fixture f;
        f.loadPatch();
        f.tone(1)->setLevel(9);
        QVERIFY(f.editor->canUndo());
        f.editor->setComparing(true);

        QVERIFY(f.session->fetchTemporaryPatch());
        f.pump();
        QVERIFY(!f.editor->canUndo());
        QVERIFY(!f.editor->canRedo());
        QVERIFY(!f.editor->comparing());
        QVERIFY(!f.editor->modified());
    }
};

QTEST_MAIN(PatchEditorTest)
#include "tst_patch_editor.moc"
