// The 128-destination User bank under construction.
//
// Everything the Bank Builder's control surface does resolves to one of these
// operations, so the rules that matter to a musician are checked here rather
// than in QML: placing preserves the source, replacing is one undo step,
// moving inside the bank swaps or moves, and undo restores exactly what was
// there — including whether the bank counted as saved.

#include "library/BankDraft.h"
#include "xpmodel/Xp60BankLocation.h"

#include <QTest>

using xp60studio::library::BankDraft;
using xp60studio::library::BankSlotContent;
using xp60studio::xpmodel::Xp60BankLocation;

namespace {

BankSlotContent patch(std::int64_t id, const char* name)
{
    BankSlotContent content;
    content.patchId = id;
    content.patchName = name;
    content.sourceName = "piano-bank.syx";
    content.sourceSlotLabel = "USER:007";
    return content;
}

int slotOf(const char* panelLabel)
{
    const auto location = Xp60BankLocation::fromPanelLabel(panelLabel);
    return location ? location->slotIndex() : -1;
}

} // namespace

class TestBankDraft : public QObject
{
    Q_OBJECT

private slots:
    void startsEmptyAndUnmodified();
    void placesAPatchAtAPanelDestination();
    void countsOccupancyPerBank();
    void replacingIsOneUndoStep();
    void movesAndSwapsInsideTheBank();
    void refusesImpossibleEdits();
    void undoAndRedoRestoreExactly();
    void aNewEditEndsTheRedoBranch();
    void savingClearsModifiedAndUndoBringsItBack();
    void resetLoadsAnArrangementAndDropsHistory();
    void aMissingPatchStillOccupiesItsDestination();
    void fillingManyDestinationsIsOneUndoStep();
    void refusesAFillThatIsNotWhollyLegal();
};

// Filling a bank from a source touches dozens of destinations. Undoing it has
// to put the whole arrangement back, not remove one placement at a time.
void TestBankDraft::fillingManyDestinationsIsOneUndoStep()
{
    BankDraft draft;
    draft.assign(slotOf("A11"), patch(1, "Kept"));

    std::vector<std::pair<int, BankSlotContent>> placements;
    for (int slotIndex = 8; slotIndex < 40; ++slotIndex) {
        placements.emplace_back(slotIndex, patch(100 + slotIndex, "Filled"));
    }
    QVERIFY(draft.assignAll(placements, "Fill from piano-bank.syx"));
    QCOMPARE(draft.occupiedCount(), 33);
    QCOMPARE(draft.lastActionLabel(), std::string{"Fill from piano-bank.syx"});
    QCOMPARE(draft.undoLabel(), std::string{"Fill from piano-bank.syx"});

    QVERIFY(draft.undo());
    // One step back is the whole fill, and the destination that was already
    // there survives it.
    QCOMPARE(draft.occupiedCount(), 1);
    QCOMPARE(draft.slot(slotOf("A11")).patchId, std::int64_t{1});

    QVERIFY(draft.redo());
    QCOMPARE(draft.occupiedCount(), 33);
    QCOMPARE(draft.slot(39).patchId, std::int64_t{139});

    // Filling with exactly what is already there changes nothing and adds no
    // undo step.
    const auto undoLabel = draft.undoLabel();
    QVERIFY(!draft.assignAll(placements, "Fill again"));
    QCOMPARE(draft.undoLabel(), undoLabel);
}

void TestBankDraft::refusesAFillThatIsNotWhollyLegal()
{
    BankDraft draft;
    draft.assign(slotOf("A11"), patch(1, "Kept"));

    // One impossible destination refuses the whole fill: half an arrangement
    // is worse than none, because nothing downstream would show what was left
    // out.
    std::vector<std::pair<int, BankSlotContent>> withBadSlot{
        {slotOf("A12"), patch(2, "Fine")},
        {128, patch(3, "Off the end")},
    };
    QVERIFY(!draft.assignAll(withBadSlot, "Fill"));
    QCOMPARE(draft.occupiedCount(), 1);
    QVERIFY(!draft.canRedo());

    std::vector<std::pair<int, BankSlotContent>> withEmptyContent{
        {slotOf("A12"), patch(2, "Fine")},
        {slotOf("A13"), BankSlotContent{}},
    };
    QVERIFY(!draft.assignAll(withEmptyContent, "Fill"));
    QCOMPARE(draft.occupiedCount(), 1);

    QVERIFY(!draft.assignAll({}, "Fill"));
    QVERIFY(draft.modified()); // from the first assign, and nothing since
}

void TestBankDraft::startsEmptyAndUnmodified()
{
    BankDraft draft;
    QCOMPARE(static_cast<int>(draft.destinations().size()), 128);
    QCOMPARE(draft.occupiedCount(), 0);
    QCOMPARE(draft.emptyCount(), 128);
    QVERIFY(!draft.modified());
    QVERIFY(!draft.canUndo());
    QVERIFY(!draft.canRedo());
    QVERIFY(!draft.savedBankId().has_value());
}

void TestBankDraft::placesAPatchAtAPanelDestination()
{
    BankDraft draft;
    const int a35 = slotOf("A35");
    QCOMPARE(a35, 20); // Patch 021

    QVERIFY(draft.assign(a35, patch(7, "WarmStrings")));
    QVERIFY(draft.isOccupied(a35));
    QCOMPARE(draft.slot(a35).patchId, static_cast<std::int64_t>(7));
    QCOMPARE(QString::fromStdString(draft.slot(a35).patchName), QStringLiteral("WarmStrings"));
    QCOMPARE(draft.occupiedCount(), 1);
    QVERIFY(draft.modified());
    QCOMPARE(QString::fromStdString(draft.undoLabel()), QStringLiteral("Place WarmStrings at A35"));

    // The same Patch can fill several destinations: a bank is an arrangement
    // of references, not a set of copies.
    QVERIFY(draft.assign(slotOf("B12"), patch(7, "WarmStrings")));
    QCOMPARE(draft.occupiedCount(), 2);
    QCOMPARE(draft.slot(slotOf("B12")).patchId, static_cast<std::int64_t>(7));
    QCOMPARE(draft.slot(a35).patchId, static_cast<std::int64_t>(7));
}

void TestBankDraft::countsOccupancyPerBank()
{
    BankDraft draft;
    QVERIFY(draft.assign(slotOf("A31"), patch(1, "One")));
    QVERIFY(draft.assign(slotOf("A35"), patch(2, "Two")));
    QVERIFY(draft.assign(slotOf("B88"), patch(3, "Three")));

    QCOMPARE(draft.occupiedInBank(0, 3), 2);
    QCOMPARE(draft.occupiedInBank(0, 1), 0);
    QCOMPARE(draft.occupiedInBank(1, 8), 1);
    QCOMPARE(draft.occupiedInBank(2, 1), -1); // no subgroup C
    QCOMPARE(draft.occupiedInBank(0, 9), -1);
}

void TestBankDraft::replacingIsOneUndoStep()
{
    BankDraft draft;
    const int slot = slotOf("A46");
    QVERIFY(draft.assign(slot, patch(1, "OldPad")));
    QVERIFY(draft.assign(slot, patch(2, "WarmStrings")));

    QCOMPARE(draft.occupiedCount(), 1);
    QCOMPARE(QString::fromStdString(draft.undoLabel()),
             QStringLiteral("Replace OldPad with WarmStrings at A46"));

    QVERIFY(draft.undo());
    QCOMPARE(draft.slot(slot).patchId, static_cast<std::int64_t>(1));
    QCOMPARE(draft.occupiedCount(), 1);
}

void TestBankDraft::movesAndSwapsInsideTheBank()
{
    BankDraft draft;
    const int from = slotOf("A11");
    const int empty = slotOf("A18");
    const int taken = slotOf("B11");
    QVERIFY(draft.assign(from, patch(1, "Piano")));
    QVERIFY(draft.assign(taken, patch(2, "Brass")));

    // Move into an empty destination.
    QVERIFY(draft.moveOrSwap(from, empty));
    QVERIFY(!draft.isOccupied(from));
    QCOMPARE(draft.slot(empty).patchId, static_cast<std::int64_t>(1));
    QCOMPARE(draft.occupiedCount(), 2);
    QCOMPARE(QString::fromStdString(draft.undoLabel()), QStringLiteral("Move Piano to A18"));

    // Move onto an occupied one: the two exchange places, nothing is lost.
    QVERIFY(draft.moveOrSwap(empty, taken));
    QCOMPARE(draft.slot(taken).patchId, static_cast<std::int64_t>(1));
    QCOMPARE(draft.slot(empty).patchId, static_cast<std::int64_t>(2));
    QCOMPARE(draft.occupiedCount(), 2);
    QCOMPARE(QString::fromStdString(draft.undoLabel()), QStringLiteral("Swap A18 and B11"));

    // And one undo puts both back.
    QVERIFY(draft.undo());
    QCOMPARE(draft.slot(empty).patchId, static_cast<std::int64_t>(1));
    QCOMPARE(draft.slot(taken).patchId, static_cast<std::int64_t>(2));
}

void TestBankDraft::refusesImpossibleEdits()
{
    BankDraft draft;
    QVERIFY(!draft.assign(-1, patch(1, "Piano")));
    QVERIFY(!draft.assign(128, patch(1, "Piano")));
    QVERIFY(!draft.assign(0, BankSlotContent{}));   // nothing to place
    QVERIFY(!draft.clear(0));                        // already empty
    QVERIFY(!draft.moveOrSwap(0, 1));                // nothing at the source
    QVERIFY(!draft.clearAll());                      // nothing to clear
    QVERIFY(!draft.modified());
    QVERIFY(!draft.canUndo());

    QVERIFY(draft.assign(0, patch(1, "Piano")));
    QVERIFY(!draft.moveOrSwap(0, 0));                // a destination onto itself
    QVERIFY(!draft.assign(0, patch(1, "Piano")));    // already exactly that
}

void TestBankDraft::undoAndRedoRestoreExactly()
{
    BankDraft draft;
    QVERIFY(draft.assign(slotOf("A11"), patch(1, "Piano")));
    QVERIFY(draft.assign(slotOf("A12"), patch(2, "Strings")));
    QVERIFY(draft.clear(slotOf("A11")));
    QCOMPARE(draft.occupiedCount(), 1);

    QVERIFY(draft.undo());
    QCOMPARE(draft.occupiedCount(), 2);
    QVERIFY(draft.canRedo());
    QCOMPARE(QString::fromStdString(draft.redoLabel()), QStringLiteral("Clear Piano from A11"));

    QVERIFY(draft.undo());
    QCOMPARE(draft.occupiedCount(), 1);
    QVERIFY(draft.undo());
    QCOMPARE(draft.occupiedCount(), 0);
    QVERIFY(!draft.canUndo());
    QVERIFY(!draft.undo());

    QVERIFY(draft.redo());
    QCOMPARE(draft.occupiedCount(), 1);
    QCOMPARE(draft.slot(slotOf("A11")).patchId, static_cast<std::int64_t>(1));
    QVERIFY(draft.redo());
    QVERIFY(draft.redo());
    QCOMPARE(draft.occupiedCount(), 1);
    QVERIFY(!draft.isOccupied(slotOf("A11")));
    QVERIFY(!draft.canRedo());
}

void TestBankDraft::aNewEditEndsTheRedoBranch()
{
    BankDraft draft;
    QVERIFY(draft.assign(slotOf("A11"), patch(1, "Piano")));
    QVERIFY(draft.undo());
    QVERIFY(draft.canRedo());

    QVERIFY(draft.assign(slotOf("A12"), patch(2, "Strings")));
    QVERIFY(!draft.canRedo());
}

void TestBankDraft::savingClearsModifiedAndUndoBringsItBack()
{
    BankDraft draft;
    QVERIFY(draft.assign(slotOf("A11"), patch(1, "Piano")));
    QVERIFY(draft.modified());

    draft.markSaved(42);
    QVERIFY(!draft.modified());
    QCOMPARE(draft.savedBankId().value(), static_cast<std::int64_t>(42));

    QVERIFY(draft.assign(slotOf("A12"), patch(2, "Strings")));
    QVERIFY(draft.modified());

    // Undoing back to the saved arrangement restores the saved state too, so
    // the surface does not keep claiming unsaved changes that no longer exist.
    QVERIFY(draft.undo());
    QVERIFY(!draft.modified());
    QCOMPARE(draft.savedBankId().value(), static_cast<std::int64_t>(42));

    draft.detachFromSavedBank();
    QVERIFY(!draft.savedBankId().has_value());
}

void TestBankDraft::resetLoadsAnArrangementAndDropsHistory()
{
    BankDraft draft;
    QVERIFY(draft.assign(slotOf("A11"), patch(1, "Piano")));

    std::vector<BankSlotContent> loaded(128);
    loaded[static_cast<std::size_t>(slotOf("B35"))] = patch(9, "Sax");
    draft.reset("Live Band Bank", loaded);

    QCOMPARE(QString::fromStdString(draft.name()), QStringLiteral("Live Band Bank"));
    QCOMPARE(draft.occupiedCount(), 1);
    QCOMPARE(draft.slot(slotOf("B35")).patchId, static_cast<std::int64_t>(9));
    QVERIFY(!draft.isOccupied(slotOf("A11")));
    QVERIFY(!draft.modified());
    QVERIFY(!draft.canUndo());
    QVERIFY(!draft.canRedo());

    // A short arrangement is padded to the full 128 rather than shortening the
    // bank.
    draft.reset("Short", std::vector<BankSlotContent>(3));
    QCOMPARE(static_cast<int>(draft.destinations().size()), 128);
}

void TestBankDraft::aMissingPatchStillOccupiesItsDestination()
{
    BankSlotContent gone;
    gone.patchName = "DeletedPad";
    gone.missing = true;
    QVERIFY(!gone.empty());

    std::vector<BankSlotContent> loaded(128);
    loaded[0] = gone;
    BankDraft draft;
    draft.reset("With a hole", loaded);

    QCOMPARE(draft.occupiedCount(), 1);
    QVERIFY(draft.isOccupied(0));
    // It can still be cleared or replaced deliberately; it is simply never
    // treated as free space.
    QVERIFY(draft.clear(0));
    QCOMPARE(draft.occupiedCount(), 0);
}

QTEST_APPLESS_MAIN(TestBankDraft)
#include "tst_bank_draft.moc"
