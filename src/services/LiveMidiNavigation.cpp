#include "services/LiveMidiNavigation.h"

#include <utility>

namespace xp60studio::services {

std::string_view liveActionName(LiveAction action) noexcept
{
    switch (action) {
    case LiveAction::Next:
        return "Next";
    case LiveAction::Previous:
        return "Previous";
    case LiveAction::AdvanceAndGo:
        return "AdvanceAndGo";
    case LiveAction::Restart:
        return "Restart";
    case LiveAction::GoToNumbered:
        return "GoToNumbered";
    }
    return "Unknown";
}

std::string_view liveTriggerKindName(LiveTriggerKind kind) noexcept
{
    switch (kind) {
    case LiveTriggerKind::ProgramChange:
        return "Program Change";
    case LiveTriggerKind::ControlChange:
        return "Control Change";
    case LiveTriggerKind::Note:
        return "Note";
    }
    return "Unknown";
}

LiveMidiNavigation::LiveMidiNavigation(DeviceSession& session, LiveSession& live, QObject* parent)
    : QObject(parent)
    , m_session(session)
    , m_live(live)
{
    connect(&m_session, &DeviceSession::channelMessageObserved, this,
            &LiveMidiNavigation::onChannelMessage);
}

void LiveMidiNavigation::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;
    emit changed();
}

QString LiveMidiNavigation::whyNotUsable(const LiveTriggerBinding& binding)
{
    if (binding.channel < 0 || binding.channel > 16) {
        return tr("MIDI channels are 1-16, or 0 for any channel.");
    }
    if (binding.number < 0 || binding.number > 127) {
        return tr("A controller, note or program number is 0-127.");
    }
    if (binding.minimumValue < 1 || binding.minimumValue > 127) {
        // Zero would fire on the release as well as the press, which for a
        // momentary switch means every stamp advances two cues.
        return tr("The threshold is 1-127: a foot switch must fire on the press "
                  "and not again on the release.");
    }
    if (binding.action == LiveAction::GoToNumbered
        && binding.kind == LiveTriggerKind::ControlChange) {
        // A control change's number is which knob moved, and its value is how
        // far. Neither is a cue, and picking one would be a guess.
        return tr("A control change carries a level, not a cue number. Bind Next, "
                  "Previous or Advance instead.");
    }
    return {};
}

bool LiveMidiNavigation::addBinding(const LiveTriggerBinding& binding)
{
    if (!whyNotUsable(binding).isEmpty()) {
        return false;
    }
    m_bindings.push_back(binding);
    emit changed();
    return true;
}

void LiveMidiNavigation::setBindings(std::vector<LiveTriggerBinding> bindings)
{
    m_bindings = std::move(bindings);
    emit changed();
}

void LiveMidiNavigation::clearBindings()
{
    if (m_bindings.empty()) {
        return;
    }
    m_bindings.clear();
    emit changed();
}

QStringList LiveMidiNavigation::describeBindings() const
{
    QStringList lines;
    for (const auto& binding : m_bindings) {
        const auto kind = QString::fromUtf8(liveTriggerKindName(binding.kind).data(),
                                            static_cast<qsizetype>(
                                                liveTriggerKindName(binding.kind).size()));
        const auto where = binding.channel == 0 ? tr("any channel")
                                                : tr("channel %1").arg(binding.channel);
        QString line;
        if (binding.action == LiveAction::GoToNumbered) {
            line = tr("%1 on %2 selects the cue of the same number").arg(kind, where);
        } else {
            const auto action = QString::fromUtf8(liveActionName(binding.action).data(),
                                                  static_cast<qsizetype>(
                                                      liveActionName(binding.action).size()));
            line = tr("%1 %2 on %3 → %4").arg(kind).arg(binding.number).arg(where, action);
        }
        if (binding.kind == LiveTriggerKind::ProgramChange) {
            // The XP-60 sends this itself whenever a Patch is chosen on its
            // panel, so the musician needs to know their own keyboard can move
            // the setlist.
            line += tr(" — note: the XP-60 sends Program Change when a Patch is "
                       "selected on its front panel, so its own buttons will "
                       "trigger this too.");
        }
        lines.append(line);
    }
    return lines;
}

void LiveMidiNavigation::ignore(QString reason)
{
    ++m_ignored;
    m_lastIgnored = std::move(reason);
    emit changed();
}

void LiveMidiNavigation::onChannelMessage(int status, int data1, int data2)
{
    if (!m_enabled || m_bindings.empty()) {
        return;
    }
    const int channel = (status & 0x0F) + 1;
    const int kindBits = status & 0xF0;

    LiveTriggerKind kind{};
    int number = 0;
    int value = 0;
    switch (kindBits) {
    case 0xC0:
        kind = LiveTriggerKind::ProgramChange;
        number = data1;
        // A Program Change has no value byte. It is always the press.
        value = 127;
        break;
    case 0xB0:
        kind = LiveTriggerKind::ControlChange;
        number = data1;
        value = data2 < 0 ? 0 : data2;
        break;
    case 0x90:
        kind = LiveTriggerKind::Note;
        number = data1;
        // Note On with velocity 0 is Note Off. Treated as the release, which
        // the threshold then rejects, rather than as a press of zero force.
        value = data2 < 0 ? 0 : data2;
        break;
    default:
        return;  // note off, aftertouch, pitch bend: nothing is bound to them
    }

    for (const auto& binding : m_bindings) {
        if (binding.kind != kind) {
            continue;
        }
        if (binding.channel != 0 && binding.channel != channel) {
            continue;
        }
        if (binding.action != LiveAction::GoToNumbered && binding.number != number) {
            continue;
        }
        if (value < binding.minimumValue) {
            continue;  // the release, or a knob below the threshold
        }
        if (m_live.switching()) {
            // The press meant "now". Honouring it once the current switch
            // finishes would put the wrong sound under the next section, so it
            // is dropped and said so.
            ignore(tr("A switch was already running."));
            return;
        }
        const bool ok = apply(binding, number);
        ++m_fired;
        emit changed();
        emit triggered(binding.action, ok);
        return;  // one message, one action
    }
}

bool LiveMidiNavigation::apply(const LiveTriggerBinding& binding, int number)
{
    switch (binding.action) {
    case LiveAction::AdvanceAndGo:
        return m_live.advance();
    case LiveAction::Next:
        return m_live.goNext() && (!binding.alsoSwitch || m_live.goToCurrent());
    case LiveAction::Previous:
        return m_live.goPrevious() && (!binding.alsoSwitch || m_live.goToCurrent());
    case LiveAction::Restart:
        m_live.rewind();
        return true;
    case LiveAction::GoToNumbered:
        // `number` is the wire byte, which is zero-based, and cue indices are
        // too — so a pedal displaying "1" sends 0 and selects the first cue,
        // which is what the musician means. No adjustment, deliberately: adding
        // one here would make pedal 1 select cue 2.
        return m_live.goToIndex(number) && (!binding.alsoSwitch || m_live.goToCurrent());
    }
    return false;
}

} // namespace xp60studio::services
