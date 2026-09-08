#include "library/Setlist.h"

#include <QCoreApplication>

namespace xp60studio::library {

namespace {

// The instrument's own limits. Stated here because a cue is refused against
// them rather than clamped into a slot the musician did not choose.
constexpr int kUserPatchCount = 128;
constexpr int kUserPerformanceCount = 32;

QString toQt(const std::string& value) { return QString::fromStdString(value); }

QString displayName(const std::string& name, const QString& fallback)
{
    return name.empty() ? fallback : toQt(name);
}

} // namespace

std::string_view liveTargetKindName(LiveTargetKind kind) noexcept
{
    switch (kind) {
    case LiveTargetKind::CarryPrevious:
        return "Carry previous";
    case LiveTargetKind::LibraryPatch:
        return "Library Patch";
    case LiveTargetKind::UserPatchSlot:
        return "USER Patch";
    case LiveTargetKind::UserPerformanceSlot:
        return "USER Performance";
    }
    return "Unknown";
}

bool LiveTarget::isValid() const noexcept
{
    switch (kind) {
    case LiveTargetKind::CarryPrevious:
        return true;
    case LiveTargetKind::LibraryPatch:
        // A deleted row is not invalid, it is missing: the cue is still a real
        // cue that names a real sound, and saying so is the point.
        return true;
    case LiveTargetKind::UserPatchSlot:
        return userNumber >= 1 && userNumber <= kUserPatchCount;
    case LiveTargetKind::UserPerformanceSlot:
        return userNumber >= 1 && userNumber <= kUserPerformanceCount;
    }
    return false;
}

QString LiveTarget::describe() const
{
    const auto named = toQt(name);
    switch (kind) {
    case LiveTargetKind::CarryPrevious:
        return QCoreApplication::translate("Setlist", "Carry previous sound");
    case LiveTargetKind::LibraryPatch:
        if (isMissing()) {
            return QCoreApplication::translate("Setlist", "%1 — no longer in the library")
                .arg(named.isEmpty() ? QCoreApplication::translate("Setlist", "Deleted Patch")
                                     : named);
        }
        return named.isEmpty() ? QCoreApplication::translate("Setlist", "Library Patch") : named;
    case LiveTargetKind::UserPatchSlot: {
        const auto slot = QStringLiteral("USER:%1").arg(userNumber, 3, 10, QLatin1Char('0'));
        return named.isEmpty() ? slot : QStringLiteral("%1 %2").arg(slot, named);
    }
    case LiveTargetKind::UserPerformanceSlot: {
        const auto slot = QCoreApplication::translate("Setlist", "PERF USER:%1")
                              .arg(userNumber, 2, 10, QLatin1Char('0'));
        return named.isEmpty() ? slot : QStringLiteral("%1 %2").arg(slot, named);
    }
    }
    return {};
}

QString SetlistCue::label() const
{
    return QStringLiteral("%1 · %2")
        .arg(displayName(songName, QCoreApplication::translate("Setlist", "Untitled song")),
             displayName(sectionName, QCoreApplication::translate("Setlist", "Untitled section")));
}

int Setlist::cueCount() const noexcept
{
    int total = 0;
    for (const auto& song : songs) {
        total += static_cast<int>(song.sections.size());
    }
    return total;
}

std::vector<SetlistCue> Setlist::cues() const
{
    std::vector<SetlistCue> list;
    list.reserve(static_cast<std::size_t>(cueCount()));

    // The sound in force, carried forward across songs. A setlist is one
    // running order, so a song that opens on CarryPrevious keeps whatever the
    // last song ended on — which is what actually happens on stage.
    LiveTarget inForce;

    for (std::size_t songIndex = 0; songIndex < songs.size(); ++songIndex) {
        const auto& song = songs[songIndex];
        for (std::size_t sectionIndex = 0; sectionIndex < song.sections.size(); ++sectionIndex) {
            const auto& section = song.sections[sectionIndex];
            if (section.target.selectsSomething()) {
                inForce = section.target;
            }
            SetlistCue cue;
            cue.index = static_cast<int>(list.size());
            cue.songIndex = static_cast<int>(songIndex);
            cue.sectionIndex = static_cast<int>(sectionIndex);
            cue.songName = song.name;
            cue.sectionName = section.name;
            cue.note = section.note;
            cue.target = section.target;
            cue.effectiveTarget = inForce;
            list.push_back(std::move(cue));
        }
    }
    return list;
}

std::optional<SetlistCue> Setlist::cueAt(int index) const
{
    if (index < 0) {
        return std::nullopt;
    }
    // Built rather than indexed into a cache: the effective sound depends on
    // everything before it, so there is no cheaper honest answer, and a setlist
    // is a few dozen cues.
    const auto list = cues();
    if (static_cast<std::size_t>(index) >= list.size()) {
        return std::nullopt;
    }
    return list[static_cast<std::size_t>(index)];
}

std::vector<SetlistProblem> Setlist::problems() const
{
    std::vector<SetlistProblem> found;
    if (songs.empty()) {
        found.push_back({SetlistProblemKind::NoSongs, -1, -1,
                         QCoreApplication::translate("Setlist", "This setlist has no songs.")});
        return found;
    }

    bool anythingSelectedYet = false;
    // The opening of a setlist is one fault, not one per blank cue. Reported
    // against the first cue and then not again: a screen listing it three times
    // for three leading carries buries the problems that differ.
    bool openingReported = false;
    for (std::size_t songIndex = 0; songIndex < songs.size(); ++songIndex) {
        const auto& song = songs[songIndex];
        const int s = static_cast<int>(songIndex);
        if (song.name.empty()) {
            found.push_back({SetlistProblemKind::UnnamedSong, s, -1,
                             QCoreApplication::translate("Setlist", "Song %1 has no name.")
                                 .arg(s + 1)});
        }
        if (song.sections.empty()) {
            found.push_back({SetlistProblemKind::SongWithNoSections, s, -1,
                             QCoreApplication::translate("Setlist", "\"%1\" has no sections.")
                                 .arg(displayName(song.name,
                                                  QCoreApplication::translate(
                                                      "Setlist", "Song %1").arg(s + 1)))});
        }
        for (std::size_t sectionIndex = 0; sectionIndex < song.sections.size(); ++sectionIndex) {
            const auto& section = song.sections[sectionIndex];
            const int t = static_cast<int>(sectionIndex);
            const auto where = QStringLiteral("%1 · %2").arg(
                displayName(song.name,
                            QCoreApplication::translate("Setlist", "Song %1").arg(s + 1)),
                displayName(section.name,
                            QCoreApplication::translate("Setlist", "Section %1").arg(t + 1)));
            if (section.name.empty()) {
                found.push_back({SetlistProblemKind::UnnamedSection, s, t,
                                 QCoreApplication::translate("Setlist", "%1: the section has no name.")
                                     .arg(where)});
            }
            if (!section.target.selectsSomething()) {
                if (!anythingSelectedYet && !openingReported) {
                    openingReported = true;
                    // The very first cue cannot carry a sound from nowhere. On
                    // stage this is the difference between opening on the right
                    // sound and opening on whatever was left on the instrument.
                    found.push_back(
                        {SetlistProblemKind::NothingToCarry, s, t,
                         QCoreApplication::translate(
                             "Setlist",
                             "%1 carries the previous sound, but nothing before it selects one.")
                             .arg(where)});
                }
                continue;
            }
            anythingSelectedYet = true;
            if (section.target.isMissing()) {
                found.push_back(
                    {SetlistProblemKind::MissingLibraryPatch, s, t,
                     QCoreApplication::translate("Setlist",
                                                 "%1 calls for \"%2\", which is no longer in the library.")
                         .arg(where, toQt(section.target.name))});
            } else if (!section.target.isValid()) {
                found.push_back(
                    {SetlistProblemKind::SlotOutOfRange, s, t,
                     QCoreApplication::translate("Setlist", "%1 calls for %2 slot %3, which the XP-60 does not have.")
                         .arg(where, QString::fromUtf8(liveTargetKindName(section.target.kind).data(),
                                                       static_cast<qsizetype>(
                                                           liveTargetKindName(section.target.kind).size())))
                         .arg(section.target.userNumber)});
            }
        }
    }
    return found;
}

QStringList Setlist::problemMessages() const
{
    QStringList messages;
    for (const auto& problem : problems()) {
        messages.append(problem.message);
    }
    return messages;
}

bool Setlist::isPlayable() const { return problems().empty(); }

} // namespace xp60studio::library
