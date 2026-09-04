#include "presentation/AppShellViewModel.h"

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
        {QStringLiteral("dashboard"), QStringLiteral("Dashboard"), QStringLiteral("▦"), false, QStringLiteral("Coming soon")},
        {QStringLiteral("library"), QStringLiteral("Library"), QStringLiteral("▤"), false, QStringLiteral("Coming soon")},
        {QStringLiteral("editor"), QStringLiteral("Editor"), QStringLiteral("✎"), true, QString()},
        {QStringLiteral("banks"), QStringLiteral("Banks"), QStringLiteral("▥"), false, QStringLiteral("Coming soon")},
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
        return connectionVerified() ? QStringLiteral("XP-60 LIVE") : QStringLiteral("Connected");
    case ConnectionState::Connecting:
        return QStringLiteral("Connecting");
    case ConnectionState::Error:
        return QStringLiteral("Connection error");
    case ConnectionState::Disconnected:
        return QStringLiteral("Offline");
    }
    return {};
}

QString AppShellViewModel::connectionDetail() const
{
    return m_devices ? m_devices->connectionDetail() : QString();
}

QString AppShellViewModel::deviceName() const
{
    return QStringLiteral("Roland XP-60");
}

} // namespace xp60studio::presentation
