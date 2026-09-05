#pragma once

#include "library/ExpansionProfile.h"
#include "library/LibraryDatabase.h"
#include "library/PatchCompatibility.h"
#include "services/PatchWorkspace.h"

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace xp60studio::presentation {

// The Expansion Manager: what is in this XP-60's four Wave Expansion slots, and
// what that means for the Patch on screen.
//
// ── Why this screen asks instead of detecting ───────────────────────────────
//
// A Tone names an expansion wave by Wave Group ID, and which board answers to
// which ID is not documented — see `library::ExpansionProfile` and
// `docs/protocol/ROLAND_XP60_PROTOCOL_FACTS.md` §7, where the obvious
// "ID is the SR-JV80 board number" hypothesis is laid out and refused because
// the golden fixture uses group 97 and no such board exists.
//
// So the musician tells XP60Studio what they own. What XP60Studio can do in
// return is *learn* the group without anyone guessing: select a wave from a
// known board in a Tone on the instrument's front panel, fetch the temporary
// Patch, and read the group ID straight out of it. `learnFromCurrentPatch()` is
// that — evidence from the user's own instrument rather than a table this
// project could not honestly write.
class ExpansionViewModel : public QObject
{
    Q_OBJECT

    // Four entries, EXP-A..EXP-D in order. Each:
    // {slot, label, name, installed, groupId (-1 when unknown), groupKnown}
    Q_PROPERTY(QVariantList slots READ slotList NOTIFY profileChanged)
    Q_PROPERTY(int installedCount READ installedCount NOTIFY profileChanged)
    // True when a board is declared whose wave group is not yet known, so every
    // "missing" verdict is held back.
    Q_PROPERTY(bool anyGroupUnknown READ anyGroupUnknown NOTIFY profileChanged)
    Q_PROPERTY(QString advice READ advice NOTIFY profileChanged)

    // The Patch currently being worked on, analysed against the profile above.
    // {usesExpansion, playable, undecided, summary, requiredGroups,
    //  missingGroups, tones: [{toneNumber, enabled, status, label, groupId,
    //  numberRaw, slot}]}
    Q_PROPERTY(QVariantMap currentPatch READ currentPatch NOTIFY analysisChanged)
    // What could be learned right now: the groups the Patch on screen uses that
    // no declared board claims. Learning is only offered for these.
    Q_PROPERTY(QVariantList learnableGroups READ learnableGroups NOTIFY analysisChanged)

public:
    explicit ExpansionViewModel(QObject* parent = nullptr);

    // Both optional. Without a database the profile is session-only; without a
    // workspace there is no Patch to analyse, which is what the screenshot
    // harness gets.
    void setDatabase(library::LibraryDatabase* database);
    void setWorkspace(services::PatchWorkspace* workspace);

    [[nodiscard]] const library::ExpansionProfile& profile() const noexcept { return m_profile; }
    [[nodiscard]] QVariantList slotList() const;
    [[nodiscard]] int installedCount() const { return m_profile.installedCount(); }
    [[nodiscard]] bool anyGroupUnknown() const { return m_profile.anyGroupUnknown(); }
    [[nodiscard]] QString advice() const;
    [[nodiscard]] QVariantMap currentPatch() const;
    [[nodiscard]] QVariantList learnableGroups() const;

    // Declares what is in a slot. An empty name clears it. `waveGroupId` of -1
    // means "installed, group not known", which is a real state and not a
    // placeholder.
    Q_INVOKABLE bool setBoard(int slot, const QString& name, int waveGroupId = -1);
    Q_INVOKABLE bool clearSlot(int slot);

    // Records that the board in `slot` answers to `waveGroupId`. This is the
    // manual half of learning, for a musician who already knows.
    Q_INVOKABLE bool setWaveGroup(int slot, int waveGroupId);

    // Learns a group from the Patch on screen: the musician selected a wave
    // from the board in `slot` on the instrument, fetched the Patch, and this
    // reads which group that Tone actually carries. False when the Patch uses no
    // expansion wave, when its Tones disagree about which group (so there is
    // nothing unambiguous to learn), or when the slot holds no board.
    //
    // Deliberately refuses an ambiguous Patch rather than picking the first
    // group: a wrong association would make every later verdict wrong.
    Q_INVOKABLE bool learnFromCurrentPatch(int slot);
    // What that button should say, and why it is disabled when it is.
    [[nodiscard]] Q_INVOKABLE QString learnAdvice(int slot) const;

Q_SIGNALS:
    void profileChanged();
    void analysisChanged();

private:
    void load();
    void persist();

    library::LibraryDatabase* m_database = nullptr;
    services::PatchWorkspace* m_workspace = nullptr;
    library::ExpansionProfile m_profile;
};

} // namespace xp60studio::presentation
