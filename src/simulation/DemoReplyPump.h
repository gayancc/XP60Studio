#pragma once

// Qt event-loop pump that keeps SimulatedXp60 answering DeviceSession traffic.

#include "midi/LoopbackMidiTransport.h"
#include "simulation/SimulatedXp60.h"

#include <QObject>
#include <QTimer>

namespace xp60studio::simulation {

class DemoReplyPump final : public QObject
{
    Q_OBJECT

public:
    DemoReplyPump(SimulatedXp60& device, midi::LoopbackMidiTransport& transport, QObject* parent = nullptr);

    void start(int intervalMilliseconds = 8);
    void stop();

    // Single exchange + inject cycle (tests / bootstrap).
    void pumpOnce();

    [[nodiscard]] bool isActive() const;

private:
    SimulatedXp60& m_device;
    midi::LoopbackMidiTransport& m_transport;
    QTimer m_timer;
};

} // namespace xp60studio::simulation
