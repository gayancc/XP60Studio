#include "presentation/AppShellViewModel.h"

#include "simulation/DemoBootstrap.h"

#include <QCoreApplication>
#include <QVariantMap>
#include <QtGlobal>

namespace xp60studio::presentation {

namespace {

// The rail resolves XpIcon by the stable key. Legacy text glyph metadata is
// retained for nonvisual consumers; it no longer determines the rendered icon.
const std::vector<AppShellViewModel::NavigationItem>& items()
{
    static const std::vector<AppShellViewModel::NavigationItem> kItems{
        {QStringLiteral("dashboard"), QStringLiteral("Dashboard"), QStringLiteral("▦"), true, QString()},
        {QStringLiteral("library"), QStringLiteral("Library"), QStringLiteral("▤"), true, QString()},
        {QStringLiteral("editor"), QStringLiteral("Editor"), QStringLiteral("✎"), true, QString()},
        {QStringLiteral("banks"), QStringLiteral("Banks"), QStringLiteral("▥"), true, QString()},
        {QStringLiteral("expansion"), QStringLiteral("Expansion"), QStringLiteral("▣"), true, QString()},
        {QStringLiteral("performance"), QStringLiteral("Performance"), QStringLiteral("♪"), false, QStringLiteral("Coming soon")},
        {QStringLiteral("compare"), QStringLiteral("Compare"), QStringLiteral("⇄"), false, QStringLiteral("Coming soon")},
        {QStringLiteral("devices"), QStringLiteral("Devices"), QStringLiteral("⌁"), true, QString()},
        {QStringLiteral("settings"), QStringLiteral("Settings"), QStringLiteral("⚙"), false, QStringLiteral("Coming soon")},
    };
    return kItems;
}

} // namespace

AppShellViewModel::AppShellViewModel(DevicesViewModel* devices, QObject* parent)
    : QObject(parent)
    , m_devices(devices)
{
    if (m_devices) {
        connect(m_devices, &DevicesViewModel::connectionChanged, this, &AppShellViewModel::connectionChanged);
        // midiActive is derived from traffic, which is reported by three
        // separate signals; without these the shell's activity indicator would
        // only ever update when the connection itself changed.
        connect(m_devices, &DevicesViewModel::operationsChanged, this, &AppShellViewModel::connectionChanged);
        connect(m_devices, &DevicesViewModel::transferChanged, this, &AppShellViewModel::connectionChanged);
        connect(m_devices, &DevicesViewModel::patchFetchChanged, this, &AppShellViewModel::connectionChanged);
    }
}

QString AppShellViewModel::appName() const
{
    return QStringLiteral("XP60Studio");
}

QString AppShellViewModel::appVersion() const
{
    const QString version = QCoreApplication::applicationVersion();
    return version.isEmpty() ? QStringLiteral("0.1.0") : version;
}

QString AppShellViewModel::buildInfo() const
{
    return QStringLiteral("Qt %1 · Phase 1 protocol foundation").arg(QString::fromLatin1(qVersion()));
}

const std::vector<AppShellViewModel::NavigationItem>& AppShellViewModel::navigationDefinition()
{
    return items();
}

QVariantList AppShellViewModel::navigationItems() const
{
    QVariantList list;
    for (const auto& item : items()) {
        QVariantMap map;
        map.insert(QStringLiteral("key"), item.key);
        map.insert(QStringLiteral("label"), item.label);
        map.insert(QStringLiteral("glyph"), item.glyph);
        map.insert(QStringLiteral("enabled"), item.enabled);
        map.insert(QStringLiteral("availability"), item.availability);
        list.append(map);
    }
    return list;
}

bool AppShellViewModel::isScreenAvailable(const QString& key) const
{
    for (const auto& item : items()) {
        if (item.key == key) {
            return item.enabled;
        }
    }
    return false;
}

void AppShellViewModel::setCurrentScreen(const QString& key)
{
    navigate(key);
}

bool AppShellViewModel::navigate(const QString& key)
{
    if (!isScreenAvailable(key)) {
        return false;
    }
    if (key == m_currentScreen) {
        return true;
    }
    m_currentScreen = key;
    emit currentScreenChanged();
    // connectionActionable depends on which screen is showing: offering
    // "Set up" while the user is already on Devices is what produced
    // "Open Devices to connect" on the Devices screen itself.
    emit connectionChanged();
    return true;
}

QString AppShellViewModel::currentScreenTitle() const
{
    for (const auto& item : items()) {
        if (item.key == m_currentScreen) {
            return item.label;
        }
    }
    return {};
}

ConnectionState AppShellViewModel::connectionState() const
{
    return m_devices ? m_devices->connectionState() : ConnectionState::Disconnected;
}

QString AppShellViewModel::connectionLabel() const
{
    // Musician-facing shell copy. Verified handshake is a secondary badge in
    // the indicator, not the primary connection string.
    switch (connectionState()) {
    case ConnectionState::Connected:
        if (m_demoMode) {
            return connectionVerified() ? QStringLiteral("XP-60 SIM") : QStringLiteral("Connected (Demo)");
        }
        return connectionVerified() ? QStringLiteral("XP-60 LIVE") : QStringLiteral("Connected");
    case ConnectionState::Connecting:
        return QStringLiteral("Connecting");
    case ConnectionState::Error:
        return QStringLiteral("Connection error");
    case ConnectionState::Disconnected:
        return m_demoMode ? QStringLiteral("Demo Offline") : QStringLiteral("Offline");
    }
    return {};
}

QString AppShellViewModel::connectionDetail() const
{
    if (m_demoMode) {
        return QStringLiteral("Simulated XP-60 — no hardware MIDI");
    }
    return m_devices ? m_devices->connectionDetail() : QString();
}

QString AppShellViewModel::connectionPhase() const
{
    // One vocabulary for the whole application. Callers switch on this rather
    // than on ConnectionState plus a verified flag plus a demo flag, each in
    // their own words.
    switch (connectionState()) {
    case ConnectionState::Connected:
        return connectionVerified() ? QStringLiteral("verified") : QStringLiteral("connected");
    case ConnectionState::Connecting:
        return QStringLiteral("connecting");
    case ConnectionState::Error:
        return QStringLiteral("problem");
    case ConnectionState::Disconnected:
        // "Available" is a materially different situation from "offline": the
        // ports are there and one click away, so the shell should not be
        // telling the user to go and find them.
        return (m_devices && m_devices->canConnect()) ? QStringLiteral("available")
                                                      : QStringLiteral("offline");
    }
    return QStringLiteral("offline");
}

QString AppShellViewModel::connectionTone() const
{
    const QString phase = connectionPhase();
    if (phase == QStringLiteral("verified")) {
        return QStringLiteral("live");
    }
    if (phase == QStringLiteral("connected") || phase == QStringLiteral("connecting")) {
        return QStringLiteral("warning");
    }
    if (phase == QStringLiteral("problem")) {
        return QStringLiteral("error");
    }
    return QStringLiteral("neutral");
}

QString AppShellViewModel::connectionShortLabel() const
{
    // Two words at most. The shell chip is for awareness; the Devices screen
    // carries the explanation.
    const QString phase = connectionPhase();
    if (phase == QStringLiteral("verified")) {
        return m_demoMode ? QStringLiteral("XP-60 SIM") : QStringLiteral("XP-60 LIVE");
    }
    if (phase == QStringLiteral("connected")) {
        return QStringLiteral("Connected");
    }
    if (phase == QStringLiteral("connecting")) {
        return QStringLiteral("Connecting");
    }
    if (phase == QStringLiteral("problem")) {
        return QStringLiteral("MIDI problem");
    }
    if (phase == QStringLiteral("available")) {
        return QStringLiteral("Ready to connect");
    }
    return QStringLiteral("Offline");
}

bool AppShellViewModel::connectionNeedsAttention() const
{
    return connectionPhase() == QStringLiteral("problem");
}

bool AppShellViewModel::connectionActionable() const
{
    if (m_currentScreen == QStringLiteral("devices")) {
        return false;
    }
    const QString phase = connectionPhase();
    return phase != QStringLiteral("verified") && phase != QStringLiteral("connecting");
}

QString AppShellViewModel::connectionActionLabel() const
{
    if (!connectionActionable()) {
        return {};
    }
    return connectionPhase() == QStringLiteral("problem") ? QStringLiteral("Diagnose")
                                                          : QStringLiteral("Set up");
}

bool AppShellViewModel::midiActive() const
{
    return m_devices
           && (m_devices->hasOutstandingRequests() || m_devices->transferBusy()
               || m_devices->patchFetchInProgress());
}

QString AppShellViewModel::deviceName() const
{
    return m_demoMode ? QStringLiteral("Roland XP-60 (Simulated)") : QStringLiteral("Roland XP-60");
}

void AppShellViewModel::setDemoMode(bool enabled)
{
    if (m_demoMode == enabled) {
        return;
    }
    m_demoMode = enabled;
    emit demoModeChanged();
    emit connectionChanged();
}

bool AppShellViewModel::requestDemoModeSwitch(bool enabled)
{
    if (m_demoModeSwitchPending) {
        return false;
    }
    if (m_demoMode == enabled) {
        return true;
    }
    m_demoModeSwitchPending = true;
    emit demoModeSwitchPendingChanged();
    if (!simulation::relaunchForDemoMode(enabled)) {
        m_demoModeSwitchPending = false;
        emit demoModeSwitchPendingChanged();
        return false;
    }
    return true;
}

bool AppShellViewModel::enterDemoMode()
{
    return requestDemoModeSwitch(true);
}

bool AppShellViewModel::exitDemoMode()
{
    return requestDemoModeSwitch(false);
}

} // namespace xp60studio::presentation
