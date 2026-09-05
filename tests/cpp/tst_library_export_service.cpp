#include "library/LibraryDatabase.h"
#include "services/LibraryExportService.h"
#include "xp60/Xp60Device.h"
#include "xpmodel/Xp60PatchCodec.h"
#include "xpmodel/Xp60PatchLayout.h"

#include <QTemporaryDir>
#include <QtTest>

#include <random>

using namespace xp60studio;
using namespace xp60studio::library;
using namespace xp60studio::services;

namespace {

// A Patch whose bytes are in range but not all identical, so an export that
// dropped or reordered anything would be visible.
xpmodel::Xp60Patch makePatch(unsigned seed, const std::string& name)
{
    const auto base = xpmodel::Xp60PatchLayout::temporaryPatchAddress();
    std::mt19937 rng(seed);
    xpmodel::MemoryImage image;
    for (const auto& block : xpmodel::Xp60PatchLayout::blocks()) {
        roland::ByteVector bytes(block.size, 0);
        for (const auto& p : block.table->parameters()) {
            std::uniform_int_distribution<int> dist(p.rawMin, p.rawMax);
            xpmodel::BlockCodec::writeRaw(p, dist(rng), bytes);
        }
        if (!block.tone) {
            // Patch names are exactly 12 characters; fromText refuses longer.
            const auto parsed = xpmodel::PatchName::fromText(name);
            Q_ASSERT(parsed.has_value());
            const auto patchName = parsed->bytes();
            std::copy(patchName.begin(), patchName.end(), bytes.begin());
        }
        image.write(*base.plus(block.offset), bytes);
    }
    return *xpmodel::Xp60PatchCodec::decode(image, base).patch;
}

LibraryEntry makeEntry(unsigned seed, const std::string& name)
{
    PatchProvenance provenance;
    provenance.origin = PatchOrigin::FetchedFromDevice;
    provenance.sourceName = "test";
    provenance.address = xpmodel::Xp60PatchLayout::temporaryPatchAddress();
    provenance.deviceId = roland::RolandDeviceId::factoryDefault();
    provenance.modelId = xp60::modelId();
    const auto patch = makePatch(seed, name);
    const auto messages = xpmodel::Xp60PatchCodec::encodeToDataSets(patch, roland::RolandDeviceId::factoryDefault(),
        xp60::modelId(), provenance.address, 128);
    roland::ByteVector raw;
    for (const auto& message : messages) {
        const auto bytes = message.encode();
        raw.insert(raw.end(), bytes.begin(), bytes.end());
    }
    return LibraryEntry(patch, raw, provenance);
}

struct Fixture
{
    QTemporaryDir dir;
    LibraryDatabase database;
    std::unique_ptr<LibraryExportService> service;
    std::vector<std::int64_t> ids;

    Fixture()
    {
        const bool opened = database.open(dir.filePath("library.db"));
        Q_ASSERT(opened);
        Q_UNUSED(opened)
        service = std::make_unique<LibraryExportService>(database);
        for (unsigned i = 0; i < 3; ++i) {
            const auto id = database.insert(makeEntry(i + 1, "Exported " + std::to_string(i)));
            Q_ASSERT(id.has_value());
            ids.push_back(*id);
        }
    }

    [[nodiscard]] QString target(const QString& name) const { return dir.filePath(name); }
};

} // namespace

class LibraryExportServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void writesTheRequestedPatches()
    {
        Fixture f;
        const auto path = f.target("all.syx");
        const auto result = f.service->exportToFile(f.ids, path);
        QVERIFY2(result.ok, qPrintable(result.error));
        QCOMPARE(result.patchCount, std::size_t(3));
        QVERIFY(result.byteCount > 0);
        QVERIFY(result.missingIds.empty());

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto bytes = file.readAll();
        QCOMPARE(static_cast<std::size_t>(bytes.size()), result.byteCount);
        QCOMPARE(static_cast<unsigned char>(bytes.front()), 0xF0);
        QCOMPARE(static_cast<unsigned char>(bytes.back()), 0xF7);
    }

    // A file with fewer Patches than were asked for is worse than no file:
    // nothing downstream would reveal the omission.
    void refusesWhenAnEntryCannotBeLoaded()
    {
        Fixture f;
        auto ids = f.ids;
        const std::int64_t missing = 999999;
        ids.push_back(missing);
        const auto path = f.target("partial.syx");

        const auto result = f.service->exportToFile(ids, path);
        QVERIFY(!result.ok);
        QCOMPARE(result.missingIds.size(), std::size_t(1));
        QCOMPARE(result.missingIds.front(), missing);
        QVERIFY(result.error.contains("nothing written"));
        QVERIFY(!QFile::exists(path)); // and nothing was written
    }

    void refusesAnEmptySelectionAndAnEmptyPath()
    {
        Fixture f;
        const auto empty = f.service->exportToFile({}, f.target("none.syx"));
        QVERIFY(!empty.ok);
        QVERIFY(!QFile::exists(f.target("none.syx")));

        const auto noPath = f.service->exportToFile(f.ids, QString());
        QVERIFY(!noPath.ok);
    }

    void reportsAnUnwritableDestinationWithoutThrowing()
    {
        Fixture f;
        // A path inside a file rather than a directory cannot be created.
        const auto blocker = f.target("blocker.syx");
        QFile file(blocker);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        const auto result = f.service->exportToFile(f.ids, blocker + "/inside.syx");
        QVERIFY(!result.ok);
        QVERIFY(!result.error.isEmpty());
    }

    // The export must carry through what it changed rather than reporting a
    // clean run: exporting several Patches to one edit buffer is refused, and
    // re-addressing to User slots is a note.
    void surfacesWhatTheExportChangedOrRefused()
    {
        Fixture f;
        SyxExportOptions toEditBuffer;
        toEditBuffer.source = SyxExportSource::ReencodedFromModel;
        toEditBuffer.target.kind = SyxExportTarget::Kind::TemporaryPatch;
        const auto refused = f.service->exportToFile(f.ids, f.target("temp.syx"), toEditBuffer);
        QVERIFY(!refused.ok); // three Patches cannot share one edit buffer
        QVERIFY(!QFile::exists(f.target("temp.syx")));

        SyxExportOptions toBank;
        toBank.source = SyxExportSource::ReencodedFromModel;
        toBank.target.kind = SyxExportTarget::Kind::UserBankFrom;
        toBank.target.firstUserNumber = 10;
        const auto moved = f.service->exportToFile(f.ids, f.target("bank.syx"), toBank);
        QVERIFY2(moved.ok, qPrintable(moved.error));
        QVERIFY(!moved.notes.isEmpty()); // an export that moves a Patch says so
    }

    void singleEntryExportSummaryReadsNaturally()
    {
        Fixture f;
        const auto result = f.service->exportToFile({f.ids.front()}, f.target("one.syx"));
        QVERIFY2(result.ok, qPrintable(result.error));
        QCOMPARE(result.patchCount, std::size_t(1));
        QVERIFY(result.summary().contains("1 patch,"));
    }
};

QTEST_MAIN(LibraryExportServiceTest)
#include "tst_library_export_service.moc"
