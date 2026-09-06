#include "presentation/ExpansionViewModel.h"

#include "library/ExpansionBoardCatalog.h"
#include "xpmodel/Xp60WaveIdentifier.h"

#include <set>

namespace xp60studio::presentation {

using library::ExpansionProfile;
using library::ToneCompatibility;

namespace {

QString toQt(const std::string& text)
{
    return QString::fromStdString(text);
}

QString toQt(std::string_view text)
{
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

QString joinGroups(const std::set<int>& groups)
{
    QStringList parts;
    for (const int group : groups) {
        parts.append(QString::number(group));
    }
    return parts.join(QStringLiteral(", "));
}

// A tone-name for a compatibility verdict, matching the StatusPill vocabulary.
QString toneFor(ToneCompatibility status)
{
    switch (status) {
    case ToneCompatibility::Internal:
        return QStringLiteral("neutral");
    case ToneCompatibility::ExpansionAvailable:
        return QStringLiteral("success");
    case ToneCompatibility::ExpansionMissing:
        return QStringLiteral("error");
    case ToneCompatibility::ExpansionUnknown:
        return QStringLiteral("warning");
    case ToneCompatibility::Disabled:
        return QStringLiteral("neutral");
    }
    return QStringLiteral("neutral");
}

} // namespace

ExpansionViewModel::ExpansionViewModel(QObject* parent)
    : QObject(parent)
{
}

void ExpansionViewModel::setDatabase(library::LibraryDatabase* database)
{
    if (m_database == database) {
        return;
    }
    m_database = database;
    load();
}

void ExpansionViewModel::setWorkspace(services::PatchWorkspace* workspace)
{
    if (m_workspace == workspace) {
        return;
    }
    if (m_workspace) {
        disconnect(m_workspace, nullptr, this, nullptr);
    }
    m_workspace = workspace;
    if (m_workspace) {
        connect(m_workspace, &services::PatchWorkspace::changed, this, &ExpansionViewModel::analysisChanged);
        connect(m_workspace, &services::PatchWorkspace::originChanged, this, &ExpansionViewModel::analysisChanged);
    }
    emit analysisChanged();
}

void ExpansionViewModel::load()
{
    if (!m_database) {
        m_profile = ExpansionProfile{};
    } else if (const auto stored = m_database->loadExpansionProfile()) {
        m_profile = *stored;
    }
    emit profileChanged();
    emit analysisChanged();
}

void ExpansionViewModel::persist()
{
    if (m_database) {
        // A failed write leaves the in-memory profile as the user set it; the
        // next change tries again. Losing the edit would be worse than a
        // profile that is briefly only in memory.
        (void)m_database->saveExpansionProfile(m_profile);
    }
    emit profileChanged();
    emit analysisChanged();
}

// ---------------------------------------------------------------------------
// The four slots
// ---------------------------------------------------------------------------

QVariantList ExpansionViewModel::slotList() const
{
    QVariantList list;
    for (int slot = 1; slot <= library::kSlotCount; ++slot) {
        const auto& board = m_profile.board(slot);
        QVariantMap map;
        map.insert(QStringLiteral("slot"), slot);
        map.insert(QStringLiteral("label"), toQt(library::slotLabel(slot)));
        map.insert(QStringLiteral("name"), toQt(board.name));
        map.insert(QStringLiteral("installed"), !board.name.empty());
        map.insert(QStringLiteral("groupKnown"), board.waveGroupId.has_value());
        map.insert(QStringLiteral("groupId"), board.waveGroupId ? *board.waveGroupId : -1);
        list.append(map);
    }
    return list;
}

QString ExpansionViewModel::advice() const
{
    if (m_profile.isEmpty()) {
        return tr("Tell XP60Studio which SR-JV80 boards are in EXP-A to EXP-D. Until then it cannot say whether a "
                  "Patch that uses expansion waves will play on your instrument — and it will say so rather than "
                  "guess.");
    }
    if (m_profile.anyGroupUnknown()) {
        return tr("One of your boards has no wave group yet, so any of them could be the one a Patch is asking "
                  "for. Select a wave from that board in a Tone on the XP-60, fetch the Patch, and press Learn.");
    }
    return tr("Every declared board has a wave group, so XP60Studio can say exactly which Patches need something "
              "you do not have.");
}

QString ExpansionViewModel::browserNote() const
{
    if (m_profile.isEmpty()) {
        return tr("No expansion boards declared. Say what is in EXP-A to EXP-D in the Expansion Manager and "
                  "XP60Studio can name the waves on the boards it holds Roland's list for.");
    }

    QStringList named;
    QStringList unnamed;
    for (int slot = 1; slot <= library::kSlotCount; ++slot) {
        const auto& board = m_profile.board(slot);
        if (board.name.empty()) {
            continue;
        }
        const QString label = tr("%1 · %2").arg(toQt(library::slotLabel(slot)), toQt(board.name));
        if (board.waveGroupId && library::hasSrJv80WaveList(*board.waveGroupId)) {
            named.append(tr("%1 (%n wave(s))", "", library::srJv80WaveCount(*board.waveGroupId)).arg(label));
        } else if (!board.waveGroupId) {
            unnamed.append(tr("%1 — wave group not known yet").arg(label));
        } else {
            unnamed.append(label);
        }
    }

    QStringList parts;
    if (!named.isEmpty()) {
        parts.append(tr("Waves can be named on: %1.").arg(named.join(QStringLiteral("; "))));
    }
    if (!unnamed.isEmpty()) {
        // Two different reasons land here, and the sentence covers both without
        // pretending either is the musician's fault: no Waveform List held, or
        // no wave group established for that board yet.
        parts.append(tr("Not on: %1 — XP60Studio does not hold Roland's Waveform List for %2. Tones using them "
                        "keep working and still show their wave number.",
                        "", static_cast<int>(unnamed.size()))
                         .arg(unnamed.join(QStringLiteral("; ")),
                              unnamed.size() == 1 ? tr("it") : tr("them")));
    }
    return parts.join(QStringLiteral(" "));
}

QVariantList ExpansionViewModel::knownBoards() const
{
    QVariantList list;
    for (const auto& board : library::srJv80Boards()) {
        const auto name = library::srJv80BoardName(board.number);
        QVariantMap map;
        map.insert(QStringLiteral("number"), board.number);
        map.insert(QStringLiteral("title"), toQt(board.title));
        map.insert(QStringLiteral("name"), name ? toQt(*name) : QString());
        // The wave group choosing this board would record. Shown in the picker
        // so the inference is visible rather than applied behind the musician.
        map.insert(QStringLiteral("waveGroupId"), board.number);
        list.append(map);
    }
    return list;
}

bool ExpansionViewModel::declareBoard(int slot, int boardNumber)
{
    const auto name = library::srJv80BoardName(boardNumber);
    if (!name) {
        return false;
    }
    return setBoard(slot, toQt(*name), boardNumber);
}

QString ExpansionViewModel::describeGroup(int waveGroupId) const
{
    return toQt(library::describeWaveGroup(waveGroupId));
}

QString ExpansionViewModel::describeWave(int waveGroupId, int rawWaveNumber) const
{
    return toQt(library::describeExpansionWave(waveGroupId, rawWaveNumber));
}

bool ExpansionViewModel::setBoard(int slot, const QString& name, int waveGroupId)
{
    const auto group = waveGroupId >= 0 ? std::optional<int>{waveGroupId} : std::nullopt;
    if (!m_profile.setBoard(slot, name.trimmed().toStdString(), group)) {
        return false;
    }
    persist();
    return true;
}

bool ExpansionViewModel::clearSlot(int slot)
{
    if (!m_profile.clearSlot(slot)) {
        return false;
    }
    persist();
    return true;
}

bool ExpansionViewModel::setWaveGroup(int slot, int waveGroupId)
{
    if (!m_profile.setWaveGroup(slot, waveGroupId)) {
        return false;
    }
    persist();
    return true;
}

// ---------------------------------------------------------------------------
// The Patch on screen
// ---------------------------------------------------------------------------

QVariantMap ExpansionViewModel::currentPatch() const
{
    QVariantMap result;
    if (!m_workspace || !m_workspace->hasPatch()) {
        return result;
    }
    const auto report = library::analysePatch(m_workspace->working(), m_profile);

    result.insert(QStringLiteral("usesExpansion"), report.usesExpansion());
    result.insert(QStringLiteral("playable"), report.playable());
    result.insert(QStringLiteral("undecided"), report.undecided());
    result.insert(QStringLiteral("summary"), toQt(report.summary()));
    result.insert(QStringLiteral("requiredGroups"), joinGroups(report.requiredGroups));
    result.insert(QStringLiteral("missingGroups"), joinGroups(report.missingGroups));

    QVariantList tones;
    for (const auto& tone : report.tones) {
        QVariantMap map;
        map.insert(QStringLiteral("toneNumber"), tone.toneNumber);
        map.insert(QStringLiteral("enabled"), tone.enabled);
        map.insert(QStringLiteral("status"), toQt(library::toneCompatibilityName(tone.status)));
        map.insert(QStringLiteral("label"), toQt(library::toneCompatibilityLabel(tone.status)));
        map.insert(QStringLiteral("tone"), toneFor(tone.status));
        map.insert(QStringLiteral("groupId"), tone.waveGroupId ? *tone.waveGroupId : -1);
        map.insert(QStringLiteral("numberRaw"), tone.waveNumberRaw ? *tone.waveNumberRaw : -1);
        // Roland's own name for the wave, when this project holds that board's
        // Waveform List. Empty rather than invented when it does not.
        map.insert(QStringLiteral("waveName"),
                   tone.waveGroupId && tone.waveNumberRaw
                       ? [&] {
                             const auto name = library::srJv80WaveName(*tone.waveGroupId, *tone.waveNumberRaw + 1);
                             return name ? toQt(*name) : QString();
                         }()
                       : QString());
        map.insert(QStringLiteral("waveDescription"),
                   tone.waveGroupId && tone.waveNumberRaw
                       ? toQt(library::describeExpansionWave(*tone.waveGroupId, *tone.waveNumberRaw))
                       : QString());
        map.insert(QStringLiteral("slot"), tone.providedBySlot ? *tone.providedBySlot : 0);
        map.insert(QStringLiteral("slotLabel"),
                   tone.providedBySlot ? toQt(library::slotLabel(*tone.providedBySlot)) : QString());
        tones.append(map);
    }
    result.insert(QStringLiteral("tones"), tones);
    return result;
}

QVariantList ExpansionViewModel::learnableGroups() const
{
    QVariantList list;
    if (!m_workspace || !m_workspace->hasPatch()) {
        return list;
    }
    const auto report = library::analysePatch(m_workspace->working(), m_profile);
    for (const int group : report.requiredGroups) {
        // A group a declared board already answers for has nothing to learn.
        if (m_profile.providesGroup(group)) {
            continue;
        }
        list.append(group);
    }
    return list;
}

bool ExpansionViewModel::learnFromCurrentPatch(int slot)
{
    if (!m_workspace || !m_workspace->hasPatch() || m_profile.board(slot).name.empty()) {
        return false;
    }
    const auto report = library::analysePatch(m_workspace->working(), m_profile);

    // Only groups nothing already claims are candidates: a group another board
    // answers for cannot also be this one's.
    std::set<int> candidates;
    for (const int group : report.requiredGroups) {
        if (!m_profile.providesGroup(group)) {
            candidates.insert(group);
        }
    }
    // Exactly one, or there is nothing unambiguous to learn. Picking the first
    // of several would make every later verdict rest on a coin toss.
    if (candidates.size() != 1) {
        return false;
    }
    if (!m_profile.setWaveGroup(slot, *candidates.begin())) {
        return false;
    }
    persist();
    return true;
}

QString ExpansionViewModel::learnAdvice(int slot) const
{
    if (m_profile.board(slot).name.empty()) {
        return tr("Name the board in this slot first.");
    }
    if (!m_workspace || !m_workspace->hasPatch()) {
        return tr("Fetch a Patch from the XP-60 that uses a wave from this board, then press Learn.");
    }
    const auto groups = learnableGroups();
    if (groups.isEmpty()) {
        return tr("The Patch on screen uses no expansion wave that is still unaccounted for, so there is nothing "
                  "to learn from it. Select a wave from this board in a Tone on the instrument and fetch the "
                  "Patch again.");
    }
    if (groups.size() > 1) {
        QStringList parts;
        for (const auto& group : groups) {
            parts.append(group.toString());
        }
        return tr("This Patch uses %n unaccounted-for wave group(s) (%1), so XP60Studio cannot tell which one is "
                  "this board. Fetch a Patch that uses only this board.",
                  "", static_cast<int>(groups.size()))
            .arg(parts.join(QStringLiteral(", ")));
    }
    return tr("Records that %1 answers to wave group %2, read from the Patch on screen.")
        .arg(toQt(library::slotLabel(slot)), groups.first().toString());
}

} // namespace xp60studio::presentation
