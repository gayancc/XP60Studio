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
// ── Why this screen asks rather than detecting ──────────────────────────────
//
// Nothing in the protocol reports which boards are fitted, so the musician says.
// What XP60Studio brings is a catalogue and a way to check:
//
//  * `knownBoards` lists Roland's SR-JV80 series, and choosing one fills in the
//    wave group its waves are inferred to carry — group ID is taken to be the
//    board number (`library::ExpansionBoardCatalog`, and
//    `docs/protocol/ROLAND_XP60_PROTOCOL_FACTS.md` §7 for the evidence and the
//    limits of that inference).
//  * `learnFromCurrentPatch` reads the group out of a Patch fetched from the
//    musician's own instrument. Evidence beats the catalogue: if their board
//    answers to a different number, Learn records that and this class believes
//    it.
//
// The inference names things and saves typing. It never concludes that an
// instrument has a board — that is only ever what the musician declared.
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
    // What the Wave Browser's Expansion tab can honestly say. See browserNote().
    Q_PROPERTY(QString browserNote READ browserNote NOTIFY profileChanged)
    // Roland's SR-JV80 catalogue, for the "which board is this?" picker:
    // {number, title, name, waveGroupId}. Choosing one fills in its wave group,
    // because group ID is inferred to be the board number — an inference the
    // musician can overrule and that Learn overrides. See
    // docs/protocol/ROLAND_XP60_PROTOCOL_FACTS.md §7.
    Q_PROPERTY(QVariantList knownBoards READ knownBoards CONSTANT)

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

    // The Wave Browser cannot list expansion waves: this project has no
    // waveform-name list for any SR-JV80 board — Roland publishes those per
    // board and none is transcribed here — and no way to turn a board into the
    // Wave Group ID a Tone would need. What it *can* say is which boards the
    // musician has declared, so the tab reports the real state of this
    // instrument instead of a flat "unverified".
    [[nodiscard]] QString browserNote() const;
    [[nodiscard]] QVariantList knownBoards() const;

    // Declares that `slot` holds SR-JV80-`boardNumber`, filling in both the name
    // and the wave group. Convenience over `setBoard`, not a different kind of
    // claim: the group it writes is the same one the musician could type, and
    // they remain free to change it afterwards.
    Q_INVOKABLE bool declareBoard(int slot, int boardNumber);

    // "wave group 14 (SR-JV80-14 Asia)" — the group number first, because that
    // is what the Tone carries; the board name is the inference resting on it.
    [[nodiscard]] Q_INVOKABLE QString describeGroup(int waveGroupId) const;

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
