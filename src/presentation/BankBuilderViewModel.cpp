#include "presentation/BankBuilderViewModel.h"

#include "library/PatchProvenance.h"
#include "xpmodel/Xp60BankLocation.h"

#include <QDateTime>

#include <chrono>

namespace xp60studio::presentation {

using library::BankDraft;
using library::BankSlotContent;
using xpmodel::Xp60BankLocation;

namespace {

QString toQt(const std::string& text)
{
    return QString::fromStdString(text);
}

// Where a Patch sat in its source, said the way the library says it elsewhere:
// "USER:007" when it came from a User bank slot, otherwise nothing. Never
// invented — a Patch read from the temporary area has no User number, and
// pretending it has one would be inventing provenance.
QString sourceSlotLabel(const library::PatchProvenance& provenance)
{
    if (!provenance.userNumber) {
        return {};
    }
    return QStringLiteral("USER:%1").arg(*provenance.userNumber, 3, 10, QLatin1Char('0'));
}

QString isoDate(std::chrono::system_clock::time_point when)
{
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(when.time_since_epoch()).count();
    return QDateTime::fromSecsSinceEpoch(seconds).toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

} // namespace

BankBuilderViewModel::BankBuilderViewModel(QObject* parent)
    : QObject(parent)
{
}

void BankBuilderViewModel::setDatabase(library::LibraryDatabase* database)
{
    if (m_database == database) {
        return;
    }
    m_database = database;
    reloadSavedBanks();
    announceBankChange();
}

// ---------------------------------------------------------------------------
// Panel selection
// ---------------------------------------------------------------------------

int BankBuilderViewModel::subgroup() const
{
    return m_subgroup;
}

int BankBuilderViewModel::bank() const
{
    return m_bank;
}

int BankBuilderViewModel::number() const
{
    return m_number;
}

int BankBuilderViewModel::currentSlotIndex() const
{
    const auto location = Xp60BankLocation::fromPanel(m_subgroup, m_bank, m_number);
    return location ? location->slotIndex() : 0;
}

void BankBuilderViewModel::selectSubgroup(int subgroup)
{
    if (!Xp60BankLocation::isValidSubgroup(subgroup) || subgroup == m_subgroup) {
        return;
    }
    m_subgroup = subgroup;
    emit selectionChanged();
    emit bankChanged();
    emit auditionChanged();
}

void BankBuilderViewModel::selectBank(int bank)
{
    if (!Xp60BankLocation::isValidBank(bank) || bank == m_bank) {
        return;
    }
    m_bank = bank;
    emit selectionChanged();
    emit bankChanged();
    emit auditionChanged();
}

void BankBuilderViewModel::selectNumber(int number)
{
    if (!Xp60BankLocation::isValidNumber(number) || number == m_number) {
        return;
    }
    m_number = number;
    emit selectionChanged();
    emit bankChanged();
    emit auditionChanged();
}

void BankBuilderViewModel::selectSlot(int slotIndex)
{
    const auto location = Xp60BankLocation::fromSlotIndex(slotIndex);
    if (!location) {
        return;
    }
    if (location->subgroup() == m_subgroup && location->bank() == m_bank && location->number() == m_number) {
        return;
    }
    m_subgroup = location->subgroup();
    m_bank = location->bank();
    m_number = location->number();
    emit selectionChanged();
    emit bankChanged();
    emit auditionChanged();
}

// ---------------------------------------------------------------------------
// The display
// ---------------------------------------------------------------------------

QString BankBuilderViewModel::subgroupLabel() const
{
    return toQt(Xp60BankLocation::subgroupName(m_subgroup));
}

QString BankBuilderViewModel::panelLabel() const
{
    const auto location = Xp60BankLocation::fromPanel(m_subgroup, m_bank, m_number);
    return location ? toQt(location->panelLabel()) : QStringLiteral("---");
}

QString BankBuilderViewModel::linearLabel() const
{
    return toQt(Xp60BankLocation::linearLabelFor(currentSlotIndex()));
}

QString BankBuilderViewModel::spokenLabel() const
{
    const auto location = Xp60BankLocation::fromPanel(m_subgroup, m_bank, m_number);
    return location ? toQt(location->spokenLabel()) : QString();
}

QString BankBuilderViewModel::currentPatchName() const
{
    // The instrument display reads from the working copy too, so the LCD and
    // the Editor's title cannot show different names for the same Patch.
    const auto& content = m_draft.slot(currentSlotIndex());
    if (m_workspace && m_workspace->hasPatch() && content.patchId > 0
        && m_workspace->origin().isLibraryEntry(content.patchId)) {
        return m_workspace->displayName();
    }
    return toQt(content.patchName);
}

QString BankBuilderViewModel::currentSourceName() const
{
    return toQt(m_draft.slot(currentSlotIndex()).sourceName);
}

QString BankBuilderViewModel::currentSourceSlot() const
{
    return toQt(m_draft.slot(currentSlotIndex()).sourceSlotLabel);
}

QString BankBuilderViewModel::currentState() const
{
    const auto& content = m_draft.slot(currentSlotIndex());
    if (content.missing) {
        return QStringLiteral("MISSING");
    }
    return content.empty() ? QStringLiteral("EMPTY") : QStringLiteral("ASSIGNED");
}

bool BankBuilderViewModel::currentOccupied() const
{
    return !m_draft.slot(currentSlotIndex()).empty();
}

// ---------------------------------------------------------------------------
// Projections
// ---------------------------------------------------------------------------

QVariantMap BankBuilderViewModel::destinationMap(int slotIndex) const
{
    QVariantMap map;
    const auto location = Xp60BankLocation::fromSlotIndex(slotIndex);
    if (!location) {
        return map;
    }
    const auto& content = m_draft.slot(slotIndex);
    map.insert(QStringLiteral("slotIndex"), slotIndex);
    map.insert(QStringLiteral("subgroup"), location->subgroup());
    map.insert(QStringLiteral("bank"), location->bank());
    map.insert(QStringLiteral("number"), location->number());
    map.insert(QStringLiteral("panelLabel"), toQt(location->panelLabel()));
    map.insert(QStringLiteral("linearLabel"), toQt(location->linearLabel()));
    map.insert(QStringLiteral("patchId"), QVariant::fromValue<qint64>(content.patchId));
    // A destination holding the Patch open in the Editor answers from the
    // working copy. Without this the panel would keep showing the name the
    // Patch had when it was placed, which is exactly the drift this
    // architecture exists to prevent.
    const bool editing = m_workspace && m_workspace->hasPatch() && content.patchId > 0
        && m_workspace->origin().isLibraryEntry(content.patchId);
    map.insert(QStringLiteral("editing"), editing);
    map.insert(QStringLiteral("edited"), editing && m_workspace->modified());
    map.insert(QStringLiteral("patchName"), editing ? m_workspace->displayName() : toQt(content.patchName));
    map.insert(QStringLiteral("sourceName"), toQt(content.sourceName));
    map.insert(QStringLiteral("sourceSlot"), toQt(content.sourceSlotLabel));
    map.insert(QStringLiteral("occupied"), !content.empty());
    map.insert(QStringLiteral("missing"), content.missing);
    map.insert(QStringLiteral("current"), slotIndex == currentSlotIndex());
    return map;
}

QVariantMap BankBuilderViewModel::destinationAt(int slotIndex) const
{
    return destinationMap(slotIndex);
}

QVariantList BankBuilderViewModel::visibleDestinations() const
{
    QVariantList list;
    for (int number = 1; number <= Xp60BankLocation::kNumbersPerBank; ++number) {
        const auto location = Xp60BankLocation::fromPanel(m_subgroup, m_bank, number);
        if (location) {
            list.append(destinationMap(location->slotIndex()));
        }
    }
    return list;
}

QVariantList BankBuilderViewModel::numberOccupancy() const
{
    QVariantList list;
    for (int number = 1; number <= Xp60BankLocation::kNumbersPerBank; ++number) {
        const auto location = Xp60BankLocation::fromPanel(m_subgroup, m_bank, number);
        list.append(location && m_draft.isOccupied(location->slotIndex()));
    }
    return list;
}

QVariantList BankBuilderViewModel::bankOccupancy() const
{
    QVariantList list;
    for (int bank = 1; bank <= Xp60BankLocation::kBanksPerSubgroup; ++bank) {
        list.append(m_draft.occupiedInBank(m_subgroup, bank));
    }
    return list;
}

int BankBuilderViewModel::subgroupOccupancyA() const
{
    int total = 0;
    for (int bank = 1; bank <= Xp60BankLocation::kBanksPerSubgroup; ++bank) {
        total += m_draft.occupiedInBank(0, bank);
    }
    return total;
}

int BankBuilderViewModel::subgroupOccupancyB() const
{
    int total = 0;
    for (int bank = 1; bank <= Xp60BankLocation::kBanksPerSubgroup; ++bank) {
        total += m_draft.occupiedInBank(1, bank);
    }
    return total;
}

QVariantList BankBuilderViewModel::overview() const
{
    QVariantList list;
    for (int subgroup = 0; subgroup < Xp60BankLocation::kSubgroupCount; ++subgroup) {
        for (int bank = 1; bank <= Xp60BankLocation::kBanksPerSubgroup; ++bank) {
            const auto first = Xp60BankLocation::fromPanel(subgroup, bank, 1);
            const auto last = Xp60BankLocation::fromPanel(subgroup, bank, Xp60BankLocation::kNumbersPerBank);
            if (!first || !last) {
                continue;
            }
            QVariantList filled;
            QVariantList names;
            for (int number = 1; number <= Xp60BankLocation::kNumbersPerBank; ++number) {
                const auto location = Xp60BankLocation::fromPanel(subgroup, bank, number);
                const auto& content = m_draft.slot(location ? location->slotIndex() : -1);
                filled.append(!content.empty());
                names.append(toQt(content.patchName));
            }
            QVariantMap map;
            map.insert(QStringLiteral("subgroup"), subgroup);
            map.insert(QStringLiteral("subgroupLabel"), toQt(Xp60BankLocation::subgroupName(subgroup)));
            map.insert(QStringLiteral("bank"), bank);
            map.insert(QStringLiteral("label"), toQt(Xp60BankLocation::subgroupName(subgroup)) + QString::number(bank));
            map.insert(QStringLiteral("occupied"), m_draft.occupiedInBank(subgroup, bank));
            map.insert(QStringLiteral("first"), toQt(first->linearLabel()));
            map.insert(QStringLiteral("last"), toQt(last->linearLabel()));
            map.insert(QStringLiteral("firstSlotIndex"), first->slotIndex());
            map.insert(QStringLiteral("current"), subgroup == m_subgroup && bank == m_bank);
            map.insert(QStringLiteral("filled"), filled);
            map.insert(QStringLiteral("names"), names);
            list.append(map);
        }
    }
    return list;
}

// ---------------------------------------------------------------------------
// The bank
// ---------------------------------------------------------------------------

QString BankBuilderViewModel::bankName() const
{
    return toQt(m_draft.name());
}

void BankBuilderViewModel::setBankName(const QString& name)
{
    if (m_draft.setName(name.trimmed().toStdString())) {
        announceBankChange();
    }
}

int BankBuilderViewModel::missingCount() const
{
    int missing = 0;
    for (const auto& content : m_draft.destinations()) {
        if (content.missing) {
            ++missing;
        }
    }
    return missing;
}

QString BankBuilderViewModel::undoLabel() const
{
    return toQt(m_draft.undoLabel());
}

QString BankBuilderViewModel::redoLabel() const
{
    return toQt(m_draft.redoLabel());
}

QString BankBuilderViewModel::panelLabelFor(int slotIndex) const
{
    const auto location = Xp60BankLocation::fromSlotIndex(slotIndex);
    return location ? toQt(location->panelLabel()) : QString();
}

QString BankBuilderViewModel::linearLabelFor(int slotIndex) const
{
    return toQt(Xp60BankLocation::linearLabelFor(slotIndex));
}

int BankBuilderViewModel::slotIndexFor(int subgroup, int bank, int number) const
{
    const auto location = Xp60BankLocation::fromPanel(subgroup, bank, number);
    return location ? location->slotIndex() : -1;
}

qint64 BankBuilderViewModel::patchIdAt(int slotIndex) const
{
    return m_draft.slot(slotIndex).patchId;
}

// ---------------------------------------------------------------------------
// Placement
// ---------------------------------------------------------------------------

std::optional<BankSlotContent> BankBuilderViewModel::contentFor(std::int64_t patchId) const
{
    if (!m_database || patchId <= 0) {
        return std::nullopt;
    }
    const auto record = m_database->record(patchId);
    if (!record) {
        return std::nullopt;
    }
    BankSlotContent content;
    content.patchId = record->id;
    content.patchName = record->name;
    content.sourceName = record->provenance.sourceName;
    content.sourceSlotLabel = sourceSlotLabel(record->provenance).toStdString();
    return content;
}

void BankBuilderViewModel::setWorkspace(services::PatchWorkspace* workspace)
{
    if (m_workspace == workspace) {
        return;
    }
    if (m_workspace) {
        disconnect(m_workspace, nullptr, this, nullptr);
    }
    m_workspace = workspace;
    if (m_workspace) {
        connect(m_workspace, &services::PatchWorkspace::changed, this, &BankBuilderViewModel::announceBankChange);
        connect(m_workspace, &services::PatchWorkspace::originChanged, this,
                &BankBuilderViewModel::announceBankChange);
    }
    announceBankChange();
}

bool BankBuilderViewModel::editSlot(int slotIndex)
{
    if (!m_database || !m_workspace) {
        return false;
    }
    const auto& content = m_draft.slot(slotIndex);
    if (content.patchId <= 0) {
        reportError(tr("That destination is empty."));
        return false;
    }
    const auto entry = m_database->loadEntry(content.patchId);
    if (!entry) {
        // The cached name survives a deleted Patch so the destination can say
        // MISSING; it is not enough to edit from.
        reportError(tr("“%1” is no longer in the library.").arg(toQt(content.patchName)));
        return false;
    }
    m_workspace->adopt(entry->patch(), services::PatchOrigin::library(content.patchId));
    selectSlot(slotIndex);
    reportAction(tr("Editing “%1” from %2").arg(toQt(content.patchName), panelLabelFor(slotIndex)),
                 QStringLiteral("info"));
    return true;
}

bool BankBuilderViewModel::editCurrent()
{
    return editSlot(currentSlotIndex());
}

bool BankBuilderViewModel::placePatch(int slotIndex, qint64 patchId)
{
    const auto content = contentFor(patchId);
    if (!content) {
        reportError(tr("That Patch is no longer in the library."));
        return false;
    }
    if (!Xp60BankLocation::isValidSlotIndex(slotIndex)) {
        reportError(tr("A User bank has no destination %1.").arg(slotIndex));
        return false;
    }
    const bool replacing = m_draft.isOccupied(slotIndex);
    if (!m_draft.assign(slotIndex, *content)) {
        // The only remaining refusal is "that Patch is already there", which
        // is not an error and not worth a message.
        return false;
    }
    reportAction(toQt(m_draft.lastActionLabel()), replacing ? QStringLiteral("warning") : QStringLiteral("success"));
    selectSlot(slotIndex);
    announceBankChange();
    return true;
}

bool BankBuilderViewModel::placePatchAtCurrent(qint64 patchId)
{
    return placePatch(currentSlotIndex(), patchId);
}

bool BankBuilderViewModel::clearSlot(int slotIndex)
{
    if (!m_draft.clear(slotIndex)) {
        return false;
    }
    reportAction(toQt(m_draft.lastActionLabel()), QStringLiteral("neutral"));
    announceBankChange();
    return true;
}

bool BankBuilderViewModel::moveSlot(int from, int to)
{
    if (!m_draft.moveOrSwap(from, to)) {
        return false;
    }
    reportAction(toQt(m_draft.lastActionLabel()), QStringLiteral("success"));
    selectSlot(to);
    announceBankChange();
    return true;
}

bool BankBuilderViewModel::clearAll()
{
    if (!m_draft.clearAll()) {
        return false;
    }
    reportAction(toQt(m_draft.lastActionLabel()), QStringLiteral("warning"));
    announceBankChange();
    return true;
}

bool BankBuilderViewModel::undo()
{
    if (!m_draft.undo()) {
        return false;
    }
    reportAction(toQt(m_draft.lastActionLabel()), QStringLiteral("info"));
    announceBankChange();
    return true;
}

bool BankBuilderViewModel::redo()
{
    if (!m_draft.redo()) {
        return false;
    }
    reportAction(toQt(m_draft.lastActionLabel()), QStringLiteral("info"));
    announceBankChange();
    return true;
}

void BankBuilderViewModel::acknowledge()
{
    if (m_lastAction.isEmpty()) {
        return;
    }
    m_lastAction.clear();
    m_lastActionTone = QStringLiteral("neutral");
    emit actionChanged();
}

QVariantMap BankBuilderViewModel::dropPreview(int slotIndex, qint64 patchId, int movingFromSlot) const
{
    QVariantMap map;
    const auto location = Xp60BankLocation::fromSlotIndex(slotIndex);
    if (!location) {
        return map;
    }
    const auto& target = m_draft.slot(slotIndex);
    map.insert(QStringLiteral("slotIndex"), slotIndex);
    map.insert(QStringLiteral("panelLabel"), toQt(location->panelLabel()));
    map.insert(QStringLiteral("linearLabel"), toQt(location->linearLabel()));
    map.insert(QStringLiteral("occupant"), toQt(target.patchName));

    if (Xp60BankLocation::isValidSlotIndex(movingFromSlot)) {
        // A drag that started inside the bank.
        const auto& moving = m_draft.slot(movingFromSlot);
        map.insert(QStringLiteral("patchName"), toQt(moving.patchName));
        map.insert(QStringLiteral("action"),
                   slotIndex == movingFromSlot        ? QStringLiteral("KEEP")
                       : target.empty()               ? QStringLiteral("MOVE")
                                                      : QStringLiteral("SWAP"));
        map.insert(QStringLiteral("from"), toQt(Xp60BankLocation::fromSlotIndex(movingFromSlot)->panelLabel()));
        return map;
    }

    const auto content = contentFor(patchId);
    map.insert(QStringLiteral("patchName"), content ? toQt(content->patchName) : QString());
    map.insert(QStringLiteral("action"), target.empty() ? QStringLiteral("PLACE") : QStringLiteral("REPLACE"));
    return map;
}

// ---------------------------------------------------------------------------
// Saved banks
// ---------------------------------------------------------------------------

void BankBuilderViewModel::newEmptyBank(const QString& name)
{
    m_draft.reset(name.trimmed().isEmpty() ? std::string("New User Bank") : name.trimmed().toStdString(),
                  std::vector<BankSlotContent>(static_cast<std::size_t>(BankDraft::kSlotCount)));
    m_subgroup = 0;
    m_bank = 1;
    m_number = 1;
    reportAction(tr("New empty bank — 128 destinations"), QStringLiteral("neutral"));
    emit selectionChanged();
    announceBankChange();
}

bool BankBuilderViewModel::saveAsNewBank(const QString& name)
{
    if (!m_database) {
        reportError(tr("The library is not open, so a bank cannot be saved."));
        return false;
    }
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        reportError(tr("Give the bank a name before saving it."));
        return false;
    }
    const auto id = m_database->saveBank(trimmed.toStdString(), m_draft.destinations());
    if (!id) {
        reportError(m_database->lastError());
        return false;
    }
    // The draft keeps its arrangement and simply becomes that saved bank: the
    // musician carries on from where they were rather than being handed a
    // fresh screen after saving.
    m_draft.setName(trimmed.toStdString());
    m_draft.markSaved(*id);
    reportAction(tr("Saved “%1” — %n destination(s) filled", "", m_draft.occupiedCount()).arg(trimmed),
                 QStringLiteral("success"));
    reloadSavedBanks();
    announceBankChange();
    return true;
}

bool BankBuilderViewModel::saveBank()
{
    if (!m_database) {
        reportError(tr("The library is not open, so a bank cannot be saved."));
        return false;
    }
    const auto existing = m_draft.savedBankId();
    if (!existing) {
        return saveAsNewBank(bankName());
    }
    const auto id = m_database->saveBank(m_draft.name(), m_draft.destinations(), existing);
    if (!id) {
        reportError(m_database->lastError());
        return false;
    }
    m_draft.markSaved(*id);
    reportAction(tr("Saved “%1”").arg(bankName()), QStringLiteral("success"));
    reloadSavedBanks();
    announceBankChange();
    return true;
}

QVariantMap BankBuilderViewModel::fillFromSource(const QString& digest)
{
    QVariantMap report;
    report.insert(QStringLiteral("ok"), false);
    report.insert(QStringLiteral("placed"), 0);
    report.insert(QStringLiteral("unplaced"), 0);
    report.insert(QStringLiteral("conflicts"), 0);
    report.insert(QStringLiteral("sourceName"), QString());

    if (!m_database) {
        report.insert(QStringLiteral("message"), tr("The library is not open."));
        reportError(tr("The library is not open."));
        return report;
    }

    library::LibraryQuery query;
    query.sourceDigest = digest.toStdString();
    query.order = library::LibraryQuery::Order::SourceSlotAscending;
    const auto records = m_database->search(query);
    if (records.empty()) {
        const auto message = tr("That source has no Patches in the library.");
        report.insert(QStringLiteral("message"), message);
        reportError(message);
        return report;
    }

    QString sourceName = toQt(records.front().provenance.sourceName);

    std::vector<std::pair<int, BankSlotContent>> placements;
    placements.reserve(records.size());
    std::vector<bool> claimed(static_cast<std::size_t>(BankDraft::kSlotCount), false);
    int unplaced = 0;
    int conflicts = 0;

    for (const auto& record : records) {
        if (!record.provenance.userNumber || !Xp60BankLocation::isValidUserNumber(*record.provenance.userNumber)) {
            // No recorded destination. Guessing one would invent provenance.
            ++unplaced;
            continue;
        }
        const int slotIndex = *record.provenance.userNumber - 1;
        if (claimed[static_cast<std::size_t>(slotIndex)]) {
            // Two Patches read from the same User slot: keep the first and say
            // so, rather than deciding for the musician which one wins.
            ++conflicts;
            continue;
        }
        claimed[static_cast<std::size_t>(slotIndex)] = true;

        BankSlotContent content;
        content.patchId = record.id;
        content.patchName = record.name;
        content.sourceName = record.provenance.sourceName;
        content.sourceSlotLabel = sourceSlotLabel(record.provenance).toStdString();
        placements.emplace_back(slotIndex, std::move(content));
    }

    const int placed = static_cast<int>(placements.size());
    report.insert(QStringLiteral("placed"), placed);
    report.insert(QStringLiteral("unplaced"), unplaced);
    report.insert(QStringLiteral("conflicts"), conflicts);
    report.insert(QStringLiteral("sourceName"), sourceName);

    if (placements.empty()) {
        const auto message = tr("No Patch in “%1” records which User slot it came from, so there is nothing "
                                "to arrange. Place them by hand instead.")
                                 .arg(sourceName);
        report.insert(QStringLiteral("message"), message);
        reportError(message);
        return report;
    }

    const QString label = tr("Fill from “%1” — %n destination(s)", "", placed).arg(sourceName);
    if (!m_draft.assignAll(placements, label.toStdString())) {
        const auto message = tr("“%1” is already arranged exactly like this.").arg(sourceName);
        report.insert(QStringLiteral("ok"), true);
        report.insert(QStringLiteral("message"), message);
        reportAction(message, QStringLiteral("info"));
        return report;
    }

    QString message = tr("Filled %n destination(s) from “%1”", "", placed).arg(sourceName);
    QString tone = QStringLiteral("success");
    QStringList caveats;
    if (unplaced > 0) {
        caveats.append(tr("%n Patch(es) record no User slot and were left out", "", unplaced));
    }
    if (conflicts > 0) {
        caveats.append(tr("%n Patch(es) wanted a destination already taken by an earlier one", "", conflicts));
    }
    if (!caveats.isEmpty()) {
        message += QStringLiteral(" — ") + caveats.join(QStringLiteral("; "));
        tone = QStringLiteral("warning");
    }

    report.insert(QStringLiteral("ok"), true);
    report.insert(QStringLiteral("message"), message);
    reportAction(message, tone);
    announceBankChange();
    return report;
}

bool BankBuilderViewModel::loadBank(qint64 bankId)
{
    if (!m_database) {
        reportError(tr("The library is not open."));
        return false;
    }
    const auto bank = m_database->loadBank(bankId);
    if (!bank) {
        reportError(m_database->lastError());
        return false;
    }
    m_draft.reset(bank->record.name, bank->destinations);
    m_draft.markSaved(bank->record.id);
    m_subgroup = 0;
    m_bank = 1;
    m_number = 1;
    if (bank->record.missingCount > 0) {
        reportAction(tr("Opened “%1” — %n destination(s) reference a Patch that is no longer in the library",
                        "", bank->record.missingCount)
                         .arg(toQt(bank->record.name)),
                     QStringLiteral("warning"));
    } else {
        reportAction(tr("Opened “%1” — %n destination(s) filled", "", bank->record.occupiedCount)
                         .arg(toQt(bank->record.name)),
                     QStringLiteral("info"));
    }
    emit selectionChanged();
    announceBankChange();
    return true;
}

bool BankBuilderViewModel::deleteBank(qint64 bankId)
{
    if (!m_database) {
        reportError(tr("The library is not open."));
        return false;
    }
    if (!m_database->removeBank(bankId)) {
        reportError(m_database->lastError());
        return false;
    }
    // Deleting the stored bank does not throw away the arrangement on screen;
    // it only stops it being attached to a bank that no longer exists.
    if (m_draft.savedBankId() && *m_draft.savedBankId() == bankId) {
        m_draft.detachFromSavedBank();
    }
    reportAction(tr("Deleted the saved bank. No Patch was removed from the library."),
                 QStringLiteral("neutral"));
    reloadSavedBanks();
    announceBankChange();
    return true;
}

void BankBuilderViewModel::refresh()
{
    reloadSavedBanks();
    announceBankChange();
}

QVariantList BankBuilderViewModel::arrangementIds() const
{
    QVariantList ids;
    for (const auto& content : m_draft.destinations()) {
        ids.append(QVariant::fromValue<qint64>(content.patchId));
    }
    return ids;
}

void BankBuilderViewModel::reloadSavedBanks()
{
    m_savedBanks.clear();
    m_sources.clear();
    if (m_database) {
        for (const auto& record : m_database->banks()) {
            QVariantMap map;
            map.insert(QStringLiteral("id"), QVariant::fromValue<qint64>(record.id));
            map.insert(QStringLiteral("name"), toQt(record.name));
            map.insert(QStringLiteral("occupied"), record.occupiedCount);
            map.insert(QStringLiteral("missing"), record.missingCount);
            map.insert(QStringLiteral("updated"), isoDate(record.updatedAt));
            m_savedBanks.append(map);
        }
        for (const auto& source : m_database->sourcesInUse()) {
            QVariantMap map;
            map.insert(QStringLiteral("digest"), toQt(source.digest));
            map.insert(QStringLiteral("name"), toQt(source.name));
            map.insert(QStringLiteral("patchCount"), source.patchCount);
            map.insert(QStringLiteral("imported"), isoDate(source.importedAt));
            m_sources.append(map);
        }
    }
    emit savedBanksChanged();
}

void BankBuilderViewModel::announceBankChange()
{
    emit bankChanged();
    // What is at the selected destination decides whether it can be
    // auditioned, so the two always move together.
    emit auditionChanged();
}

void BankBuilderViewModel::reportAction(const QString& text, const QString& tone)
{
    m_lastAction = text;
    m_lastActionTone = tone;
    emit actionChanged();
}

void BankBuilderViewModel::reportError(const QString& message)
{
    m_lastError = message;
    reportAction(message, QStringLiteral("error"));
    emit errorOccurred(message);
}


// ---------------------------------------------------------------------------
// Audition
//
// Nothing new is invented here. The Patch is rebuilt from the preserved
// original SysEx by the library, and sent through services::PatchTransfer,
// which writes only to the temporary Patch area, requires a verified
// temporary read first, and proves the write by reading it back.
// ---------------------------------------------------------------------------

void BankBuilderViewModel::setTransfer(services::PatchTransfer* transfer)
{
    if (m_transfer == transfer) {
        return;
    }
    if (m_transfer) {
        disconnect(m_transfer, nullptr, this, nullptr);
    }
    m_transfer = transfer;
    if (m_transfer) {
        connect(m_transfer, &services::PatchTransfer::changed, this, &BankBuilderViewModel::auditionChanged);
    }
    emit auditionChanged();
}

bool BankBuilderViewModel::canAudition() const
{
    if (!m_transfer || !m_database) {
        return false;
    }
    const auto& content = m_draft.slot(currentSlotIndex());
    if (content.empty() || content.missing || content.patchId <= 0) {
        return false;
    }
    return m_transfer->canArm() && !m_transfer->isBusy() && !m_transfer->liveActive();
}

bool BankBuilderViewModel::auditionBusy() const
{
    return m_transfer && m_transfer->isBusy();
}

QString BankBuilderViewModel::auditionState() const
{
    return m_transfer ? toQt(m_transfer->stateLabel()) : QString();
}

QString BankBuilderViewModel::auditionMessage() const
{
    if (!m_transfer) {
        return tr("Auditioning needs a connected XP-60.");
    }
    if (m_transfer->isBusy()) {
        return toQt(m_transfer->message());
    }
    const auto& content = m_draft.slot(currentSlotIndex());
    if (content.missing) {
        return tr("This destination references a Patch that is no longer in the library.");
    }
    if (content.empty()) {
        return tr("Select a destination that holds a Patch.");
    }
    if (!m_transfer->canArm()) {
        return tr("Auditioning becomes available once a temporary-Patch read has succeeded in this session.");
    }
    return tr("Sends this Patch to the temporary Patch area and reads it back. "
              "Permanent User memory is not written.");
}

bool BankBuilderViewModel::auditionSlot(int slotIndex)
{
    selectSlot(slotIndex);
    return auditionCurrent();
}

bool BankBuilderViewModel::auditionCurrent()
{
    if (!canAudition()) {
        reportError(auditionMessage());
        return false;
    }
    const auto& content = m_draft.slot(currentSlotIndex());
    const auto entry = m_database->loadEntry(content.patchId);
    if (!entry) {
        reportError(m_database->lastError());
        return false;
    }
    if (!m_transfer->arm()) {
        reportError(tr("The write could not be armed."));
        return false;
    }
    if (!m_transfer->writeAndVerifyTemporaryPatch(entry->patch())) {
        reportError(toQt(m_transfer->message()));
        return false;
    }
    reportAction(tr("Auditioning %1 at %2").arg(toQt(content.patchName), panelLabel()), QStringLiteral("live"));
    emit auditionChanged();
    return true;
}

} // namespace xp60studio::presentation
