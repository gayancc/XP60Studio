// Phase 5 — librarian foundation: fingerprints, provenance, .syx import.
//
// The import path is exercised against tests/fixtures/xp60/user-bank-amal.syx,
// a real XP-60 user bank (see tests/fixtures/xp60/README.md), so the structure
// under test is the structure the instrument actually produced.

#include "library/LibraryEntry.h"
#include "library/PatchFingerprint.h"
#include "library/SyxImport.h"
#include "roland/RolandSysExMessage.h"
#include "xp60/Xp60Device.h"
#include "xpmodel/Xp60PatchCodec.h"
#include "xpmodel/Xp60PatchLayout.h"

#include <QFile>
#include <QTest>

#include <algorithm>
#include <optional>

using namespace xp60studio;
using library::LibraryEntry;
using library::PatchFingerprint;
using library::PatchUserMetadata;
using xpmodel::Xp60PatchLayout;

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

// A DT1 carrying `data` to `address`, encoded exactly as the instrument would.
roland::ByteVector dataSet(const roland::RolandAddress& address, roland::ByteVector data)
{
    const auto message = roland::RolandSysExMessage::dataSet(roland::RolandDeviceId::factoryDefault(),
                                                             xp60::modelId(), address, std::move(data));
    return message ? message->encode() : roland::ByteVector{};
}

void append(roland::ByteVector& out, const roland::ByteVector& more)
{
    out.insert(out.end(), more.begin(), more.end());
}

// A real Patch written to `base` as five DT1s, one per documented block —
// the shape the instrument itself produces. `omitBlock` leaves one out.
//
// The bytes come from a Patch decoded out of the fixture rather than from a
// constant fill: a block of identical bytes is not a valid Patch (a nibble
// parameter rejects any byte above 0FH), so filler would only ever exercise
// the codec's rejection path.
roland::ByteVector wholePatch(const roland::RolandAddress& base, const xpmodel::Xp60Patch& patch,
                              std::string_view omitBlock = {})
{
    const auto blockBytes = xpmodel::Xp60PatchCodec::blockBytes(patch);
    roland::ByteVector out;
    std::size_t index = 0;
    for (const auto& block : Xp60PatchLayout::blocks()) {
        if (block.name != omitBlock) {
            append(out, dataSet(*base.plus(block.offset), blockBytes[index]));
        }
        ++index;
    }
    return out;
}

} // namespace

class TestLibraryImport : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // Fingerprint
    void fingerprintIsStableAndDistinguishesParameters();
    void fingerprintIgnoresHowTheSysExWasDelivered();
    void fingerprintHexRoundTrips();

    // User metadata
    void userMetadataKeepsTagsExact();
    void editingUserMetadataLeavesTheParametersAlone();

    // Import — the real fixture
    void importsEveryUserPatchFromTheFixture();
    void preservesTheOriginalSysExByteForByte();
    void recordsProvenanceForEachEntry();
    void countsPerformanceDataAsUnattributedRatherThanGuessing();

    // Import — constructed edge cases
    void reportsAPartialPatchInsteadOfDroppingIt();
    void ignoresSlotsTheFileSaysNothingAbout();
    void findsExactDuplicatesWithinOneFile();
    void reportsStreamAnomalies();
    void skipsPerformancePartsUnlessAsked();

private:
    roland::ByteVector m_fixture;
    // Two different real Patches, used to construct the edge-case streams.
    std::vector<xpmodel::Xp60Patch> m_realPatches;
};

void TestLibraryImport::initTestCase()
{
    m_fixture = readFixture();
    QVERIFY2(!m_fixture.empty(), "golden fixture missing");

    const auto imported = library::importSyxStream(m_fixture);
    QVERIFY(imported.entries.size() >= 2);
    m_realPatches.push_back(imported.entries[0].patch());
    m_realPatches.push_back(imported.entries[1].patch());
    QVERIFY2(!(m_realPatches[0] == m_realPatches[1]), "the two sample patches must differ");
}

// ---------------------------------------------------------------------------
// Fingerprint
// ---------------------------------------------------------------------------

void TestLibraryImport::fingerprintIsStableAndDistinguishesParameters()
{
    const auto result = library::importSyxStream(m_fixture);
    QVERIFY(result.entries.size() >= 2);

    const auto& first = result.entries.front();
    QCOMPARE(PatchFingerprint::of(first.patch()), first.fingerprint());
    QCOMPARE(PatchFingerprint::of(first.patch()), PatchFingerprint::of(first.patch()));
    QVERIFY(!first.fingerprint().isNull());
    QCOMPARE(first.fingerprint().toHexString().size(), std::size_t{64});

    // One parameter apart is a different fingerprint.
    auto changed = first.patch();
    const int level = changed.raw(xpmodel::CommonParameter::PatchLevel);
    QVERIFY(changed.setRaw(xpmodel::CommonParameter::PatchLevel, level == 0 ? 1 : level - 1));
    QVERIFY(PatchFingerprint::of(changed) != first.fingerprint());
}

void TestLibraryImport::fingerprintIgnoresHowTheSysExWasDelivered()
{
    const auto result = library::importSyxStream(m_fixture);
    QVERIFY(!result.entries.empty());
    const auto& entry = result.entries.front();

    // Re-transmit the same Patch with a different device ID and a 16-byte
    // chunking, then re-import it. The delivery differs in every byte of
    // framing; the parameters do not, so the fingerprint must not move.
    const auto deviceId = roland::RolandDeviceId::fromDisplayNumber(21);
    QVERIFY(deviceId.has_value());
    const auto messages = xpmodel::Xp60PatchCodec::encodeToDataSets(
        entry.patch(), *deviceId, xp60::modelId(), Xp60PatchLayout::temporaryPatchAddress(), 16);
    QVERIFY(!messages.empty());

    roland::ByteVector stream;
    for (const auto& message : messages) {
        append(stream, message.encode());
    }
    const auto reimported = library::importSyxStream(stream);
    QCOMPARE(reimported.entries.size(), std::size_t{1});
    QCOMPARE(reimported.entries.front().fingerprint(), entry.fingerprint());
    QVERIFY(reimported.entries.front().hasSameParameters(entry));
    // ...while the bytes that carried it are plainly not the same.
    QVERIFY(reimported.entries.front().originalSysEx() != entry.originalSysEx());
}

void TestLibraryImport::fingerprintHexRoundTrips()
{
    const auto result = library::importSyxStream(m_fixture);
    QVERIFY(!result.entries.empty());
    const auto fingerprint = result.entries.front().fingerprint();

    const auto parsed = PatchFingerprint::fromHexString(fingerprint.toHexString());
    QVERIFY(parsed.has_value());
    QCOMPARE(*parsed, fingerprint);
    QCOMPARE(fingerprint.toShortString(), fingerprint.toHexString().substr(0, 8));

    QVERIFY(!PatchFingerprint::fromHexString("").has_value());
    QVERIFY(!PatchFingerprint::fromHexString(std::string(63, 'a')).has_value());
    QVERIFY(!PatchFingerprint::fromHexString(std::string(64, 'z')).has_value());
    QVERIFY(PatchFingerprint().isNull());
}

// ---------------------------------------------------------------------------
// User metadata
// ---------------------------------------------------------------------------

void TestLibraryImport::userMetadataKeepsTagsExact()
{
    PatchUserMetadata metadata;
    QVERIFY(metadata.addTag("pad"));
    QVERIFY(!metadata.addTag("pad"));   // already there
    QVERIFY(!metadata.addTag(""));      // an empty tag is not a tag
    QVERIFY(metadata.addTag("Pad"));    // different case is a different tag
    QCOMPARE(metadata.tags.size(), std::size_t{2});
    QVERIFY(metadata.hasTag("pad"));
    QVERIFY(metadata.removeTag("pad"));
    QVERIFY(!metadata.removeTag("pad"));
    QCOMPARE(metadata.tags.size(), std::size_t{1});

    QVERIFY(PatchUserMetadata::isValidRating(0));
    QVERIFY(PatchUserMetadata::isValidRating(5));
    QVERIFY(!PatchUserMetadata::isValidRating(6));
    QVERIFY(!PatchUserMetadata::isValidRating(-1));
}

void TestLibraryImport::editingUserMetadataLeavesTheParametersAlone()
{
    auto result = library::importSyxStream(m_fixture);
    QVERIFY(!result.entries.empty());
    auto& entry = result.entries.front();

    const auto fingerprintBefore = entry.fingerprint();
    const auto sysExBefore = entry.originalSysEx();
    const auto nameBefore = entry.name();

    entry.userMetadata().favourite = true;
    entry.userMetadata().rating = 4;
    entry.userMetadata().category = "Strings";
    QVERIFY(entry.userMetadata().addTag("live"));

    QCOMPARE(entry.fingerprint(), fingerprintBefore);
    QCOMPARE(entry.originalSysEx(), sysExBefore);
    QCOMPARE(entry.name(), nameBefore);
}

// ---------------------------------------------------------------------------
// Import — the real fixture
// ---------------------------------------------------------------------------

void TestLibraryImport::importsEveryUserPatchFromTheFixture()
{
    library::SyxImportOptions options;
    options.sourceName = "user-bank-amal.syx";
    const auto result = library::importSyxStream(m_fixture, options);

    // The fixture holds the 128 User Patches and nothing in the temporary area.
    QCOMPARE(result.entries.size(), std::size_t{128});
    QVERIFY(result.partial.empty());
    QVERIFY2(result.rejected.empty(), qPrintable(QString::fromStdString(result.summary())));
    QVERIFY(result.stream.isClean());

    for (std::size_t i = 0; i < result.entries.size(); ++i) {
        const auto& entry = result.entries[i];
        QVERIFY(entry.provenance().userNumber.has_value());
        QCOMPARE(*entry.provenance().userNumber, static_cast<int>(i) + 1);
        QCOMPARE(entry.provenance().address, *Xp60PatchLayout::userPatchAddress(static_cast<int>(i) + 1));
    }

    // tst_golden_fixture already proves all 128 decode without range warnings;
    // the importer must not introduce any of its own.
    QVERIFY2(result.warnings.empty(), qPrintable(QString::fromStdString(result.summary())));

    // The supplied bank contains 11 pairs of patches whose parameters are
    // byte-for-byte identical. That is a fact about the user's own data, not a
    // defect: real libraries accumulate copies. It is asserted here so a change
    // to what the fingerprint covers shows up as a change in this number.
    QCOMPARE(result.duplicates.size(), std::size_t{11});
    for (const auto& pair : result.duplicates) {
        QVERIFY(pair.firstIndex < pair.secondIndex);
        QVERIFY(result.entries[pair.firstIndex].hasSameParameters(result.entries[pair.secondIndex]));
        // Identical sounds in different slots: the provenance still separates them.
        QVERIFY(result.entries[pair.firstIndex].provenance().userNumber
                != result.entries[pair.secondIndex].provenance().userNumber);
    }
}

void TestLibraryImport::preservesTheOriginalSysExByteForByte()
{
    const auto result = library::importSyxStream(m_fixture);
    QVERIFY(!result.entries.empty());

    for (const auto& entry : result.entries) {
        // Five blocks arrived as five DT1s, and the entry kept them whole.
        QVERIFY(!entry.originalSysEx().empty());
        QCOMPARE(entry.originalSysEx().front(), roland::kSysExStart);
        QCOMPARE(entry.originalSysEx().back(), roland::kSysExEnd);

        // The kept bytes are a verbatim slice of the source, not a re-encoding.
        const auto& provenance = entry.provenance();
        QCOMPARE(provenance.sourceByteCount, static_cast<std::uint64_t>(entry.originalSysEx().size()));
        QVERIFY(provenance.sourceByteOffset + provenance.sourceByteCount <= m_fixture.size());
        const auto begin = m_fixture.begin() + static_cast<std::ptrdiff_t>(provenance.sourceByteOffset);
        const roland::ByteVector slice(begin, begin + static_cast<std::ptrdiff_t>(provenance.sourceByteCount));
        QCOMPARE(entry.originalSysEx(), slice);
    }
}

void TestLibraryImport::recordsProvenanceForEachEntry()
{
    library::SyxImportOptions options;
    options.sourceName = "user-bank-amal.syx";
    options.importedAt = std::chrono::system_clock::time_point{std::chrono::seconds{1'700'000'000}};
    const auto result = library::importSyxStream(m_fixture, options);
    QVERIFY(!result.entries.empty());

    // The fixture's own provenance record: device ID 17, single-byte model 6A.
    for (const auto& entry : result.entries) {
        const auto& provenance = entry.provenance();
        QCOMPARE(provenance.origin, library::PatchOrigin::ImportedFile);
        QCOMPARE(QString::fromStdString(provenance.sourceName), QStringLiteral("user-bank-amal.syx"));
        QCOMPARE(QString::fromStdString(provenance.sourceDigest),
                 QStringLiteral("13d709c210dce6403aebf60b07f23cc52feaa4ef0fb1844c926eb27fa6f09d53"));
        QCOMPARE(provenance.importedAt, *options.importedAt);
        QVERIFY(provenance.deviceId.has_value());
        QCOMPARE(provenance.deviceId->displayNumber(), 17);
        QVERIFY(provenance.modelId.has_value());
        QCOMPARE(provenance.modelId->size(), std::size_t{1});
    }

    QVERIFY(result.entries.front().provenance().describe().find("USER:001") != std::string::npos);
    QVERIFY(result.entries.back().provenance().describe().find("USER:128") != std::string::npos);
}

void TestLibraryImport::countsPerformanceDataAsUnattributedRatherThanGuessing()
{
    const auto result = library::importSyxStream(m_fixture);

    // 1314 data sets: 640 belong to the 128 Patches (5 blocks each); the rest
    // are Performance-area blocks this project has not transcribed. They are
    // counted, never decoded as something they might not be.
    QCOMPARE(result.stream.dataSetCount, std::size_t{1314});
    QCOMPARE(result.unattributedDataSets, std::size_t{1314 - 128 * 5});
    QVERIFY(result.summary().find("unattributed") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Import — constructed edge cases
// ---------------------------------------------------------------------------

void TestLibraryImport::reportsAPartialPatchInsteadOfDroppingIt()
{
    const auto base = *Xp60PatchLayout::userPatchAddress(3);
    const auto stream = wholePatch(base, m_realPatches[0], "Tone 3");

    const auto result = library::importSyxStream(stream);
    QVERIFY(result.entries.empty());
    QCOMPARE(result.partial.size(), std::size_t{1});

    const auto& partial = result.partial.front();
    QCOMPARE(partial.userNumber, std::optional<int>{3});
    QCOMPARE(partial.missingBlocks.size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(partial.missingBlocks.front()), QStringLiteral("Tone 3"));
    QVERIFY(partial.coveredBytes < partial.expectedBytes);
    QVERIFY(partial.describe().find("Tone 3") != std::string::npos);
    // The bytes are accounted for by the partial report, so they are not also
    // reported as bytes nobody claimed.
    QCOMPARE(result.unattributedDataSets, std::size_t{0});
    QVERIFY(!result.isClean());
}

void TestLibraryImport::ignoresSlotsTheFileSaysNothingAbout()
{
    const auto result = library::importSyxStream(wholePatch(*Xp60PatchLayout::userPatchAddress(64), m_realPatches[0]));
    QCOMPARE(result.entries.size(), std::size_t{1});
    QCOMPARE(result.entries.front().provenance().userNumber, std::optional<int>{64});
    // 127 untouched User slots and the temporary area are silent, not partial.
    QVERIFY(result.partial.empty());
    QVERIFY(result.rejected.empty());
}

void TestLibraryImport::findsExactDuplicatesWithinOneFile()
{
    roland::ByteVector stream;
    append(stream, wholePatch(*Xp60PatchLayout::userPatchAddress(1), m_realPatches[0]));
    append(stream, wholePatch(*Xp60PatchLayout::userPatchAddress(2), m_realPatches[1]));
    append(stream, wholePatch(*Xp60PatchLayout::userPatchAddress(3), m_realPatches[0]));

    const auto result = library::importSyxStream(stream);
    QCOMPARE(result.entries.size(), std::size_t{3});
    QCOMPARE(result.duplicates.size(), std::size_t{1});
    QCOMPARE(result.duplicates.front().firstIndex, std::size_t{0});
    QCOMPARE(result.duplicates.front().secondIndex, std::size_t{2});

    // Same parameters, different slots: the duplicate is in the sound, and the
    // provenance still says where each copy came from.
    QVERIFY(result.entries[0].hasSameParameters(result.entries[2]));
    QVERIFY(!result.entries[0].hasSameParameters(result.entries[1]));
    QCOMPARE(result.entries[0].provenance().userNumber, std::optional<int>{1});
    QCOMPARE(result.entries[2].provenance().userNumber, std::optional<int>{3});
}

void TestLibraryImport::reportsStreamAnomalies()
{
    roland::ByteVector stream;
    stream.push_back(0x42); // a stray data byte before anything else
    append(stream, wholePatch(*Xp60PatchLayout::userPatchAddress(1), m_realPatches[0]));
    auto corrupt = dataSet(*Xp60PatchLayout::userPatchAddress(2), roland::ByteVector(8, 0x01));
    corrupt[corrupt.size() - 2] ^= 0x01; // break the checksum
    append(stream, corrupt);

    const auto result = library::importSyxStream(stream);
    QCOMPARE(result.entries.size(), std::size_t{1});
    QVERIFY(!result.stream.isClean());
    QCOMPARE(result.stream.strayBytes, std::size_t{1});
    QCOMPARE(result.stream.rejectedRolandCount, std::size_t{1});
    // A message that failed validation never reaches the image, so USER:002 is
    // silent rather than partial: the anomaly is reported by the stream report.
    QVERIFY(result.partial.empty());
    QVERIFY(!result.isClean());
    QVERIFY(result.summary().find("not clean") != std::string::npos);
}

void TestLibraryImport::skipsPerformancePartsUnlessAsked()
{
    const roland::RolandAddress performancePart1{0x02, 0x00, 0x00, 0x00};
    const roland::RolandAddress rhythmPart{0x02, 0x09, 0x00, 0x00};
    roland::ByteVector stream;
    append(stream, wholePatch(performancePart1, m_realPatches[0]));
    append(stream, wholePatch(rhythmPart, m_realPatches[1]));

    const auto ignored = library::importSyxStream(stream);
    QVERIFY(ignored.entries.empty());
    QVERIFY(ignored.partial.empty());
    QCOMPARE(ignored.unattributedDataSets, std::size_t{10});

    library::SyxImportOptions options;
    options.includePerformanceParts = true;
    const auto included = library::importSyxStream(stream, options);
    // Part 1 is a Patch; Part 10 is the Rhythm Setup and stays untouched.
    QCOMPARE(included.entries.size(), std::size_t{1});
    QCOMPARE(included.entries.front().provenance().address, performancePart1);
    QVERIFY(!included.entries.front().provenance().userNumber.has_value());
    QCOMPARE(included.unattributedDataSets, std::size_t{5});
}

QTEST_MAIN(TestLibraryImport)
#include "tst_library_import.moc"
