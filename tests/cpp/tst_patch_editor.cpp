#include "support/FakeXp60.h"

#include "presentation/PatchEditorViewModel.h"
#include "xp60/Xp60Device.h"
#include "xpmodel/Xp60PatchLayout.h"
#include "xpmodel/Xp60Effects.h"
#include "presentation/ToneViewModel.h"
#include "services/PatchTransfer.h"
#include "services/PatchWorkspace.h"
#include "sounddna/PatchFeatureExtractor.h"
#include "sounddna/SoundDnaKnowledgeModel.h"

#include <QSignalSpy>
#include <QAbstractItemModelTester>
#include <QVariantMap>
#include <QtTest>

#include <chrono>
#include <memory>
#include <optional>
#include <utility>

using namespace xp60studio;
using namespace xp60studio::presentation;
using namespace xp60studio::roland;
using namespace xp60studio::testsupport;
using namespace xp60studio::xpmodel;
using namespace std::chrono_literals;

namespace {

const RolandAddress kTemp = temporaryPatchAddress();

sounddna::SoundDnaKnowledgeModel validatedDnaModel()
{
    sounddna::DimensionEvidence evidence{128, 6, 0.84, 0.76, 0.78, 10, true, true,
                                         0.82, 4.4, 0.90, 0.80};
    sounddna::DimensionModel body{
        "body", "Body", "test patches", {}, 0.0,
        {{"tone.1.tone_level", {{0.0, -1.0}, {1.0, 1.0}}, 0.5, 0.0, 1.0, true, 1.0, 1.0},
         {"tone.2.tone_level", {{0.0, -0.5}, {1.0, 0.5}}, 0.5, 0.0, 1.0, true, 1.0, 1.0}},
        {{"tone.1.tone_level", "tone.2.tone_level", 0.25}},
        {{-2.0, 0.0}, {0.0, 50.0}, {2.0, 100.0}}, evidence, false, {}};
    return {"test/1", std::string(sounddna::PatchFeatureExtractor::kSchemaVersion), {body}};
}

struct Fixture
{
    midi::LoopbackMidiTransport* transport = nullptr;
    std::unique_ptr<services::DeviceSession> session;
    std::unique_ptr<services::PatchTransfer> transfer;
    services::PatchWorkspace workspace;
    std::unique_ptr<PatchEditorViewModel> editor;
    std::unique_ptr<FakeXp60> device;
    protocol::TimePoint now{std::chrono::duration_cast<protocol::Clock::duration>(1000ms)};
    // The workspace groups a run of commits about the same parameter into one
    // undo step when they arrive close together. A frozen clock makes that
    // deterministic: without moving it, every edit in a test looks like one
    // continuous drag — which is what a drag test wants and what a test of two
    // deliberate edits must opt out of with `separateEdits()`.
    qint64 editClockMs = 1;

    explicit Fixture(int patchInTemporaryArea = 4,
                     std::optional<sounddna::SoundDnaKnowledgeModel> dnaModel = std::nullopt)
    {
        auto loopback = std::make_unique<midi::LoopbackMidiTransport>();
        loopback->addInput("in-1", "XP-60 IN");
        loopback->addOutput("out-1", "XP-60 OUT");
        transport = loopback.get();
        session = std::make_unique<services::DeviceSession>(std::move(loopback));
        session->setAutomaticTimeoutPolling(false);
        session->setClocks([this] { return now; }, {});
        workspace.setClockForTesting([this] { return editClockMs; });
        auto pacing = session->pacing();
        pacing.interMessageDelay = 0ms;
        session->setPacing(pacing);
        transfer = std::make_unique<services::PatchTransfer>(*session);
        editor = dnaModel
            ? std::make_unique<PatchEditorViewModel>(*session, workspace, transfer.get(), std::move(*dnaModel))
            : std::make_unique<PatchEditorViewModel>(*session, workspace, transfer.get());
        device = std::make_unique<FakeXp60>(temporaryAreaWith(patchInTemporaryArea));
        session->connectEndpoints("in-1", "out-1");
    }

    // Puts enough time between two edits that they are separate undo steps
    // rather than one adjustment.
    void separateEdits()
    {
        editClockMs += services::PatchWorkspace::kCoalesceWindowMs + 1;
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

    // Drives the fixture until `done` holds, or gives up after `limit`.
    //
    // Live audition is paced by real timers, so a fixed QTest::qWait is a bet
    // that the timer fires within it. That bet loses under load -- the whole
    // suite running in parallel was enough to make
    // liveAuditionRestoresSnapshotWhileKeepingLocalEdits fail once while
    // passing in isolation. Waiting on the condition keeps what the test
    // asserts while removing the dependency on how long the machine took.
    template <typename Predicate>
    bool pumpUntilTrue(Predicate done, std::chrono::milliseconds limit = std::chrono::seconds(5))
    {
        const auto deadline = std::chrono::steady_clock::now() + limit;
        while (!done()) {
            if (std::chrono::steady_clock::now() > deadline) {
                return false;
            }
            QCoreApplication::processEvents();
            const auto replies = device->exchange(*transport);
            for (const auto& reply : replies) {
                const auto bytes = reply.encode();
                transport->injectIncoming(midi::MidiByteSpan(bytes.data(), bytes.size()));
            }
            QCoreApplication::processEvents();
            QTest::qWait(5);
        }
        return true;
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
    void unvalidatedSoundDnaIsEvidenceGatedAndCannotMutateThePatch()
    {
        Fixture f;
        f.loadPatch();
        const auto before = f.editor->patch();
        QVERIFY(!f.editor->soundDnaAvailable());
        QVERIFY(f.editor->soundDnaDimensions().isEmpty());
        QVERIFY(f.editor->soundDnaStatusText().contains(QStringLiteral("evidence"), Qt::CaseInsensitive));
        f.editor->beginSoundDnaGesture();
        f.editor->previewSoundDnaTarget(QStringLiteral("warmth"), 80);
        f.editor->endSoundDnaGesture();
        QVERIFY(f.editor->patch() == before);
        QVERIFY(!f.editor->canUndo());
    }

    void soundDnaDragIsOneWorkspaceUndoAndKeepsItsDelegateStable()
    {
        Fixture f(4, validatedDnaModel());
        f.loadPatch();
        const auto before = f.editor->patch();
        const int score = f.editor->soundDnaDimensions().first().toMap().value("score").toInt();
        const int target = score > 50 ? score - 12 : score + 12;
        QSignalSpy dnaChanged(f.editor.get(), &PatchEditorViewModel::soundDnaChanged);
        f.editor->beginSoundDnaGesture();
        f.editor->previewSoundDnaTarget(QStringLiteral("body"), target);
        QVERIFY(f.editor->patch() != before);
        QCOMPARE(dnaChanged.count(), 0);
        f.editor->endSoundDnaGesture();
        QCOMPARE(dnaChanged.count(), 1);
        QVERIFY(f.editor->canUndo());
        f.editor->undo();
        QVERIFY(f.editor->patch() == before);
        QVERIFY(!f.editor->canUndo());
    }

    void soundDnaUsesTheExistingLiveTemporaryPatchPath()
    {
        Fixture f(4, validatedDnaModel());
        f.loadPatch();
        f.editor->armWrite();
        f.editor->startLiveAudition();
        f.pump();
        const int score = f.editor->soundDnaDimensions().first().toMap().value("score").toInt();
        const int target = score > 50 ? score - 12 : score + 12;
        f.editor->beginSoundDnaGesture();
        f.editor->previewSoundDnaTarget(QStringLiteral("body"), target);
        f.editor->endSoundDnaGesture();
        const auto edited = f.editor->patch();
        QVERIFY(f.pumpUntilTrue([&] {
            return patchFrom(f.device->memory(), temporaryPatchAddress()) == edited;
        }));
        f.editor->stopLiveAudition();
        QVERIFY(f.pumpUntilTrue([&] { return !f.editor->liveAudition(); }));
    }

    void routingFollowsSelectionHistoryAndABWithoutSending()
    {
        Fixture f;
        f.loadPatch();
        const auto original = f.editor->routing();
        const auto sent = f.device->dataSetsReceived();
        f.editor->setCommonRaw(CommonParameter::StructureType12, 0);
        f.editor->setToneRaw(ToneIndex::tone1(), ToneParameter::OutputAssign, 2);
        const auto direct = f.editor->routing();
        const auto edges = direct.value("edges").toList();
        QCOMPARE(edges.size(), 1);
        QCOMPARE(edges.first().toMap().value("to").toString(), QStringLiteral("direct"));
        f.editor->setComparing(true);
        QCOMPARE(f.editor->routing(), original);
        f.editor->setComparing(false);
        QCOMPARE(f.editor->routing(), direct);
        f.editor->undo();
        f.editor->redo();
        QCOMPARE(f.editor->routing(), direct);
        QSignalSpy changed(f.editor.get(), &PatchEditorViewModel::patchChanged);
        f.editor->setSelectedTone(3);
        QVERIFY(!changed.isEmpty());
        QVERIFY(f.editor->routing().value("outputTone").toInt() >= 3);
        f.pump();
        QCOMPARE(f.device->dataSetsReceived(), sent);
    }
    void efxNamesAndStructureRoutingFollowTheRolandManual()
    {
        Fixture f;
        f.loadPatch();
        QCOMPARE(xpmodel::efxTypeName(0).value(), std::string_view("STEREO-EQ"));
        QCOMPARE(xpmodel::efxTypeName(39).value(), std::string_view("CHORUS/FLANGER"));
        QVERIFY(!xpmodel::efxTypeName(40));
        f.editor->setCommonRaw(CommonParameter::EfxType, 10);
        QCOMPARE(f.editor->mfxText(), QStringLiteral("HEXA-CHORUS"));
        f.editor->setSection(PatchEditorViewModel::Effects);
        auto* model = f.editor->sectionParameters();
        QCOMPARE(model->data(model->index(0), EditorParameterModel::ChoicesRole).toStringList().size(), 40);
        model->edit(QStringLiteral("common.efx_type"), 0, 39);
        QCOMPARE(f.editor->patch().raw(CommonParameter::EfxType), 39);
        QCOMPARE(f.editor->mfxText(), QStringLiteral("CHORUS/FLANGER"));
        f.editor->setCommonRaw(CommonParameter::StructureType12, 1);
        f.editor->setSelectedTone(1);
        f.editor->setToneRaw(ToneIndex::tone1(), ToneParameter::OutputAssign, 0);
        f.editor->setToneRaw(ToneIndex::tone2(), ToneParameter::OutputAssign, 2);
        QVERIFY(f.editor->routingSummary().contains("Tone 2 output settings"));
        QVERIFY(f.editor->routingSummary().contains("DIRECT"));
        f.editor->setCommonRaw(CommonParameter::StructureType12, 0);
        QVERIFY(f.editor->routingSummary().contains("Tone 1 routing"));
        QVERIFY(f.editor->routingSummary().contains("Dry sound to MIX"));
    }
    void liveAuditionAppliesSoloMuteAndABWithoutChangingLocalHistory()
    {
        Fixture f;
        f.loadPatch();
        for (int n = 1; n <= 4; ++n) f.tone(n)->setEnabled(true);
        const auto current = f.editor->patch();
        const auto before = patchFrom(f.device->memory(), temporaryPatchAddress());
        f.editor->armWrite();
        QVERIFY(f.editor->canStartLiveAudition());
        f.editor->startLiveAudition();
        f.pump();
        QVERIFY(f.editor->liveAudition());
        QVERIFY(!f.editor->canArmWrite());
        f.tone(2)->setSolo(true);
        f.tone(3)->setMute(true);
        QVERIFY(f.editor->patch() == current);
        QVERIFY(f.pumpUntilTrue([&] {
            return !patchFrom(f.device->memory(), temporaryPatchAddress()).toneEnabled(ToneIndex::tone1());
        }));
        auto received = patchFrom(f.device->memory(), temporaryPatchAddress());
        QVERIFY(!received.toneEnabled(ToneIndex::tone1()));
        QVERIFY(received.toneEnabled(ToneIndex::tone2()));
        QVERIFY(!received.toneEnabled(ToneIndex::tone3()));
        QVERIFY(!received.toneEnabled(ToneIndex::tone4()));
        f.tone(2)->setSolo(false);
        f.tone(3)->setMute(false);
        f.editor->setComparing(true);
        QVERIFY(f.pumpUntilTrue(
            [&] { return patchFrom(f.device->memory(), temporaryPatchAddress()) == before; }));
        f.editor->stopLiveAudition();
        QVERIFY(f.pumpUntilTrue([&] { return !f.editor->liveAudition(); }));
        QVERIFY(!f.editor->comparing());
        QVERIFY(f.editor->patch() == current);
        QVERIFY(patchFrom(f.device->memory(), temporaryPatchAddress()) == current);
    }

    void liveAuditionRestoresSnapshotWhileKeepingLocalEdits()
    {
        Fixture f;
        f.loadPatch();
        const auto original = f.editor->patch();
        f.tone(1)->setLevel(original.raw(ToneIndex::tone1(), ToneParameter::ToneLevel) == 42 ? 43 : 42);
        const auto edited = f.editor->patch();
        f.editor->armWrite();
        f.editor->startLiveAudition();
        f.pump();
        f.editor->restoreBeforeAudition();
        QVERIFY(f.pumpUntilTrue([&] { return !f.editor->liveAudition(); }));
        QVERIFY(f.editor->patch() == edited);
        QVERIFY(patchFrom(f.device->memory(), temporaryPatchAddress()) == original);
        QVERIFY(f.editor->modified());
    }

    // Use in Tone. The wave identifier mapping it relies on is hardware
    // evidence, not documentation: see docs/PHASE_4_WAVE_BROWSER.md and
    // src/xpmodel/Xp60WaveIdentifier.h.
    void useWaveInToneWritesAllThreeBytesAsOneEdit()
    {
        Fixture f;
        f.loadPatch();
        const auto before = f.editor->patch();
        const auto sent = f.device->dataSetsReceived();

        QVERIFY(f.editor->useWaveInTone(2, QStringLiteral("INT-B"), 193));

        const auto wave = f.editor->patch().wave(ToneIndex::tone2());
        QCOMPARE(wave.groupTypeRaw, 0);  // INT
        QCOMPARE(wave.groupId, 2);       // INT-B
        QCOMPARE(wave.numberRaw, 192);   // zero-based
        QCOMPARE(wave.numberDisplay, 193);

        // One edit, so one undo restores every byte it touched.
        f.editor->undo();
        QVERIFY(f.editor->patch() == before);
        f.editor->redo();
        QCOMPARE(f.editor->patch().wave(ToneIndex::tone2()).numberRaw, 192);

        // Local edit only: nothing was transmitted.
        f.pump();
        QCOMPARE(f.device->dataSetsReceived(), sent);
    }

    void useWaveInToneLeavesOtherTonesAndTheRestOfTheToneAlone()
    {
        Fixture f;
        f.loadPatch();
        const auto before = f.editor->patch();
        QVERIFY(f.editor->useWaveInTone(1, QStringLiteral("INT-A"), 36));

        const auto after = f.editor->patch();
        for (const auto tone : ToneIndex::all()) {
            if (tone == ToneIndex::tone1()) continue;
            QCOMPARE(after.wave(tone).groupTypeRaw, before.wave(tone).groupTypeRaw);
            QCOMPARE(after.wave(tone).groupId, before.wave(tone).groupId);
            QCOMPARE(after.wave(tone).numberRaw, before.wave(tone).numberRaw);
        }
        // Wave Gain shares the wave group but is not part of the selection.
        QCOMPARE(after.wave(ToneIndex::tone1()).gainRaw, before.wave(ToneIndex::tone1()).gainRaw);
        QCOMPARE(after.raw(ToneIndex::tone1(), ToneParameter::CutoffFrequency),
                 before.raw(ToneIndex::tone1(), ToneParameter::CutoffFrequency));
    }

    void useWaveInToneRefusesWhatTheInstrumentCannotSelect()
    {
        Fixture f;
        f.loadPatch();
        const auto before = f.editor->patch();

        // INT-B holds 193 waves; 194 does not exist. Nothing is clamped, and a
        // refused edit must not touch the patch or the undo history.
        QVERIFY(!f.editor->useWaveInTone(1, QStringLiteral("INT-B"), 194));
        QVERIFY(!f.editor->useWaveInTone(1, QStringLiteral("INT-A"), 256));
        QVERIFY(!f.editor->useWaveInTone(1, QStringLiteral("INT-A"), 0));
        QVERIFY(!f.editor->useWaveInTone(1, QStringLiteral("INT-C"), 1));
        QVERIFY(!f.editor->useWaveInTone(5, QStringLiteral("INT-A"), 1));
        QVERIFY(f.editor->patch() == before);
        QVERIFY(!f.editor->modified());
    }

    void useWaveInToneIsRefusedWhileComparing()
    {
        Fixture f;
        f.loadPatch();
        f.editor->setComparing(true);
        const auto before = f.editor->patch();
        QVERIFY(!f.editor->useWaveInTone(1, QStringLiteral("INT-A"), 36));
        QVERIFY(f.editor->patch() == before);
    }

    void useSelectedWaveAppliesTheBrowserSelectionToTheSelectedTone()
    {
        Fixture f;
        f.loadPatch();
        QVERIFY(!f.editor->canUseSelectedWave()); // nothing selected yet

        f.editor->setSelectedTone(3);
        f.editor->waves()->setQuery(QStringLiteral("Kalimba"));
        QVERIFY(f.editor->waves()->count() > 0);
        QSignalSpy availabilityChanged(f.editor.get(), &PatchEditorViewModel::canUseSelectedWaveChanged);
        f.editor->waves()->selectRow(0);
        QCOMPARE(availabilityChanged.count(), 1);
        QVERIFY(f.editor->canUseSelectedWave());

        const auto selected = f.editor->waves()->selected();
        QCOMPARE(selected.value("bank").toString(), QStringLiteral("INT-B"));
        QCOMPARE(selected.value("number").toInt(), 1);

        QVERIFY(f.editor->useSelectedWaveInTone());
        const auto wave = f.editor->patch().wave(ToneIndex::tone3());
        QCOMPARE(wave.groupId, 2);
        QCOMPARE(wave.numberRaw, 0);
    }

    void disclosureChangesDoNotAlterThePatchOrHistory()
    {
        Fixture f;
        f.loadPatch();
        const auto original = f.editor->patch();
        QCOMPARE(f.editor->disclosure(), PatchEditorViewModel::Design);
        for (int mode = 0; mode <= 2; ++mode) {
            f.editor->setDisclosure(mode);
            QCOMPARE(f.editor->disclosure(), mode);
            QVERIFY(!f.editor->modified());
            QVERIFY(!f.editor->canUndo());
        }
        f.editor->setDisclosure(42);
        QCOMPARE(f.editor->disclosure(), PatchEditorViewModel::Expert);
        QVERIFY(Xp60PatchDiff::compare(original, f.editor->patch()).identical());
    }

    void sectionParametersUseTheSelectedToneAndSharedHistory()
    {
        Fixture f;
        auto* model = f.editor->sectionParameters();
        QAbstractItemModelTester tester(model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QCOMPARE(model->rowCount(), 0);
        f.loadPatch();
        f.editor->setSection(PatchEditorViewModel::Filter);
        f.editor->setSelectedTone(3);
        const auto tone = ToneIndex::all()[2];
        const int original = f.editor->patch().raw(tone, ToneParameter::CutoffFrequency);
        const int edited = original == 10 ? 20 : 10;
        QSignalSpy reset(model, &QAbstractItemModel::modelReset);
        model->edit(QStringLiteral("tone.cutoff_frequency"), 3, edited);
        QCOMPARE(f.editor->patch().raw(tone, ToneParameter::CutoffFrequency), edited);
        QCOMPARE(reset.count(), 0); // typing must not destroy the focused delegate
        f.editor->undo();
        QCOMPARE(f.editor->patch().raw(tone, ToneParameter::CutoffFrequency), original);
        model->edit(QStringLiteral("tone.cutoff_frequency"), 3, 128);
        model->edit(QStringLiteral("tone.cutoff_frequency"), 1, edited); // stale tone identity
        QVERIFY(f.editor->canRedo());
        f.editor->redo();
        f.editor->setComparing(true);
        model->edit(QStringLiteral("tone.cutoff_frequency"), 3, 30);
        QCOMPARE(f.editor->patch().raw(tone, ToneParameter::CutoffFrequency), original);
        f.editor->setComparing(false);
        QCOMPARE(f.editor->patch().raw(tone, ToneParameter::CutoffFrequency), edited);
        QVERIFY(f.transport->sentMessages().empty()); // no implicit MIDI writes
    }

    void bothLfosExposeWaveformAndAllFourModulationDepths()
    {
        Fixture f;
        f.loadPatch();
        f.editor->setSection(PatchEditorViewModel::Motion);
        auto* model = f.editor->sectionParameters();
        for (int lfo = 1; lfo <= 2; ++lfo) {
            model->setGroup(lfo - 1);
            QCOMPARE(model->rowCount(), 12);
            QStringList ids;
            for (int row = 0; row < model->rowCount(); ++row) {
                ids.append(model->data(model->index(row), EditorParameterModel::IdRole).toString());
            }
            QVERIFY(ids.contains(QStringLiteral("tone.lfo%1_waveform").arg(lfo)));
            for (const auto& target : {"pitch", "filter", "level", "pan"}) {
                QVERIFY(ids.contains(QStringLiteral("tone.%1_lfo%2_depth").arg(QLatin1String(target)).arg(lfo)));
            }
            model->edit(QStringLiteral("tone.lfo%1_waveform").arg(lfo), 1, 3);
            const auto p = lfo == 1 ? ToneParameter::Lfo1Waveform : ToneParameter::Lfo2Waveform;
            QCOMPARE(f.editor->patch().raw(ToneIndex::all()[0], p), 3);
        }
    }

    void effectsAndExpertResolveParameterIndicesRatherThanByteOffsets()
    {
        Fixture f;
        f.loadPatch();
        f.editor->setSection(PatchEditorViewModel::Effects);
        auto* model = f.editor->sectionParameters();
        model->setGroup(2); // Reverb
        model->edit(QStringLiteral("common.reverb_type"), 0, 6);
        QCOMPARE(f.editor->patch().raw(CommonParameter::ReverbType), 6);
        model->setGroup(3); // selected Tone routing
        QCOMPARE(model->rowCount(), 4);
        model->edit(QStringLiteral("tone.output_assign"), 1, 1);
        QCOMPARE(f.editor->patch().raw(ToneIndex::all()[0], ToneParameter::OutputAssign), 1);

        auto* expert = f.editor->expertParameters();
        QAbstractItemModelTester tester(expert, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QCOMPARE(expert->rowCount(), 128);
        expert->setCommonScope(true);
        QCOMPARE(expert->rowCount(), 60); // twelve name bytes are edited as a string
        expert->edit(QStringLiteral("common.patch_tempo"), 0, 250); // two-byte nibble
        expert->edit(QStringLiteral("common.patch_level"), 0, 37); // follows nibble
        QCOMPARE(f.editor->patch().raw(CommonParameter::PatchTempo), 250);
        QCOMPARE(f.editor->patch().raw(CommonParameter::PatchLevel), 37);
        expert->setSearch(QStringLiteral("  REVERB TYPE  "));
        QCOMPARE(expert->rowCount(), 1);
        QCOMPARE(expert->data(expert->index(0), EditorParameterModel::ChoicesRole).toStringList().at(6), QStringLiteral("DELAY"));
        expert->setSearch(QStringLiteral("no such parameter"));
        QCOMPARE(expert->rowCount(), 0);
        expert->edit(QStringLiteral("common.patch_level"), 0, 60);
        QCOMPARE(f.editor->patch().raw(CommonParameter::PatchLevel), 37);
        QVERIFY(f.transport->sentMessages().empty());
    }

    void expertCannotCrossKeyOrVelocityBounds()
    {
        Fixture f;
        f.loadPatch();
        f.editor->setKeyRangeLower(20);
        f.editor->setKeyRangeUpper(90);
        f.editor->setVelocityLower(10);
        f.editor->setVelocityUpper(100);
        auto* expert = f.editor->expertParameters();
        expert->edit(QStringLiteral("tone.keyboard_range_lower"), 1, 91);
        expert->edit(QStringLiteral("tone.keyboard_range_upper"), 1, 19);
        expert->edit(QStringLiteral("tone.velocity_range_lower"), 1, 101);
        expert->edit(QStringLiteral("tone.velocity_range_upper"), 1, 9);
        QCOMPARE(f.editor->keyRangeLower(), 20);
        QCOMPARE(f.editor->keyRangeUpper(), 90);
        QCOMPARE(f.editor->velocityLower(), 10);
        QCOMPARE(f.editor->velocityUpper(), 100);
        f.editor->undo();
        QCOMPARE(f.editor->velocityUpper(), 127); // refused edits added no history
    }

    // -- Empty state ---------------------------------------------------------

    void withoutAPatchTheScreenSaysSoAndNothingIsEditable()
    {
        Fixture f;
        QVERIFY(!f.editor->hasPatch());
        // With no Patch there is nothing to say about either axis, so neither
        // badge is shown rather than one saying "NO PATCH" twice.
        QVERIFY(f.editor->studioBadgeText().isEmpty());
        QVERIFY(f.editor->deviceBadgeText().isEmpty());
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
        // A read from the temporary area is itself a verification of it — these
        // are the bytes the instrument just sent — but it does not put the Patch
        // in the library, and the two badges say exactly that.
        QCOMPARE(f.editor->deviceBadgeText(), QStringLiteral("XP TEMP"));
        QCOMPARE(f.editor->deviceBadgeTone(), QStringLiteral("success"));
        QCOMPARE(f.editor->studioBadgeText(), QStringLiteral("NOT IN LIBRARY"));
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
        // Unkept work outranks provenance: the urgent thing about an edited
        // Patch read from the instrument is that the edit is held nowhere.
        QCOMPARE(f.editor->studioBadgeText(), QStringLiteral("EDITED"));
        QCOMPARE(f.editor->studioBadgeTone(), QStringLiteral("warning"));
        // ...and the instrument no longer holds what is on screen.
        QCOMPARE(f.editor->deviceBadgeText(), QStringLiteral("NOT SENT"));
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

    void filterEnvelopeUsesTheFullUnsignedLevelRange()
    {
        Fixture f;
        f.loadPatch();
        f.editor->setSection(PatchEditorViewModel::Filter);
        f.editor->setEnvelopeStageRaw(0, true, 127);
        QCOMPARE(at(f.editor->envelopePoints(), 0).value(QStringLiteral("y")).toDouble(), 0.0);
        QCOMPARE(at(f.editor->envelopePoints(), 1).value(QStringLiteral("y")).toDouble(), 1.0);
        f.editor->moveEnvelopePoint(1, 0.25, 0.0);
        QCOMPARE(f.editor->patch().raw(ToneIndex::tone1(), ToneParameter::FilterEnvelopeLevel1), 0);
        f.editor->moveEnvelopePoint(1, 0.25, 1.0);
        QCOMPARE(f.editor->patch().raw(ToneIndex::tone1(), ToneParameter::FilterEnvelopeLevel1), 127);
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
        QCOMPARE(level, 127); // Parameter Address Map: Filter Envelope Level is 0..127
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

    void theKeybedWindowComesFromTheInstrumentAndWidensForTheRange()
    {
        Fixture f;
        f.loadPatch();
        f.editor->setSelectedTone(1);
        const auto keys = xp60::keybed();
        QCOMPARE(keys.noteCount, 61);
        QCOMPARE(keys.lowestNote, 36); // C2
        QCOMPARE(keys.highestNote, 96); // C7

        // A range inside the instrument's own keys leaves the window alone.
        f.editor->setKeyRangeLower(48);
        f.editor->setKeyRangeUpper(79);
        QCOMPARE(f.editor->keyboardWindowLower(), keys.lowestNote);
        QCOMPARE(f.editor->keyboardWindowUpper(), keys.highestNote);
        QVERIFY(!f.editor->keyRangeExceedsKeybed());
        QVERIFY(f.editor->keyRangeNote().contains(QStringLiteral("Within")));

        // A range past them widens the window to whole octaves, and the screen
        // says so rather than clamping a value the parameter allows.
        f.editor->setKeyRangeUpper(127);
        f.editor->setKeyRangeLower(0);
        QCOMPARE(f.editor->keyboardWindowLower(), 0);
        QCOMPARE(f.editor->keyboardWindowUpper(), 127);
        QVERIFY(f.editor->keyRangeExceedsKeybed());
        QVERIFY(f.editor->keyRangeNote().contains(QStringLiteral("MIDI")));
        QCOMPARE(f.editor->keyRangeLower(), 0);
        QCOMPARE(f.editor->keyRangeUpper(), 127);
    }

    void theKeybedWindowSnapsToWholeOctaves()
    {
        Fixture f;
        f.loadPatch();
        f.editor->setSelectedTone(1);
        f.editor->setKeyRangeLower(0);
        f.editor->setKeyRangeUpper(100); // C7 is 96, so the window must grow
        QCOMPARE(f.editor->keyboardWindowUpper(), 108); // next C above, C8
        f.editor->setKeyRangeUpper(96);
        f.editor->setKeyRangeLower(25); // between C1 (24) and C2 (36)
        QCOMPARE(f.editor->keyboardWindowLower(), 24);
    }

    void withoutAPatchTheKeybedWindowIsTheInstrumentsOwn()
    {
        Fixture f;
        const auto keys = xp60::keybed();
        QCOMPARE(f.editor->keyboardWindowLower(), keys.lowestNote);
        QCOMPARE(f.editor->keyboardWindowUpper(), keys.highestNote);
        QVERIFY(!f.editor->keyRangeExceedsKeybed());
        QVERIFY(f.editor->keyRangeNote().isEmpty());
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
        f.separateEdits();
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

    void rejectedEditsPreserveRedoAndFullUndoHistory()
    {
        Fixture f;
        f.loadPatch();
        f.tone(1)->setLevel(10);
        f.editor->undo();
        f.tone(1)->setLevel(128);
        f.editor->setCommonRaw(CommonParameter::PatchLevel, 128);
        QVERIFY(f.editor->canRedo());
        f.editor->redo();
        QCOMPARE(f.tone(1)->level(), 10);
        for (int i = 1; i <= 80; ++i) {
            f.separateEdits();
            f.tone(1)->setLevel(i);
        }
        f.tone(1)->setLevel(-1);
        int steps = 0;
        while (f.editor->canUndo()) {
            f.editor->undo();
            ++steps;
        }
        QCOMPARE(steps, 64);
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
            f.separateEdits();
            f.tone(1)->setLevel(i % 128);
        }
        int steps = 0;
        while (f.editor->canUndo() && steps < 200) {
            f.editor->undo();
            ++steps;
        }
        QCOMPARE(steps, 64);
    }

    // -- Drags must not flood the undo history --------------------------------
    //
    // The bound above is what makes this dangerous. A drag commits once per
    // pointer sample; unbrokered, a second of movement pushes more than 64
    // steps and every earlier edit in the session is evicted out of the front
    // of the deque. These tests drive real drags and assert the history
    // survives them.

    void aKnobDragIsOneUndoStepAndLeavesEarlierHistoryIntact()
    {
        Fixture f;
        f.loadPatch();
        const auto original = f.editor->patch();

        // Three deliberate edits, well apart in time.
        f.separateEdits(); f.tone(1)->setLevel(10);
        f.separateEdits(); f.tone(2)->setLevel(20);
        f.separateEdits(); f.tone(3)->setLevel(30);
        const auto beforeDrag = f.editor->patch();

        // Now a drag: 200 samples on one parameter, a few milliseconds apart,
        // which is what a pointer actually produces.
        f.separateEdits();
        for (int i = 0; i < 200; ++i) {
            f.editClockMs += 4;
            f.tone(4)->setLevel(1 + i % 127);
        }
        QVERIFY(f.editor->patch() != beforeDrag);

        // One undo takes the whole drag back.
        QVERIFY(f.editor->canUndo());
        f.editor->undo();
        QVERIFY(f.editor->patch() == beforeDrag);

        // And the three earlier edits are still reachable.
        QVERIFY(f.editor->canUndo());
        f.editor->undo();
        QCOMPARE(f.tone(3)->level(), original.raw(ToneIndex::tone3(), ToneParameter::ToneLevel));
        QVERIFY(f.editor->canUndo());
        f.editor->undo();
        QVERIFY(f.editor->canUndo());
        f.editor->undo();
        QVERIFY(f.editor->patch() == original);
        QVERIFY(!f.editor->canUndo());
    }

    void anEnvelopeDragIsOneStepThoughItWritesATimeAndALevel()
    {
        Fixture f;
        f.loadPatch();
        f.editor->setSection(PatchEditorViewModel::Filter);
        f.editor->setSelectedTone(1);
        const auto beforeDrag = f.editor->patch();

        // Each sample writes two parameters. Keyed per parameter they would
        // alternate and each break the other's run; keyed per point they are
        // one adjustment.
        for (int i = 0; i < 200; ++i) {
            f.editClockMs += 4;
            f.editor->moveEnvelopePoint(1, 0.1 + 0.004 * i, 0.1 + 0.004 * i);
        }
        QVERIFY(f.editor->patch() != beforeDrag);
        QVERIFY(f.editor->canUndo());
        f.editor->undo();
        QVERIFY(f.editor->patch() == beforeDrag);
        QVERIFY(!f.editor->canUndo());
    }

    void aDraggedPointAndThenADifferentPointAreTwoSteps()
    {
        Fixture f;
        f.loadPatch();
        f.editor->setSection(PatchEditorViewModel::Filter);
        const auto original = f.editor->patch();

        for (int i = 0; i < 20; ++i) {
            f.editClockMs += 4;
            f.editor->moveEnvelopePoint(1, 0.2, 0.1 + 0.02 * i);
        }
        const auto afterFirst = f.editor->patch();
        for (int i = 0; i < 20; ++i) {
            f.editClockMs += 4;
            f.editor->moveEnvelopePoint(2, 0.5, 0.9 - 0.02 * i);
        }
        QVERIFY(f.editor->patch() != afterFirst);
        QVERIFY(f.editor->canUndo());
        f.editor->undo();
        QVERIFY(f.editor->patch() == afterFirst);
        QVERIFY(f.editor->canUndo());
        f.editor->undo();
        QVERIFY(f.editor->patch() == original);
    }

    void aDeclaredGestureGroupsADragHoweverSlowItIs()
    {
        Fixture f;
        f.loadPatch();
        const auto beforeDrag = f.editor->patch();

        // A gesture the view brackets explicitly does not depend on the timing
        // window at all: a musician who pauses mid-drag still gets one step.
        f.editor->beginEditGesture();
        QVERIFY(f.editor->editGestureActive());
        for (int i = 0; i < 20; ++i) {
            f.separateEdits();
            f.tone(1)->setLevel(1 + i);
        }
        f.editor->endEditGesture();
        QVERIFY(!f.editor->editGestureActive());

        QVERIFY(f.editor->canUndo());
        f.editor->undo();
        QVERIFY(f.editor->patch() == beforeDrag);
        QVERIFY(!f.editor->canUndo());
    }

    void aDeclaredGestureThatEndsWhereItStartedRecordsNothing()
    {
        Fixture f;
        f.loadPatch();
        const auto original = f.editor->patch();
        const int level = f.tone(1)->level();

        f.editor->beginEditGesture();
        for (int i = 0; i < 10; ++i) {
            f.separateEdits();
            f.tone(1)->setLevel(1 + i);
        }
        f.tone(1)->setLevel(level);
        f.editor->endEditGesture();

        QVERIFY(f.editor->patch() == original);
        QVERIFY(!f.editor->canUndo());
        QVERIFY(!f.editor->modified());
    }

    void nestedGestureBracketsOnlyCountOnce()
    {
        Fixture f;
        f.loadPatch();
        const auto original = f.editor->patch();
        f.editor->beginEditGesture();
        f.editor->beginEditGesture();
        f.separateEdits(); f.tone(1)->setLevel(11);
        f.editor->endEditGesture();
        QVERIFY(f.editor->editGestureActive()); // the outer bracket is still open
        f.separateEdits(); f.tone(1)->setLevel(22);
        f.editor->endEditGesture();
        QVERIFY(!f.editor->editGestureActive());

        QVERIFY(f.editor->canUndo());
        f.editor->undo();
        QVERIFY(f.editor->patch() == original);
        QVERIFY(!f.editor->canUndo());
    }

    // -- A/B comparison -------------------------------------------------------

    void comparingShowsTheOriginalAndFreezesEditing()
    {
        Fixture f;
        f.loadPatch();
        const auto originalName = f.editor->patchName();
        const auto original = f.editor->patch();
        f.tone(1)->setLevel(3);
        f.editor->setPatchName(QStringLiteral("Edited"));
        QCOMPARE(f.editor->patchName(), QStringLiteral("Edited"));

        f.editor->setComparing(true);
        QCOMPARE(f.editor->studioBadgeText(), QStringLiteral("A · ORIGINAL"));
        QCOMPARE(f.editor->patchName(), originalName);
        QCOMPARE(f.tone(1)->level(), original.raw(ToneIndex::tone1(), ToneParameter::ToneLevel));
        QVERIFY(f.editor->patch() == original);
        QVERIFY(!f.editor->canUndo());
        QVERIFY(!f.editor->canArmWrite());
        f.editor->undo();
        f.editor->redo();
        f.editor->revertToOriginal();

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
        const auto original = f.editor->patch();
        f.tone(1)->setLevel(64);
        f.editor->armWrite();
        f.editor->writeToDevice();
        f.pumpUntil(services::PatchTransfer::State::Verified);

        QCOMPARE(f.transfer->state(), services::PatchTransfer::State::Verified);
        QCOMPARE(f.editor->writeTone(), QStringLiteral("success"));
        QCOMPARE(patchFrom(f.device->memory(), kTemp).raw(ToneIndex::tone1(), ToneParameter::ToneLevel), 64);
        QCOMPARE(f.tone(1)->level(), 64);
        QVERIFY(f.editor->canUndo());
        QCOMPARE(f.editor->deviceBadgeText(), QStringLiteral("XP TEMP"));
        f.editor->setComparing(true);
        QVERIFY(f.editor->patch() == original);
        f.editor->setComparing(false);
        f.editor->undo();
        QVERIFY(f.editor->patch() == original);
        // Undoing is a local edit like any other, so the instrument — which
        // still holds the written version — is no longer in step.
        QCOMPARE(f.editor->studioBadgeText(), QStringLiteral("NOT IN LIBRARY"));
        QCOMPARE(f.editor->deviceBadgeText(), QStringLiteral("NOT SENT"));
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
        QCOMPARE(f.tone(1)->level(), 64);
        QVERIFY(f.editor->modified());
        QVERIFY(f.editor->canUndo());
    }

    void disconnectRetainsPatchWithoutClaimingItIsOnTheDevice()
    {
        Fixture f;
        f.loadPatch();
        const auto original = f.editor->patch();
        f.session->disconnectEndpoints();
        QVERIFY(f.editor->patch() == original);
        // Editing continues with no instrument attached, and no claim about the
        // XP-60 survives the disconnection.
        QCOMPARE(f.editor->deviceBadgeText(), QStringLiteral("OFFLINE"));
        QCOMPARE(f.editor->studioBadgeText(), QStringLiteral("NOT IN LIBRARY"));
    }

    void editorCanCancelAWriteWithoutLosingLocalEdits()
    {
        Fixture f;
        f.loadPatch();
        f.tone(1)->setLevel(64);
        f.editor->armWrite();
        f.editor->writeToDevice();
        QVERIFY(f.editor->writeBusy());
        f.editor->cancelWrite();
        f.pump();
        QVERIFY(!f.editor->writeBusy());
        QCOMPARE(f.transfer->state(), services::PatchTransfer::State::Cancelled);
        QCOMPARE(f.tone(1)->level(), 64);
        QVERIFY(f.editor->canUndo());
    }

    // -- Signal flow ----------------------------------------------------------

    void theSignalFlowReadsFromThePatchAndSaysWhenALabelIsUnknown()
    {
        Fixture f;
        f.loadPatch();
        QVERIFY(!f.editor->structureText().isEmpty());
        // The EFX type list is not transcribed, so the index is shown as such.
        QVERIFY(!f.editor->mfxText().isEmpty());
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
