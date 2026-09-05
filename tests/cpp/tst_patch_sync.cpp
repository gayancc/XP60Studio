// Cross-screen synchronization: one working Patch, three views.
//
// The requirement this file exists for, stated plainly: if the Patch name
// changes in the Editor, the Bank Builder must not go on showing the old name,
// and neither must the Library. Before the shared workspace those three held
// separate copies — the Editor its own Patch, the Library a cached row, the
// Bank Builder a cached destination name — and nothing connected them.
//
// So these tests deliberately never touch two view models at once. They change
// the Patch in one place and read it in another, which is the only way to prove
// the views are projections of one thing rather than three things that happen
// to agree at construction time.
//
// Everything runs over tests/fixtures/xp60/user-bank-amal.syx, a real XP-60
// User bank, in an in-memory library.

#include "library/BankDraft.h"
#include "library/LibraryDatabase.h"
#include "library/SyxImport.h"
#include "presentation/BankBuilderViewModel.h"
#include "presentation/LibraryListModel.h"
#include "services/PatchWorkspace.h"
#include "xpmodel/Xp60BankLocation.h"

#include <QFile>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QTest>

using namespace xp60studio;
using library::LibraryDatabase;
using presentation::BankBuilderViewModel;
using presentation::LibraryListModel;
using services::PatchOrigin;
using services::PatchWorkspace;
using xpmodel::Xp60BankLocation;

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

int slotOf(const char* panelLabel)
{
    const auto location = Xp60BankLocation::fromPanelLabel(panelLabel);
    return location ? location->slotIndex() : -1;
}

} // namespace

class TestPatchSync : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    void aRenameInTheEditorReachesTheBankBuilderAndTheLibrary();
    void aBankDestinationCanBeOpenedForEditing();
    void aLibraryRowCanBeOpenedForEditing();
    void onlyTheDestinationsHoldingThatPatchAreOverlaid();
    void savingTheWorkingPatchSettlesEveryViewOnTheSameName();
    void withoutAWorkspaceTheViewsAreExactlyWhatTheDatabaseSays();

private:
    std::vector<std::int64_t> importFixture();

    roland::ByteVector m_fixture;
    LibraryDatabase m_db;
};

void TestPatchSync::initTestCase()
{
    m_fixture = readFixture();
    QVERIFY2(!m_fixture.empty(), "golden fixture missing");
    QVERIFY2(QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE")),
             "this Qt build has no QSQLITE driver");
}

void TestPatchSync::init()
{
    m_db.close();
    QVERIFY2(m_db.open(QString::fromLatin1(LibraryDatabase::kInMemoryPath)), qPrintable(m_db.lastError()));
}

std::vector<std::int64_t> TestPatchSync::importFixture()
{
    library::SyxImportOptions options;
    options.sourceName = "user-bank-amal.syx";
    options.importedAt = std::chrono::system_clock::time_point{std::chrono::seconds{1'700'000'000}};
    const auto result = library::importSyxStream(m_fixture, options);
    const auto ids = m_db.insertAll(result.entries);
    return ids ? *ids : std::vector<std::int64_t>{};
}

// ---------------------------------------------------------------------------

void TestPatchSync::aRenameInTheEditorReachesTheBankBuilderAndTheLibrary()
{
    const auto ids = importFixture();
    QVERIFY(ids.size() >= 2);

    PatchWorkspace workspace;
    LibraryListModel library;
    library.setDatabase(&m_db);
    library.setWorkspace(&workspace);
    BankBuilderViewModel builder;
    builder.setDatabase(&m_db);
    builder.setWorkspace(&workspace);

    // The Patch is placed in a bank, and separately opened for editing — which
    // is exactly the situation the old architecture could not represent.
    QVERIFY(builder.placePatch(slotOf("A35"), ids[0]));
    const auto placedName = builder.destinationAt(slotOf("A35")).value(QStringLiteral("patchName")).toString();
    QVERIFY(!placedName.isEmpty());

    const auto entry = m_db.loadEntry(ids[0]);
    QVERIFY(entry.has_value());
    workspace.adopt(entry->patch(), PatchOrigin::library(ids[0]));

    // The rename happens in the workspace, as the Editor would do it. Nothing
    // below touches the Bank Builder or the Library.
    QSignalSpy bankSpy(&builder, &BankBuilderViewModel::bankChanged);
    QVERIFY(workspace.edit(QStringLiteral("Rename"), [](xpmodel::Xp60Patch& p) {
        p.setName(*xpmodel::PatchName::fromText("Renamed"));
    }));
    QVERIFY(bankSpy.count() > 0);

    // The bank destination shows the new name...
    const auto destination = builder.destinationAt(slotOf("A35"));
    QCOMPARE(destination.value(QStringLiteral("patchName")).toString(), QStringLiteral("Renamed"));
    QVERIFY(destination.value(QStringLiteral("editing")).toBool());
    QVERIFY(destination.value(QStringLiteral("edited")).toBool());
    // ...and so does the instrument display beside it.
    builder.selectSlot(slotOf("A35"));
    QCOMPARE(builder.currentPatchName(), QStringLiteral("Renamed"));

    // ...and so does the Library row, which still has the old name in the
    // database: the row is a projection, not a copy.
    int row = -1;
    for (int i = 0; i < library.rowCount(); ++i) {
        if (library.data(library.index(i, 0), LibraryListModel::IdRole).toLongLong() == ids[0]) {
            row = i;
            break;
        }
    }
    QVERIFY(row >= 0);
    QCOMPARE(library.data(library.index(row, 0), LibraryListModel::NameRole).toString(),
             QStringLiteral("Renamed"));
    QVERIFY(library.data(library.index(row, 0), LibraryListModel::EditingRole).toBool());
    QVERIFY(library.data(library.index(row, 0), LibraryListModel::EditedRole).toBool());
    QCOMPARE(QString::fromStdString(m_db.record(ids[0])->name), placedName);
}

void TestPatchSync::aBankDestinationCanBeOpenedForEditing()
{
    const auto ids = importFixture();
    PatchWorkspace workspace;
    BankBuilderViewModel builder;
    builder.setDatabase(&m_db);
    builder.setWorkspace(&workspace);

    QVERIFY(builder.placePatch(slotOf("B11"), ids[3]));
    QVERIFY(builder.editSlot(slotOf("B11")));

    QVERIFY(workspace.hasPatch());
    QCOMPARE(workspace.origin(), PatchOrigin::library(ids[3]));
    // Opening a destination also moves the panel there, so the display and the
    // Editor are talking about the same thing.
    QCOMPARE(builder.currentSlotIndex(), slotOf("B11"));

    // An empty destination has nothing to open, and says so rather than
    // silently doing nothing.
    QVERIFY(!builder.editSlot(slotOf("B88")));
    QCOMPARE(workspace.origin(), PatchOrigin::library(ids[3]));

    // A destination whose Patch has been deleted keeps its cached name so it can
    // report itself MISSING, but that name is not enough to edit from.
    QVERIFY(m_db.remove(ids[3]));
    QVERIFY(!builder.editSlot(slotOf("B11")));
}

void TestPatchSync::aLibraryRowCanBeOpenedForEditing()
{
    const auto ids = importFixture();
    PatchWorkspace workspace;
    LibraryListModel library;
    library.setDatabase(&m_db);
    library.setWorkspace(&workspace);

    QVERIFY(library.rowCount() > 0);
    QVERIFY(library.editRow(0));
    QVERIFY(workspace.hasPatch());
    QCOMPARE(workspace.origin().kind, PatchOrigin::Kind::LibraryEntry);
    QCOMPARE(workspace.studioState(), services::StudioState::Saved);

    QVERIFY(!library.editRow(9999));
    QVERIFY(!library.editEntry(0));
    // A refused open changes nothing: the Patch already open stays open.
    QVERIFY(workspace.hasPatch());
}

void TestPatchSync::onlyTheDestinationsHoldingThatPatchAreOverlaid()
{
    const auto ids = importFixture();
    PatchWorkspace workspace;
    BankBuilderViewModel builder;
    builder.setDatabase(&m_db);
    builder.setWorkspace(&workspace);

    QVERIFY(builder.placePatch(slotOf("A11"), ids[0]));
    QVERIFY(builder.placePatch(slotOf("A12"), ids[1]));
    // The same Patch in two destinations: a bank is an arrangement of
    // references, so both must follow the edit.
    QVERIFY(builder.placePatch(slotOf("A21"), ids[0]));
    const auto neighbourName = builder.destinationAt(slotOf("A12")).value(QStringLiteral("patchName")).toString();

    const auto entry = m_db.loadEntry(ids[0]);
    QVERIFY(entry.has_value());
    workspace.adopt(entry->patch(), PatchOrigin::library(ids[0]));
    QVERIFY(workspace.edit(QStringLiteral("Rename"), [](xpmodel::Xp60Patch& p) {
        p.setName(*xpmodel::PatchName::fromText("Both"));
    }));

    QCOMPARE(builder.destinationAt(slotOf("A11")).value(QStringLiteral("patchName")).toString(),
             QStringLiteral("Both"));
    QCOMPARE(builder.destinationAt(slotOf("A21")).value(QStringLiteral("patchName")).toString(),
             QStringLiteral("Both"));
    // The neighbour is a different Patch and must be left completely alone.
    const auto neighbour = builder.destinationAt(slotOf("A12"));
    QCOMPARE(neighbour.value(QStringLiteral("patchName")).toString(), neighbourName);
    QVERIFY(!neighbour.value(QStringLiteral("editing")).toBool());
    QVERIFY(!neighbour.value(QStringLiteral("edited")).toBool());
}

void TestPatchSync::savingTheWorkingPatchSettlesEveryViewOnTheSameName()
{
    const auto ids = importFixture();
    PatchWorkspace workspace;
    BankBuilderViewModel builder;
    builder.setDatabase(&m_db);
    builder.setWorkspace(&workspace);
    QVERIFY(builder.placePatch(slotOf("A35"), ids[0]));

    const auto entry = m_db.loadEntry(ids[0]);
    QVERIFY(entry.has_value());
    workspace.adopt(entry->patch(), PatchOrigin::library(ids[0]));
    QVERIFY(workspace.edit(QStringLiteral("Rename"), [](xpmodel::Xp60Patch& p) {
        p.setName(*xpmodel::PatchName::fromText("Kept"));
    }));
    QCOMPARE(workspace.studioState(), services::StudioState::Edited);
    QVERIFY(builder.destinationAt(slotOf("A35")).value(QStringLiteral("edited")).toBool());

    // Marking it saved is what the Editor does after the library write lands.
    workspace.markSaved(ids[0]);
    QCOMPARE(workspace.studioState(), services::StudioState::Saved);
    // The destination still shows the working name — it is the same name now —
    // and no longer claims unsaved changes.
    const auto destination = builder.destinationAt(slotOf("A35"));
    QCOMPARE(destination.value(QStringLiteral("patchName")).toString(), QStringLiteral("Kept"));
    QVERIFY(destination.value(QStringLiteral("editing")).toBool());
    QVERIFY(!destination.value(QStringLiteral("edited")).toBool());
}

// The overlay is additive. Without a workspace every view is exactly what it was
// before this architecture existed, which is what the screenshot harness and any
// non-editing context get.
void TestPatchSync::withoutAWorkspaceTheViewsAreExactlyWhatTheDatabaseSays()
{
    const auto ids = importFixture();
    LibraryListModel library;
    library.setDatabase(&m_db);
    BankBuilderViewModel builder;
    builder.setDatabase(&m_db);

    QVERIFY(builder.placePatch(slotOf("A35"), ids[0]));
    const auto destination = builder.destinationAt(slotOf("A35"));
    QCOMPARE(destination.value(QStringLiteral("patchName")).toString(),
             QString::fromStdString(m_db.record(ids[0])->name));
    QVERIFY(!destination.value(QStringLiteral("editing")).toBool());
    QVERIFY(!library.data(library.index(0, 0), LibraryListModel::EditingRole).toBool());

    // And the open-for-editing actions refuse rather than crashing.
    QVERIFY(!builder.editSlot(slotOf("A35")));
    QVERIFY(!library.editRow(0));
}

QTEST_MAIN(TestPatchSync)
#include "tst_patch_sync.moc"
