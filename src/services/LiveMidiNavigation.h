#pragma once

#include "services/LiveSession.h"

#include <QObject>
#include <QString>

#include <vector>

namespace xp60studio::services {

// What kind of message a binding listens for.
enum class LiveTriggerKind {
    ProgramChange,
    ControlChange,
    Note,
};

// What it does when it hears one.
enum class LiveAction {
    Next,
    Previous,
    // Move to the cue and realise it — the foot switch action.
    AdvanceAndGo,
    Restart,
    // The message's own number selects the cue: Program Change 1 is cue 1.
    // Only meaningful for ProgramChange and Note bindings.
    GoToNumbered,
};

[[nodiscard]] std::string_view liveActionName(LiveAction action) noexcept;
[[nodiscard]] std::string_view liveTriggerKindName(LiveTriggerKind kind) noexcept;

struct LiveTriggerBinding
{
    LiveTriggerKind kind = LiveTriggerKind::ControlChange;
    // 1..16, or 0 for "any channel".
    int channel = 0;
    // Controller number, note number, or — for a ProgramChange binding that is
    // not GoToNumbered — the program that fires it. Ignored when the action is
    // GoToNumbered.
    int number = 0;
    // A control change or note fires when its value reaches this, so a
    // momentary foot switch acts on the press and not again on the release.
    // Roland and most pedals send 127 down and 0 up.
    int minimumValue = 64;
    LiveAction action = LiveAction::AdvanceAndGo;
    // Whether this binding also moves the sound, or only the cursor. Only
    // meaningful for the actions that are not AdvanceAndGo.
    bool alsoSwitch = false;

    friend bool operator==(const LiveTriggerBinding&, const LiveTriggerBinding&) = default;
};

// Driving a setlist from a foot switch or a controller.
//
// ── Off unless it is turned on, with nothing bound by default ───────────────
//
// This listens to whatever is plugged into the application's MIDI input, which
// is usually the XP-60's own MIDI OUT — and the XP-60 transmits Bank Select and
// Program Change whenever a Patch is selected on its front panel (Owner's
// Manual p.218-219). A default binding on Program Change would therefore make
// the setlist jump every time the musician touched a Patch button, which is
// exactly the kind of surprise a stage tool must not have. So `enabled()` is
// false until something turns it on, `bindings()` starts empty, and choosing a
// Program Change binding is the user knowingly aiming at that same message.
//
// ── It refuses to fire while a switch is running ────────────────────────────
//
// A pedal pressed twice in a bar must not queue two switches. A trigger that
// arrives while `LiveSession` is mid-switch is dropped and counted, rather than
// stacking behind the first — the second press meant "now", and honouring it
// four seconds later would put the wrong sound under the next verse.
//
// ── It reports what it ignored ──────────────────────────────────────────────
//
// `ignoredCount()` and `lastIgnoredReason()` exist because the failure mode of
// MIDI control is silence: a pedal on the wrong channel and a pedal that is not
// plugged in look identical from the stage. This says which.
class LiveMidiNavigation : public QObject
{
    Q_OBJECT

public:
    LiveMidiNavigation(DeviceSession& session, LiveSession& live, QObject* parent = nullptr);

    [[nodiscard]] bool enabled() const noexcept { return m_enabled; }
    void setEnabled(bool enabled);

    [[nodiscard]] const std::vector<LiveTriggerBinding>& bindings() const noexcept
    {
        return m_bindings;
    }
    void setBindings(std::vector<LiveTriggerBinding> bindings);
    // False when the binding cannot fire — a channel outside 1..16, a number
    // outside 0..127, a threshold outside 1..127, or GoToNumbered on a control
    // change, whose value is a level rather than a cue.
    bool addBinding(const LiveTriggerBinding& binding);
    void clearBindings();
    [[nodiscard]] static QString whyNotUsable(const LiveTriggerBinding& binding);

    // A plain-language description of what is bound, for a settings screen and
    // for the warning a Program Change binding deserves.
    [[nodiscard]] QStringList describeBindings() const;

    [[nodiscard]] int firedCount() const noexcept { return m_fired; }
    [[nodiscard]] int ignoredCount() const noexcept { return m_ignored; }
    [[nodiscard]] QString lastIgnoredReason() const { return m_lastIgnored; }

Q_SIGNALS:
    void changed();
    // A binding matched and its action was carried out. `ok` is what the action
    // returned: moving off the end of the list is a legitimate false.
    void triggered(LiveAction action, bool ok);

private:
    void onChannelMessage(int status, int data1, int data2);
    bool apply(const LiveTriggerBinding& binding, int number);
    void ignore(QString reason);

    DeviceSession& m_session;
    LiveSession& m_live;
    bool m_enabled = false;
    std::vector<LiveTriggerBinding> m_bindings;
    int m_fired = 0;
    int m_ignored = 0;
    QString m_lastIgnored;
};

} // namespace xp60studio::services
