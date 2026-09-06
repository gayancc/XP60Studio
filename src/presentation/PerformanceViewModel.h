#pragma once

#include "services/DeviceSession.h"
#include "xpmodel/Xp60Performance.h"

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <deque>
#include <optional>

namespace xp60studio::presentation {

// The Performance editor and its 16-Part mixer.
//
// ── What this owns, and what it deliberately does not ────────────────────────
//
// It owns the working Performance and its undo history, the way
// services::PatchWorkspace owns the working Patch. It is a separate object
// rather than a second mode of that one because a Performance and a Patch are
// edited at the same time — a Performance names sixteen Patches, and the
// musician moves between them — so conflating the two working copies would make
// opening a Part's Patch destroy the Performance being edited.
//
// ── Sending ──────────────────────────────────────────────────────────────────
//
// `sendToTemporary()` writes the **temporary** Performance (`01 00 00 00`),
// which is what the instrument is playing now and is erased by power-off or by
// selecting another Performance. That is audition, and it is the only write
// offered here.
//
// Writing a USER Performance (`10 nn 00 00`) is a persistent write and is not
// implemented yet: it needs the read-before-write, verify-by-read-back and
// restore-what-was-there machinery `services::UserMemoryWrite` already provides
// for Patches. Until that exists this class must not pretend otherwise — see
// PATCH_SYNCHRONIZATION.md §7 for why the two are kept apart by address.
class PerformanceViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool hasPerformance READ hasPerformance NOTIFY changed)
    Q_PROPERTY(QString name READ name NOTIFY changed)
    Q_PROPERTY(QString summary READ summary NOTIFY changed)
    Q_PROPERTY(QString sourceText READ sourceText NOTIFY changed)
    Q_PROPERTY(int tempo READ tempo NOTIFY changed)
    Q_PROPERTY(QString keyboardMode READ keyboardMode NOTIFY changed)
    Q_PROPERTY(int activePartCount READ activePartCount NOTIFY changed)
    Q_PROPERTY(int partCount READ partCount CONSTANT)

    // Sixteen entries in Part order. Each carries everything one mixer strip
    // draws: {partNumber, isRhythmPart, receives, midiChannel, level, pan,
    // panText, outputAssign, chorusSend, reverbSend, voiceReserve,
    // patchGroupType, patchGroupLabel, patchGroupId, patchNumber,
    // keyLowerRaw, keyUpperRaw, keyLowerNote, keyUpperNote, octaveShift,
    // coarseTune, fineTune}.
    Q_PROPERTY(QVariantList parts READ parts NOTIFY changed)

    Q_PROPERTY(bool modified READ modified NOTIFY changed)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY changed)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY changed)
    Q_PROPERTY(QString undoLabel READ undoLabel NOTIFY changed)

    Q_PROPERTY(bool canFetch READ canFetch NOTIFY transferChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY transferChanged)
    Q_PROPERTY(QString transferMessage READ transferMessage NOTIFY transferChanged)
    Q_PROPERTY(QString transferTone READ transferTone NOTIFY transferChanged)
    Q_PROPERTY(bool canSend READ canSend NOTIFY transferChanged)

public:
    explicit PerformanceViewModel(services::DeviceSession& session, QObject* parent = nullptr);

    [[nodiscard]] bool hasPerformance() const noexcept { return m_working.has_value(); }
    [[nodiscard]] QString name() const;
    [[nodiscard]] QString summary() const;
    [[nodiscard]] QString sourceText() const { return m_sourceText; }
    [[nodiscard]] int tempo() const;
    [[nodiscard]] QString keyboardMode() const;
    [[nodiscard]] int activePartCount() const;
    [[nodiscard]] static int partCount() noexcept { return xpmodel::PartIndex::kCount; }
    [[nodiscard]] QVariantList parts() const;

    [[nodiscard]] bool modified() const;
    [[nodiscard]] bool canUndo() const noexcept { return !m_undo.empty(); }
    [[nodiscard]] bool canRedo() const noexcept { return !m_redo.empty(); }
    [[nodiscard]] QString undoLabel() const;

    [[nodiscard]] bool canFetch() const;
    [[nodiscard]] bool busy() const;
    [[nodiscard]] QString transferMessage() const { return m_transferMessage; }
    [[nodiscard]] QString transferTone() const { return m_transferTone; }
    [[nodiscard]] bool canSend() const;

    // Reads the temporary Performance, or USER:01..32 when `userNumber` is in
    // range. False when not connected or a fetch is already running.
    Q_INVOKABLE bool fetchTemporary();
    Q_INVOKABLE bool fetchUser(int userNumber);

    // Adopts a Performance directly. Used by tests and Demo Mode; the device
    // path goes through the fetch above.
    void adopt(const xpmodel::Xp60Performance& performance, const QString& sourceText);
    [[nodiscard]] const xpmodel::Xp60Performance& working() const { return *m_working; }

    // ── Mixer edits ─────────────────────────────────────────────────────────
    //
    // Each is one undo step and each refuses a value the Parameter Address Map
    // does not allow, rather than clamping it. `partNumber` is 1..16.
    Q_INVOKABLE bool setPartLevel(int partNumber, int level);
    Q_INVOKABLE bool setPartPan(int partNumber, int pan);
    Q_INVOKABLE bool setPartReceives(int partNumber, bool receives);
    Q_INVOKABLE bool setPartMidiChannel(int partNumber, int channel);      // 1..16
    Q_INVOKABLE bool setPartChorusSend(int partNumber, int level);
    Q_INVOKABLE bool setPartReverbSend(int partNumber, int level);
    Q_INVOKABLE bool setPartOutputAssign(int partNumber, int raw);
    Q_INVOKABLE bool setPartVoiceReserve(int partNumber, int voices);      // 0..64
    Q_INVOKABLE bool setPartOctaveShift(int partNumber, int octaves);      // -3..+3
    // Refused rather than clamped when it would cross the other bound: Roland
    // prints the pair as bounding each other, the same invariant the Patch
    // Tone's key range carries.
    Q_INVOKABLE bool setPartKeyRange(int partNumber, int lowerRaw, int upperRaw);
    Q_INVOKABLE bool setName(const QString& text);

    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    // Back to the Performance as fetched, in one step.
    Q_INVOKABLE void revert();

    // Sends the whole working Performance to the temporary area — audition, not
    // a persistent write. See the class comment.
    Q_INVOKABLE bool sendToTemporary();

Q_SIGNALS:
    void changed();
    void transferChanged();

private:
    // Applies `mutate` as one undoable step. An edit that changes nothing
    // records no history.
    bool commit(const QString& label, const std::function<bool(xpmodel::Xp60Performance&)>& mutate);
    void onFetchChanged();
    void setTransfer(const QString& message, const QString& tone);

    struct Step
    {
        xpmodel::Xp60Performance performance;
        QString label;
    };

    services::DeviceSession& m_session;
    std::optional<xpmodel::Xp60Performance> m_working;
    // What was fetched or adopted: the A side, and what revert() restores.
    std::optional<xpmodel::Xp60Performance> m_baseline;
    QString m_sourceText;
    std::deque<Step> m_undo;
    std::deque<Step> m_redo;
    QString m_transferMessage;
    QString m_transferTone{QStringLiteral("neutral")};
    bool m_sending = false;
};

} // namespace xp60studio::presentation
