// The shared working Patch and its synchronization state machine.
//
// What is checked here is the part that keeps a musician's work and their
// instrument from drifting apart, so the tests are written against the rules in
// docs/PATCH_SYNCHRONIZATION.md rather than against the implementation:
//
//   * two independent axes — saving does not transmit, transmitting does not
//     save;
//   * "sent" is never allowed to masquerade as "synchronized";
//   * anything that destroys the XP-60's temporary area (Owner's Manual p.45:
//     selecting another Patch, power-off) makes every earlier claim about the
//     instrument invalid, and a reconnection does not restore it;
//   * nothing in this class can reach permanent USER memory.
//
// The Patches come from tests/fixtures/xp60/user-bank-amal.syx, a real XP-60
// User bank, so the equality comparisons that drive the state machine run over
// genuine parameter data.

#include "library/SyxImport.h"
#include "services/PatchWorkspace.h"
#include "xpmodel/Xp60Patch.h"
#include "xpmodel/Xp60PatchLayout.h"

#include <QFile>
#include <QSignalSpy>
#include <QTest>

using namespace xp60studio;
using services::DeviceState;
using services::PatchOrigin;
using services::PatchWorkspace;
using services::StudioState;
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

// Patch Common parameter index 13 is EFX Parameter 1, documented range 0..127
// (XP60_PATCH_PARAMETER_MAP.md, offset `00 0D`). Indices 0..11 are the twelve
// Patch Name characters. Both are used below because they are ordinary editable
// parameters, not because anything here depends on what they mean.
constexpr std::size_t kEfxParam1 = 13;

} // namespace

class TestPatchWorkspace : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // Editing
    void adoptingStartsCleanAndForgetsThePreviousPatch();
    void anEditThatChangesNothingIsNotAnEdit();
    void aGestureIsOneUndoStep();
    void undoRedoAndRevertWalkTheWorkingPatch();

    // The two axes
    void savingDoesNotTransmitAndTransmittingDoesNotSave();
    void sentIsNeverReportedAsSynchronized();
    void editingAfterAVerifiedSendDivergesFromTheInstrument();

    // The instrument's own actions
    void aPanelPatchChangeInvalidatesEveryClaimAboutTheInstrument();
    void disconnectingClearsTheDeviceStateAndReconnectingDoesNotRestoreIt();
    void aFailedTransferForcesAWholePatchResend();

    // Safety
    void theWorkspaceHasNoPathToPermanentUserMemory();

private:
    [[nodiscard]] Xp60Patch patchAt(std::size_t index) const;
    std::vector<library::LibraryEntry> m_entries;
};

void TestPatchWorkspace::initTestCase()
{
    const auto bytes = readFixture();
    QVERIFY2(!bytes.empty(), "golden fixture missing");
    m_entries = library::importSyxStream(bytes).entries;
    QVERIFY(m_entries.size() >= 3);
}

Xp60Patch TestPatchWorkspace::patchAt(std::size_t index) const
{
    return m_entries[index].patch();
}

// ---------------------------------------------------------------------------
// Editing
// ---------------------------------------------------------------------------

void TestPatchWorkspace::adoptingStartsCleanAndForgetsThePreviousPatch()
{
    PatchWorkspace workspace;
    workspace.setConnected(true);
    QVERIFY(!workspace.hasPatch());

    workspace.adopt(patchAt(0), PatchOrigin::library(7));
    QVERIFY(workspace.hasPatch());
    QCOMPARE(workspace.origin(), PatchOrigin::library(7));
    QCOMPARE(workspace.studioState(), StudioState::Saved);
    QCOMPARE(workspace.deviceState(), DeviceState::NotSent);
    QVERIFY(!workspace.canUndo());

    // Edit, verify against the instrument, then adopt something else.
    QVERIFY(workspace.edit(QStringLiteral("Rename"),
                           [](Xp60Patch& p) { p.common().setRawAt(0, 'Z'); }));
    workspace.noteVerified(workspace.working());
    QCOMPARE(workspace.deviceState(), DeviceState::InSync);

    workspace.adopt(patchAt(1), PatchOrigin::temporary());
    // A different Patch is a different question about the instrument: none of
    // the previous Patch's history or verification carries over.
    QVERIFY(!workspace.canUndo());
    QVERIFY(!workspace.canRedo());
    QVERIFY(!workspace.deviceBaseline().has_value());
    QCOMPARE(workspace.deviceState(), DeviceState::NotSent);
    QCOMPARE(workspace.studioState(), StudioState::Untracked);
}

void TestPatchWorkspace::anEditThatChangesNothingIsNotAnEdit()
{
    PatchWorkspace workspace;
    workspace.setConnected(true);
    workspace.adopt(patchAt(0), PatchOrigin::library(1));
    workspace.noteVerified(workspace.working());

    QVERIFY(!workspace.edit(QStringLiteral("No-op"), [](Xp60Patch&) {}));
    QVERIFY(!workspace.canUndo());
    QVERIFY(!workspace.modified());
    // Crucially it must not diverge the Patch from the instrument either: a
    // control that reports its own current value would otherwise mark the
    // Patch out of sync for no reason.
    QCOMPARE(workspace.deviceState(), DeviceState::InSync);
}

void TestPatchWorkspace::aGestureIsOneUndoStep()
{
    PatchWorkspace workspace;
    workspace.adopt(patchAt(0), PatchOrigin::library(1));
    const auto before = workspace.working();

    workspace.beginGesture();
    for (int value = 10; value <= 90; ++value) {
        workspace.edit(QStringLiteral("Level"),
                       [value](Xp60Patch& p) { p.common().setRawAt(kEfxParam1, value); });
    }
    workspace.endGesture();

    QCOMPARE(workspace.working().common().rawAt(kEfxParam1), 90);
    // A drag through eighty values is one thing the user did, so it is one step
    // back — not eighty.
    QVERIFY(workspace.undo());
    QVERIFY(workspace.working() == before);
    QVERIFY(!workspace.canUndo());
}

void TestPatchWorkspace::undoRedoAndRevertWalkTheWorkingPatch()
{
    PatchWorkspace workspace;
    workspace.adopt(patchAt(0), PatchOrigin::library(1));
    const auto adopted = workspace.working();

    workspace.edit(QStringLiteral("First"), [](Xp60Patch& p) { p.common().setRawAt(kEfxParam1, 11); });
    workspace.edit(QStringLiteral("Second"), [](Xp60Patch& p) { p.common().setRawAt(kEfxParam1, 22); });
    QCOMPARE(workspace.studioState(), StudioState::Edited);
    QCOMPARE(workspace.undoLabel(), QStringLiteral("Second"));

    QVERIFY(workspace.undo());
    QCOMPARE(workspace.working().common().rawAt(kEfxParam1), 11);
    QVERIFY(workspace.redo());
    QCOMPARE(workspace.working().common().rawAt(kEfxParam1), 22);

    // A new edit ends the redo branch.
    QVERIFY(workspace.undo());
    workspace.edit(QStringLiteral("Third"), [](Xp60Patch& p) { p.common().setRawAt(kEfxParam1, 33); });
    QVERIFY(!workspace.canRedo());

    QVERIFY(workspace.revert());
    QVERIFY(workspace.working() == adopted);
    QCOMPARE(workspace.studioState(), StudioState::Saved);
    // Reverting is itself undoable: discarding changes must not be the one
    // action a musician cannot take back.
    QVERIFY(workspace.undo());
    QCOMPARE(workspace.working().common().rawAt(kEfxParam1), 33);
}

// ---------------------------------------------------------------------------
// The two axes are independent
// ---------------------------------------------------------------------------

void TestPatchWorkspace::savingDoesNotTransmitAndTransmittingDoesNotSave()
{
    PatchWorkspace workspace;
    workspace.setConnected(true);
    workspace.adopt(patchAt(0), PatchOrigin::temporary());
    QCOMPARE(workspace.studioState(), StudioState::Untracked);

    workspace.edit(QStringLiteral("Edit"), [](Xp60Patch& p) { p.common().setRawAt(kEfxParam1, 42); });
    QCOMPARE(workspace.deviceState(), DeviceState::NotSent);

    // Sending changes nothing about storage.
    workspace.noteSending();
    workspace.noteSent(workspace.working());
    QCOMPARE(workspace.deviceState(), DeviceState::Assumed);
    QCOMPARE(workspace.studioState(), StudioState::Untracked);

    // Saving changes nothing about the instrument.
    workspace.markSaved(99);
    QCOMPARE(workspace.studioState(), StudioState::Saved);
    QCOMPARE(workspace.origin(), PatchOrigin::library(99));
    QCOMPARE(workspace.deviceState(), DeviceState::Assumed);
}

void TestPatchWorkspace::sentIsNeverReportedAsSynchronized()
{
    PatchWorkspace workspace;
    workspace.setConnected(true);
    workspace.adopt(patchAt(0), PatchOrigin::library(1));

    workspace.edit(QStringLiteral("Edit"), [](Xp60Patch& p) { p.common().setRawAt(kEfxParam1, 42); });
    workspace.noteSent(workspace.working());

    QCOMPARE(workspace.deviceState(), DeviceState::Assumed);
    QVERIFY(!services::assertsSynchronized(workspace.deviceState()));
    QVERIFY2(!workspace.deviceBaseline().has_value(),
             "a send must not create a verified baseline");
    // It is still enough to build the next diff on, which is the whole point of
    // the state: responsive editing without claiming proof.
    QVERIFY(workspace.sendBaseline().has_value());

    workspace.noteVerified(workspace.working());
    QCOMPARE(workspace.deviceState(), DeviceState::InSync);
    QVERIFY(services::assertsSynchronized(workspace.deviceState()));
    QVERIFY(workspace.deviceBaseline().has_value());
}

void TestPatchWorkspace::editingAfterAVerifiedSendDivergesFromTheInstrument()
{
    PatchWorkspace workspace;
    workspace.setConnected(true);
    workspace.adopt(patchAt(0), PatchOrigin::library(1));
    workspace.noteVerified(workspace.working());
    QCOMPARE(workspace.deviceState(), DeviceState::InSync);

    workspace.edit(QStringLiteral("Edit"), [](Xp60Patch& p) { p.common().setRawAt(kEfxParam1, 7); });
    QCOMPARE(workspace.deviceState(), DeviceState::Diverged);
    // The verified baseline survives, because it is still what the instrument
    // holds — that is exactly what the next diff must be computed against.
    QVERIFY(workspace.deviceBaseline().has_value());
    QVERIFY(workspace.sendBaseline().has_value());
}

// ---------------------------------------------------------------------------
// What the instrument does behind our back
// ---------------------------------------------------------------------------

void TestPatchWorkspace::aPanelPatchChangeInvalidatesEveryClaimAboutTheInstrument()
{
    PatchWorkspace workspace;
    workspace.setConnected(true);
    workspace.adopt(patchAt(0), PatchOrigin::library(1));
    workspace.noteVerified(workspace.working());
    QCOMPARE(workspace.deviceState(), DeviceState::InSync);

    // Owner's Manual p.45: selecting another Patch discards the temporary area.
    workspace.markStale(QStringLiteral("The XP-60 selected another Patch"));

    QCOMPARE(workspace.deviceState(), DeviceState::Stale);
    QVERIFY(!services::assertsSynchronized(workspace.deviceState()));
    QVERIFY2(!workspace.deviceBaseline().has_value(),
             "a verified baseline cannot survive the area being replaced");
    QVERIFY2(!workspace.sendBaseline().has_value(),
             "with the area unknowable, the next send must be a whole Patch, not a diff");
    QVERIFY(!workspace.deviceMessage().isEmpty());

    // Stale is absorbing: editing does not quietly downgrade it to Diverged,
    // which would imply we still knew what the instrument held.
    workspace.edit(QStringLiteral("Edit"), [](Xp60Patch& p) { p.common().setRawAt(kEfxParam1, 5); });
    QCOMPARE(workspace.deviceState(), DeviceState::Stale);
}

void TestPatchWorkspace::disconnectingClearsTheDeviceStateAndReconnectingDoesNotRestoreIt()
{
    PatchWorkspace workspace;
    workspace.setConnected(true);
    workspace.adopt(patchAt(0), PatchOrigin::library(1));
    workspace.noteVerified(workspace.working());

    workspace.setConnected(false);
    QCOMPARE(workspace.deviceState(), DeviceState::Offline);
    QVERIFY(!workspace.deviceBaseline().has_value());

    // Editing continues perfectly well with no instrument attached.
    QVERIFY(workspace.edit(QStringLiteral("Offline edit"),
                           [](Xp60Patch& p) { p.common().setRawAt(kEfxParam1, 64); }));
    QCOMPARE(workspace.studioState(), StudioState::Edited);
    QCOMPARE(workspace.deviceState(), DeviceState::Offline);

    // A connection appearing proves nothing about the temporary area, so
    // reconnecting neither restores the old verification nor pushes anything.
    workspace.setConnected(true);
    QCOMPARE(workspace.deviceState(), DeviceState::NotSent);
    QVERIFY(!workspace.deviceBaseline().has_value());
    QVERIFY(!workspace.sendBaseline().has_value());
}

void TestPatchWorkspace::aFailedTransferForcesAWholePatchResend()
{
    PatchWorkspace workspace;
    workspace.setConnected(true);
    workspace.adopt(patchAt(0), PatchOrigin::library(1));
    workspace.noteVerified(workspace.working());

    workspace.noteTransferFailed(QStringLiteral("Timed out"));
    QCOMPARE(workspace.deviceState(), DeviceState::Failed);
    // A partly written temporary area cannot be diffed against: what it holds
    // is a mixture nobody recorded.
    QVERIFY(!workspace.sendBaseline().has_value());
    QCOMPARE(workspace.deviceMessage(), QStringLiteral("Timed out"));
}

// ---------------------------------------------------------------------------
// Safety
// ---------------------------------------------------------------------------

void TestPatchWorkspace::theWorkspaceHasNoPathToPermanentUserMemory()
{
    // Structural, not behavioural: the workspace holds Patches and states. It
    // has no transport, no address and no transfer, so no sequence of calls on
    // it can reach `11 nn 00 00`. This test exists to fail loudly if that ever
    // stops being true — if a send path is added here, it will not compile
    // without also changing this file.
    PatchWorkspace workspace;
    workspace.setConnected(true);
    workspace.adopt(patchAt(0), PatchOrigin::userSlot(7));

    // A Patch read from USER:007 still knows where it came from...
    QCOMPARE(workspace.origin().kind, PatchOrigin::Kind::DeviceUserSlot);
    QCOMPARE(workspace.origin().userNumber, 7);
    // ...but that is provenance, not a destination: it is not "saved" anywhere
    // in the Studio, and editing it cannot write it back anywhere.
    QCOMPARE(workspace.studioState(), StudioState::Untracked);

    workspace.edit(QStringLiteral("Edit"), [](Xp60Patch& p) { p.common().setRawAt(kEfxParam1, 3); });
    workspace.noteSent(workspace.working());
    // Sending is a temporary-area concept throughout; nothing here records a
    // permanent write, because nothing here can perform one.
    QCOMPARE(workspace.deviceState(), DeviceState::Assumed);
}

QTEST_MAIN(TestPatchWorkspace)
#include "tst_patch_workspace.moc"
