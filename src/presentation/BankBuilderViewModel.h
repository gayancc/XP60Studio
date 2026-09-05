#pragma once

#include "library/BankDraft.h"
#include "library/PatchFingerprint.h"

#include <map>
#include <set>
#include "library/LibraryDatabase.h"
#include "services/PatchTransfer.h"
#include "services/PatchWorkspace.h"
#include "services/UserBankRead.h"
#include "services/UserMemoryWrite.h"

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <cstdint>
#include <optional>

namespace xp60studio::presentation {

// The Virtual XP-60 patch-selection panel, as state.
//
// The Bank Builder's surface is a control panel: a musician selects a
// SUBGROUP, a BANK and a NUMBER, and drags Patches from the source library
// onto the destinations that selection exposes. This view model holds exactly
// what that panel needs and nothing else — which subgroup/bank/number is
// selected, what the eight visible destinations currently hold, and what the
// instrument display should read.
//
// Every rule lives below it. The A/B, BANK, NUMBER to 001-128 bijection is
// xpmodel::Xp60BankLocation's; the arrangement, its undo history and its
// modified state are library::BankDraft's; persistence is
// library::LibraryDatabase's. QML computes none of it — no screen re-derives
// "bank 3 number 5 is patch 21", because doing that in two places is how the
// two get to disagree.
//
// The visible-destination and overview projections are deliberately plain
// QVariantList: eight and sixteen entries respectively, rebuilt on change.
// The 128-entry library behind them is *not* materialised — the source list
// stays on the virtualized LibraryListModel.
class BankBuilderViewModel : public QObject
{
    Q_OBJECT

    // Panel selection ------------------------------------------------------
    Q_PROPERTY(int subgroup READ subgroup WRITE selectSubgroup NOTIFY selectionChanged)
    Q_PROPERTY(int bank READ bank WRITE selectBank NOTIFY selectionChanged)
    Q_PROPERTY(int number READ number WRITE selectNumber NOTIFY selectionChanged)
    Q_PROPERTY(int currentSlotIndex READ currentSlotIndex NOTIFY selectionChanged)

    // The instrument display ----------------------------------------------
    Q_PROPERTY(QString subgroupLabel READ subgroupLabel NOTIFY selectionChanged)
    // "A35" — the panel identity, primary everywhere.
    Q_PROPERTY(QString panelLabel READ panelLabel NOTIFY selectionChanged)
    // "021" — the linear identity, always supporting information.
    Q_PROPERTY(QString linearLabel READ linearLabel NOTIFY selectionChanged)
    // "A · BANK 3 · 5"
    Q_PROPERTY(QString spokenLabel READ spokenLabel NOTIFY selectionChanged)
    Q_PROPERTY(QString currentPatchName READ currentPatchName NOTIFY bankChanged)
    Q_PROPERTY(QString currentSourceName READ currentSourceName NOTIFY bankChanged)
    Q_PROPERTY(QString currentSourceSlot READ currentSourceSlot NOTIFY bankChanged)
    // "EMPTY" | "ASSIGNED" | "MISSING"
    Q_PROPERTY(QString currentState READ currentState NOTIFY bankChanged)
    Q_PROPERTY(bool currentOccupied READ currentOccupied NOTIFY bankChanged)

    // The eight destinations the current SUBGROUP + BANK exposes.
    // Each entry: {slotIndex, number, panelLabel, linearLabel, patchName,
    //              sourceName, sourceSlot, occupied, missing, current}
    Q_PROPERTY(QVariantList visibleDestinations READ visibleDestinations NOTIFY bankChanged)
    // Sixteen entries, A1..A8 then B1..B8, each
    // {subgroup, bank, label, occupied, first, last, current, filled:[8 bools]}
    Q_PROPERTY(QVariantList overview READ overview NOTIFY bankChanged)
    // Which numbers of the current bank are filled, for the NUMBER row's LEDs.
    Q_PROPERTY(QVariantList numberOccupancy READ numberOccupancy NOTIFY bankChanged)
    // How full each of the eight banks in the current subgroup is, for the
    // BANK row's LEDs.
    Q_PROPERTY(QVariantList bankOccupancy READ bankOccupancy NOTIFY bankChanged)
    Q_PROPERTY(int subgroupOccupancyA READ subgroupOccupancyA NOTIFY bankChanged)
    Q_PROPERTY(int subgroupOccupancyB READ subgroupOccupancyB NOTIFY bankChanged)

    // The bank as a whole ---------------------------------------------------
    Q_PROPERTY(QString bankName READ bankName WRITE setBankName NOTIFY bankChanged)
    Q_PROPERTY(int occupiedCount READ occupiedCount NOTIFY bankChanged)
    Q_PROPERTY(int emptyCount READ emptyCount NOTIFY bankChanged)
    Q_PROPERTY(int missingCount READ missingCount NOTIFY bankChanged)
    // Destinations holding a sound that also sits somewhere else in this bank.
    // Reported, never acted on: filling four destinations with one Patch is a
    // legitimate thing to do, and so is keeping two copies of a sound. The
    // musician decides whether it was meant.
    Q_PROPERTY(int duplicateCount READ duplicateCount NOTIFY bankChanged)
    Q_PROPERTY(int slotCount READ slotCount CONSTANT)
    Q_PROPERTY(bool modified READ modified NOTIFY bankChanged)
    Q_PROPERTY(bool savedBefore READ savedBefore NOTIFY bankChanged)

    Q_PROPERTY(bool canUndo READ canUndo NOTIFY bankChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY bankChanged)
    Q_PROPERTY(QString undoLabel READ undoLabel NOTIFY bankChanged)
    Q_PROPERTY(QString redoLabel READ redoLabel NOTIFY bankChanged)
    // What just happened, for the surface's confirmation line. Cleared by
    // `acknowledge()`, never on a timer: a musician reads it when they look.
    Q_PROPERTY(QString lastAction READ lastAction NOTIFY actionChanged)
    Q_PROPERTY(QString lastActionTone READ lastActionTone NOTIFY actionChanged)

    // Audition ---------------------------------------------------------------
    // The existing armed-write path, reused unchanged: a Patch is auditioned by
    // sending it to the XP-60's **temporary** Patch area, which is not saved
    // across a power cycle. Permanent User memory is not reachable from here at
    // all, and the same rules apply as in the Editor — a verified temporary
    // read must have succeeded first, and arming is spent by one attempt.
    Q_PROPERTY(bool canAudition READ canAudition NOTIFY auditionChanged)
    Q_PROPERTY(bool auditionBusy READ auditionBusy NOTIFY auditionChanged)
    Q_PROPERTY(QString auditionState READ auditionState NOTIFY auditionChanged)
    Q_PROPERTY(QString auditionMessage READ auditionMessage NOTIFY auditionChanged)

    // Writing the bank into the XP-60's permanent USER memory --------------
    // The point of building a bank: getting it onto the keyboard. This is the
    // only destructive operation the application performs, so it is armed
    // separately from the audition path, states what it will overwrite, reads
    // each destination before writing it, and verifies every write.
    Q_PROPERTY(bool canWriteToUserMemory READ canWriteToUserMemory NOTIFY userWriteChanged)
    Q_PROPERTY(bool canArmUserWrite READ canArmUserWrite NOTIFY userWriteChanged)
    Q_PROPERTY(bool userWriteArmed READ userWriteArmed NOTIFY userWriteChanged)
    Q_PROPERTY(bool userWriteBusy READ userWriteBusy NOTIFY userWriteChanged)
    Q_PROPERTY(QString userWriteState READ userWriteState NOTIFY userWriteChanged)
    Q_PROPERTY(QString userWriteMessage READ userWriteMessage NOTIFY userWriteChanged)
    Q_PROPERTY(QString userWriteTone READ userWriteTone NOTIFY userWriteChanged)
    Q_PROPERTY(QString userWritePlan READ userWritePlan NOTIFY bankChanged)
    Q_PROPERTY(int userWriteCompleted READ userWriteCompleted NOTIFY userWriteChanged)
    Q_PROPERTY(int userWriteTotal READ userWriteTotal NOTIFY userWriteChanged)
    Q_PROPERTY(bool canRestoreUserMemory READ canRestoreUserMemory NOTIFY userWriteChanged)
    Q_PROPERTY(bool canRetryUserWrite READ canRetryUserWrite NOTIFY userWriteChanged)

    // Reading the instrument's USER bank ------------------------------------
    // Read-only, and the backup that makes writing safe to offer: the whole
    // 128-Patch bank comes into the library as one source and is arranged here
    // at the slots it was read from.
    Q_PROPERTY(bool canFetchBank READ canFetchBank NOTIFY bankFetchChanged)
    Q_PROPERTY(bool bankFetchBusy READ bankFetchBusy NOTIFY bankFetchChanged)
    Q_PROPERTY(QString bankFetchMessage READ bankFetchMessage NOTIFY bankFetchChanged)
    Q_PROPERTY(int bankFetchCompleted READ bankFetchCompleted NOTIFY bankFetchChanged)
    Q_PROPERTY(int bankFetchTotal READ bankFetchTotal NOTIFY bankFetchChanged)

    // Sections ----------------------------------------------------------------
    // The musician's own grouping of the 128 destinations. XP60Studio's
    // organisation, saved with the bank, never transmitted.
    // Each entry: {name, firstSlot, lastSlot, firstLabel, lastLabel, count,
    //              occupied, current}
    Q_PROPERTY(QVariantList sections READ sections NOTIFY bankChanged)
    Q_PROPERTY(QString currentSectionName READ currentSectionName NOTIFY bankChanged)

    // Multi-selection ---------------------------------------------------------
    // A marked set, deliberately separate from the panel's own selection: the
    // panel always names exactly one destination, as the instrument does, and
    // marking several for a bulk action must not fight with that.
    Q_PROPERTY(int selectionCount READ selectionCount NOTIFY selectionSetChanged)
    Q_PROPERTY(QVariantList selectedSlots READ selectedSlots NOTIFY selectionSetChanged)

    // Comparing two destinations ---------------------------------------------
    // A bank is where "are these two really the same sound?" gets asked, and
    // the duplicate marks make people ask it. Pinning one destination and
    // selecting another shows exactly what differs.
    Q_PROPERTY(bool comparing READ comparing NOTIFY comparisonChanged)
    Q_PROPERTY(QVariantMap comparison READ comparison NOTIFY comparisonChanged)

    // Saved banks: {id, name, occupied, missing, updated}
    Q_PROPERTY(QVariantList savedBanks READ savedBanks NOTIFY savedBanksChanged)
    // Import sources, so the source library can be opened one bank at a time:
    // {digest, name, patchCount}
    Q_PROPERTY(QVariantList sources READ sources NOTIFY savedBanksChanged)

public:
    explicit BankBuilderViewModel(QObject* parent = nullptr);

    // The database must outlive the view model. Passing nullptr leaves the
    // draft usable but unable to save or resolve Patch names.
    void setDatabase(library::LibraryDatabase* database);
    // Optional. Without it the surface is arrangement-only, which is what it
    // is when no instrument is connected.
    void setTransfer(services::PatchTransfer* transfer);
    // The shared working Patch. Optional: without it destinations read purely
    // from the draft. With it, a destination holding the Patch currently open
    // in the Editor shows the *working* name and says it is being edited, so a
    // rename made in the Editor is visible on the panel immediately.
    void setWorkspace(services::PatchWorkspace* workspace);
    // Optional. Without it the bank can be arranged, saved and exported but not
    // written to the instrument, which is what the screenshot harness gets.
    void setUserMemoryWrite(services::UserMemoryWrite* writer);

    [[nodiscard]] bool canWriteToUserMemory() const;
    [[nodiscard]] bool canArmUserWrite() const;
    [[nodiscard]] bool userWriteArmed() const;
    [[nodiscard]] bool userWriteBusy() const;
    [[nodiscard]] QString userWriteState() const;
    [[nodiscard]] QString userWriteMessage() const;
    [[nodiscard]] QString userWriteTone() const;
    // Exactly what pressing Write would overwrite, for the confirmation.
    [[nodiscard]] QString userWritePlan() const;
    [[nodiscard]] int userWriteCompleted() const;
    [[nodiscard]] int userWriteTotal() const;
    [[nodiscard]] bool canRestoreUserMemory() const;
    [[nodiscard]] bool canRetryUserWrite() const;

    Q_INVOKABLE bool armUserWrite();
    Q_INVOKABLE void disarmUserWrite();
    // Writes every occupied destination to the USER slot it occupies. Empty
    // destinations are skipped entirely rather than erased: whether a gap in a
    // bank means "wipe whatever the instrument has there" is not this
    // application's decision to make.
    Q_INVOKABLE bool writeBankToUserMemory();
    Q_INVOKABLE bool restoreUserMemory();
    // Writes the destinations the last run did not get to. Needs arming, like
    // any other write.
    Q_INVOKABLE bool retryUserWrite();
    Q_INVOKABLE void cancelUserWrite();

    // Optional. Without it the bank cannot be read off the instrument.
    void setUserBankRead(services::UserBankRead* reader);
    [[nodiscard]] bool canFetchBank() const;
    [[nodiscard]] bool bankFetchBusy() const;
    [[nodiscard]] QString bankFetchMessage() const;
    [[nodiscard]] int bankFetchCompleted() const;
    [[nodiscard]] int bankFetchTotal() const;
    // Reads all 128 USER Patches into the library as one source and arranges
    // them here at the slots they came from. Nothing is written to the
    // instrument; this is RQ1 only.
    Q_INVOKABLE bool fetchBankFromDevice();
    Q_INVOKABLE void cancelBankFetch();

    // Comparison -------------------------------------------------------------
    [[nodiscard]] bool comparing() const noexcept { return m_pinnedSlot >= 0; }
    // {pinnedLabel, pinnedName, otherLabel, otherName, identical, summary,
    //  differences: [{name, block, left, right}], shown, total}
    [[nodiscard]] QVariantMap comparison() const;
    // Pins `slotIndex` as the left side. Pinning the destination already
    // pinned clears the comparison.
    Q_INVOKABLE bool pinForComparison(int slotIndex);
    Q_INVOKABLE bool pinCurrentForComparison();
    Q_INVOKABLE void clearComparison();

    // Multi-selection --------------------------------------------------------
    [[nodiscard]] QVariantList sections() const;
    [[nodiscard]] QString currentSectionName() const;
    // Names the marked set's range, or the current destination when nothing is
    // marked. Refused for an empty name or a range overlapping a section that
    // already exists — a destination in two sections would make the rail lie.
    Q_INVOKABLE bool addSection(const QString& name);
    Q_INVOKABLE bool addSectionForRange(const QString& name, int firstSlot, int lastSlot);
    Q_INVOKABLE bool renameSection(int firstSlot, const QString& name);
    Q_INVOKABLE bool removeSection(int firstSlot);

    [[nodiscard]] int selectionCount() const noexcept { return static_cast<int>(m_selection.size()); }
    [[nodiscard]] QVariantList selectedSlots() const;
    [[nodiscard]] Q_INVOKABLE bool isSelected(int slotIndex) const;
    Q_INVOKABLE void toggleSelected(int slotIndex);
    // Marks every destination between the panel's current one and `slotIndex`,
    // inclusive, in linear order. This is what shift-clicking means.
    Q_INVOKABLE void selectRangeTo(int slotIndex);
    Q_INVOKABLE void selectAllOccupied();
    Q_INVOKABLE void clearSelection();
    // Empties every marked destination as one undo step, then unmarks them.
    // The Patches stay in the library; only the arrangement changes.
    Q_INVOKABLE bool clearSelectedSlots();

    // Opens the Patch at `slotIndex` in the Editor by adopting it into the
    // shared workspace. False when the destination is empty or its Patch is no
    // longer in the library; nothing is changed either way.
    Q_INVOKABLE bool editSlot(int slotIndex);
    Q_INVOKABLE bool editCurrent();

    [[nodiscard]] int subgroup() const;
    [[nodiscard]] int bank() const;
    [[nodiscard]] int number() const;
    [[nodiscard]] int currentSlotIndex() const;

    [[nodiscard]] QString subgroupLabel() const;
    [[nodiscard]] QString panelLabel() const;
    [[nodiscard]] QString linearLabel() const;
    [[nodiscard]] QString spokenLabel() const;
    [[nodiscard]] QString currentPatchName() const;
    [[nodiscard]] QString currentSourceName() const;
    [[nodiscard]] QString currentSourceSlot() const;
    [[nodiscard]] QString currentState() const;
    [[nodiscard]] bool currentOccupied() const;

    [[nodiscard]] QVariantList visibleDestinations() const;
    [[nodiscard]] QVariantList overview() const;
    [[nodiscard]] QVariantList numberOccupancy() const;
    [[nodiscard]] QVariantList bankOccupancy() const;
    [[nodiscard]] int subgroupOccupancyA() const;
    [[nodiscard]] int subgroupOccupancyB() const;

    [[nodiscard]] QString bankName() const;
    void setBankName(const QString& name);
    [[nodiscard]] int occupiedCount() const { return m_draft.occupiedCount(); }
    [[nodiscard]] int emptyCount() const { return m_draft.emptyCount(); }
    [[nodiscard]] int missingCount() const;
    [[nodiscard]] int duplicateCount() const;
    [[nodiscard]] int slotCount() const { return library::BankDraft::kSlotCount; }
    [[nodiscard]] bool modified() const { return m_draft.modified(); }
    [[nodiscard]] bool savedBefore() const { return m_draft.savedBankId().has_value(); }
    [[nodiscard]] bool canUndo() const { return m_draft.canUndo(); }
    [[nodiscard]] bool canRedo() const { return m_draft.canRedo(); }
    [[nodiscard]] QString undoLabel() const;
    [[nodiscard]] QString redoLabel() const;
    [[nodiscard]] QString lastAction() const { return m_lastAction; }
    [[nodiscard]] QString lastActionTone() const { return m_lastActionTone; }
    [[nodiscard]] bool canAudition() const;
    [[nodiscard]] bool auditionBusy() const;
    [[nodiscard]] QString auditionState() const;
    [[nodiscard]] QString auditionMessage() const;
    // Sends the Patch at `slotIndex` to the temporary Patch area and verifies
    // the read-back. False when refused; `auditionMessage` says why.
    Q_INVOKABLE bool auditionSlot(int slotIndex);
    Q_INVOKABLE bool auditionCurrent();

    [[nodiscard]] QVariantList savedBanks() const { return m_savedBanks; }
    [[nodiscard]] QVariantList sources() const { return m_sources; }

    // Panel controls --------------------------------------------------------
    // Each is what pressing that physical button does. Out-of-range values are
    // ignored rather than clamped.
    Q_INVOKABLE void selectSubgroup(int subgroup);
    Q_INVOKABLE void selectBank(int bank);
    Q_INVOKABLE void selectNumber(int number);
    // Moves the whole panel selection to a linear destination. Used by the
    // overview map and by a drop that lands outside the visible eight.
    Q_INVOKABLE void selectSlot(int slotIndex);

    // Placement -------------------------------------------------------------
    // Places the library Patch `patchId` at `slotIndex`, resolving its name
    // and provenance from the database. False when the Patch is not in the
    // library or the destination does not exist; nothing is changed either
    // way, and the source Patch is never modified.
    Q_INVOKABLE bool placePatch(int slotIndex, qint64 patchId);
    // The same, at the destination the panel currently names.
    Q_INVOKABLE bool placePatchAtCurrent(qint64 patchId);
    Q_INVOKABLE bool clearSlot(int slotIndex);
    // Move or swap inside the bank.
    Q_INVOKABLE bool moveSlot(int from, int to);
    Q_INVOKABLE bool clearAll();

    Q_INVOKABLE bool undo();
    Q_INVOKABLE bool redo();
    Q_INVOKABLE void acknowledge();

    // What a drop would do, in the instrument's own words, before it happens:
    // {panelLabel, linearLabel, action: "PLACE"|"REPLACE"|"MOVE"|"SWAP",
    //  occupant} — so the surface never has to decide for itself whether a
    // drop replaces something.
    Q_INVOKABLE QVariantMap dropPreview(int slotIndex, qint64 patchId, int movingFromSlot = -1) const;
    // One destination as a map, for a tooltip or an inspector.
    Q_INVOKABLE QVariantMap destinationAt(int slotIndex) const;
    // The library id at a destination, or 0. Used to start a target-to-target
    // drag and to open a Patch elsewhere.
    Q_INVOKABLE qint64 patchIdAt(int slotIndex) const;
    // "A35" for any slot index, so a drag ghost can name where it is going
    // without re-deriving the mapping.
    Q_INVOKABLE QString panelLabelFor(int slotIndex) const;
    Q_INVOKABLE QString linearLabelFor(int slotIndex) const;
    // The slot index for panel coordinates, or -1.
    Q_INVOKABLE int slotIndexFor(int subgroup, int bank, int number) const;

    // Banks -----------------------------------------------------------------
    // Starts again with 128 empty destinations. Refuses nothing: the caller is
    // responsible for confirming when `modified` is true.
    Q_INVOKABLE void newEmptyBank(const QString& name = QString());
    // Writes the arrangement as a new saved bank and keeps working on it.
    Q_INVOKABLE bool saveAsNewBank(const QString& name);
    // Writes over the bank this draft was loaded from or last saved as.
    Q_INVOKABLE bool saveBank();
    // Fills the draft from one import source, putting every Patch back at the
    // User slot it was read from. This is what "open the bank I imported" has
    // to mean: an imported `.syx` already says where each Patch lived, and
    // re-deriving that arrangement by hand across 128 destinations is exactly
    // the work this application exists to remove.
    //
    // It never guesses. A Patch whose provenance recorded no User number is
    // left unplaced and counted, rather than dropped into the first free
    // destination, and when two Patches claim one destination the first is
    // kept and the collision is reported. Destinations the source says nothing
    // about are left exactly as they are, so filling from a second source adds
    // to the bank instead of replacing it.
    //
    // The whole fill is one undo step. Returns
    // {ok, placed, unplaced, conflicts, sourceName, message}.
    Q_INVOKABLE QVariantMap fillFromSource(const QString& digest);

    Q_INVOKABLE bool loadBank(qint64 bankId);
    Q_INVOKABLE bool deleteBank(qint64 bankId);
    Q_INVOKABLE void refresh();

    // Every library id in slot order, empty destinations included as 0. This
    // is what an export or a transfer works from, so the caller decides what
    // an empty destination means rather than this class guessing.
    Q_INVOKABLE QVariantList arrangementIds() const;

    [[nodiscard]] QString lastError() const { return m_lastError; }

    // For tests and the screenshot harness: the draft itself.
    [[nodiscard]] const library::BankDraft& draft() const noexcept { return m_draft; }

Q_SIGNALS:
    void selectionChanged();
    void bankChanged();
    void actionChanged();
    void savedBanksChanged();
    void auditionChanged();
    void userWriteChanged();
    void bankFetchChanged();
    void comparisonChanged();
    void selectionSetChanged();
    // A device read added Patches, so any library model showing this database
    // needs to re-read.
    void libraryChanged();
    void errorOccurred(const QString& message);

private:
    [[nodiscard]] std::optional<library::BankSlotContent> contentFor(std::int64_t patchId) const;
    [[nodiscard]] std::vector<services::UserMemoryWrite::Destination> userWriteDestinations() const;
    void adoptFetchedBank();
    void refreshDuplicates();
    // Slot index -> the first destination holding the same sound, or -1. Kept
    // as a projection of the arrangement, recomputed when it changes.
    std::vector<int> m_duplicateOf;
    // Whether that match is literally the same library row, as opposed to a
    // different Patch whose parameters happen to be identical.
    std::vector<bool> m_duplicateIsSameEntry;
    // Fingerprints by library id. A stored Patch's fingerprint does not change
    // while it sits in the library, so this is cached across edits rather than
    // re-read from the database on every drag.
    mutable std::map<std::int64_t, library::PatchFingerprint> m_fingerprints;
    // The left side of a comparison, or -1. The right side is always whichever
    // destination the panel currently names, so comparing is done by walking
    // the panel rather than by picking from a list.
    int m_pinnedSlot = -1;
    // Sorted, so a bulk action happens in the order a musician reads the bank.
    std::set<int> m_selection;
    // Recorded on Patches read from the instrument, so provenance says which
    // device answered. Set alongside the reader.
    std::optional<roland::RolandDeviceId> m_deviceIdForProvenance;
    [[nodiscard]] QVariantMap destinationMap(int slotIndex) const;
    void reportAction(const QString& text, const QString& tone);
    void reportError(const QString& message);
    void reloadSavedBanks();
    void announceBankChange();

    library::LibraryDatabase* m_database = nullptr;
    services::PatchTransfer* m_transfer = nullptr;
    services::PatchWorkspace* m_workspace = nullptr;
    services::UserMemoryWrite* m_userWrite = nullptr;
    services::UserBankRead* m_bankRead = nullptr;
    library::BankDraft m_draft;

    int m_subgroup = 0;
    int m_bank = 1;
    int m_number = 1;

    QString m_lastAction;
    QString m_lastActionTone = QStringLiteral("neutral");
    QString m_lastError;
    QVariantList m_savedBanks;
    QVariantList m_sources;
};

} // namespace xp60studio::presentation
