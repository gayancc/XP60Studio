// Phase 8 — the XP-60 Rhythm Setup tables.
//
// The model and codec for a Rhythm Setup are not built yet; this proves the
// transcription itself against the two real User Rhythm Setups in the golden
// fixture, so the tables are known good before anything is built on them.

#include "xp60/Xp60Device.h"
#include "xpmodel/BlockCodec.h"
#include "xpmodel/SysExStream.h"
#include "xpmodel/generated/Xp60RhythmTables.h"

#include <QFile>
#include <QTest>

#include <set>

using namespace xp60studio;
namespace rhythm = xp60studio::xpmodel::xp60rhythm;

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

// The two User Rhythm Setups documented in ROLAND_XP60_PROTOCOL_FACTS.md §3.
roland::RolandAddress userRhythmSetup(int number)
{
    return roland::RolandAddress{0x10, static_cast<roland::Byte>(0x40 + number - 1), 0x00, 0x00};
}

} // namespace

class TestXp60Rhythm : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void tablesCoverTheBlocksRolandDeclares();
    void notesAreAddressedByMidiKeyNumber();
    void decodesBothUserRhythmSetupsInTheFixture();
    void roundTripsEveryNoteByteForByte();

private:
    xpmodel::MemoryImage m_image;
};

void TestXp60Rhythm::initTestCase()
{
    const auto bytes = readFixture();
    QVERIFY2(!bytes.empty(), "golden fixture missing");
    const std::vector<roland::RolandModelId> models{xp60::modelId()};
    const auto stream = xpmodel::parseSysExStream(bytes, models);
    QVERIFY(stream.isClean());
    m_image = xpmodel::imageFromStream(stream);
}

void TestXp60Rhythm::tablesCoverTheBlocksRolandDeclares()
{
    QCOMPARE(rhythm::kRhythmCommonSize, 12u);
    QCOMPARE(rhythm::kRhythmNoteSize, 58u);
    QCOMPARE(rhythm::rhythmCommonTable().blockSize(), rhythm::kRhythmCommonSize);
    QCOMPARE(rhythm::rhythmNoteTable().blockSize(), rhythm::kRhythmNoteSize);
    QVERIFY(rhythm::rhythmCommonTable().validate().empty());
    QVERIFY(rhythm::rhythmNoteTable().validate().empty());

    // A Rhythm Setup's Common block is its name and nothing else.
    QCOMPARE(rhythm::rhythmCommonTable().size(), std::size_t{12});
    QCOMPARE(rhythm::descriptor(rhythm::RhythmCommonParameter::RhythmName1).offset, 0u);

    // A Note is close to a Patch Tone but not the same, and the differences are
    // what make it a drum key.
    QCOMPARE(rhythm::descriptor(rhythm::RhythmNoteParameter::SourceKey).rawMax, 127);
    QCOMPARE(rhythm::descriptor(rhythm::RhythmNoteParameter::MuteGroup).rawMax, 32);
    QCOMPARE(rhythm::descriptor(rhythm::RhythmNoteParameter::EnvelopeMode).rawMax, 1);
}

void TestXp60Rhythm::notesAreAddressedByMidiKeyNumber()
{
    QCOMPARE(rhythm::kFirstRhythmKey, 35);
    QCOMPARE(rhythm::kLastRhythmKey, 98);
    QCOMPARE(rhythm::kRhythmNoteCount, 64);
    QCOMPARE(rhythm::kRhythmNoteOffsets.size(), std::size_t{64});

    // The offset *is* the key: Key# 35 lives at Roland 23 00 because 0x23 is 35.
    for (int key = rhythm::kFirstRhythmKey; key <= rhythm::kLastRhythmKey; ++key) {
        const auto offset = rhythm::kRhythmNoteOffsets[static_cast<std::size_t>(key - rhythm::kFirstRhythmKey)];
        QCOMPARE(offset, static_cast<std::uint32_t>(key) * 128u);
    }
    QCOMPARE(rhythm::kRhythmSetupSpan, 98u * 128u + rhythm::kRhythmNoteSize);
}

void TestXp60Rhythm::decodesBothUserRhythmSetupsInTheFixture()
{
    for (int setup = 1; setup <= 2; ++setup) {
        const auto base = userRhythmSetup(setup);
        const auto commonBytes = m_image.read(base, rhythm::kRhythmCommonSize);
        QVERIFY2(commonBytes.has_value(), "the fixture holds both User Rhythm Setups");
        const auto common = xpmodel::BlockCodec::decode(rhythm::rhythmCommonTable(), *commonBytes);
        QVERIFY(common.ok());
        // Real user data must sit inside every documented range: a warning here
        // would mean the transcription disagrees with the instrument.
        QVERIFY2(common.issues.empty(), qPrintable(QStringLiteral("Rhythm %1 Common").arg(setup)));

        int notes = 0;
        for (int key = rhythm::kFirstRhythmKey; key <= rhythm::kLastRhythmKey; ++key) {
            const auto address = base.plus(static_cast<std::uint32_t>(key) * 128u);
            QVERIFY(address.has_value());
            const auto bytes = m_image.read(*address, rhythm::kRhythmNoteSize);
            QVERIFY2(bytes.has_value(),
                     qPrintable(QStringLiteral("Rhythm %1 Key# %2 missing").arg(setup).arg(key)));
            const auto decoded = xpmodel::BlockCodec::decode(rhythm::rhythmNoteTable(), *bytes);
            QVERIFY(decoded.ok());
            QVERIFY2(decoded.issues.empty(),
                     qPrintable(QStringLiteral("Rhythm %1 Key# %2 out of range").arg(setup).arg(key)));
            ++notes;
        }
        QCOMPARE(notes, 64);
    }
}

// Encoding what was decoded must reproduce the instrument's bytes exactly: the
// tables describe the block, they do not normalise it.
void TestXp60Rhythm::roundTripsEveryNoteByteForByte()
{
    for (int setup = 1; setup <= 2; ++setup) {
        const auto base = userRhythmSetup(setup);
        const auto commonBytes = *m_image.read(base, rhythm::kRhythmCommonSize);
        const auto common = xpmodel::BlockCodec::decode(rhythm::rhythmCommonTable(), commonBytes);
        QVERIFY(xpmodel::BlockCodec::encode(*common.values) == commonBytes);

        for (int key = rhythm::kFirstRhythmKey; key <= rhythm::kLastRhythmKey; ++key) {
            const auto address = *base.plus(static_cast<std::uint32_t>(key) * 128u);
            const auto bytes = *m_image.read(address, rhythm::kRhythmNoteSize);
            const auto decoded = xpmodel::BlockCodec::decode(rhythm::rhythmNoteTable(), bytes);
            QVERIFY2(xpmodel::BlockCodec::encode(*decoded.values) == bytes,
                     qPrintable(QStringLiteral("Rhythm %1 Key# %2 did not round-trip").arg(setup).arg(key)));
        }
    }
}

QTEST_MAIN(TestXp60Rhythm)
#include "tst_xp60_rhythm.moc"
