#include "simulation/DemoReplyPump.h"

#include "midi/MidiTypes.h"

namespace xp60studio::simulation {

DemoReplyPump::DemoReplyPump(SimulatedXp60& device, midi::LoopbackMidiTransport& transport, QObject* parent)
    : QObject(parent)
    , m_device(device)
    , m_transport(transport)
{
    m_timer.setTimerType(Qt::PreciseTimer);
    QObject::connect(&m_timer, &QTimer::timeout, this, &DemoReplyPump::pumpOnce);
}

void DemoReplyPump::start(int intervalMilliseconds)
{
    if (intervalMilliseconds < 1) {
        intervalMilliseconds = 1;
    }
    m_timer.start(intervalMilliseconds);
}

void DemoReplyPump::stop()
{
    m_timer.stop();
}

bool DemoReplyPump::isActive() const
{
    return m_timer.isActive();
}

void DemoReplyPump::pumpOnce()
{
    const auto replies = m_device.exchange(m_transport);
    for (const auto& reply : replies) {
        const auto bytes = reply.encode();
        m_transport.injectIncoming(midi::MidiByteSpan(bytes.data(), bytes.size()));
    }
}

} // namespace xp60studio::simulation
