#pragma once

#include <QObject>

// QML-visible semantic connection state. Registered as the uncreatable
// "ConnectionState" type in the XP60Studio.Presentation import.
namespace xp60studio::presentation {

Q_NAMESPACE

enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Error,
};
Q_ENUM_NS(ConnectionState)

} // namespace xp60studio::presentation
