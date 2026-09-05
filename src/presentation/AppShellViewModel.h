#pragma once

#include "presentation/ConnectionState.h"
#include "presentation/DevicesViewModel.h"

#include <QObject>
#include <QString>
#include <QVariantList>

namespace xp60studio::presentation {

// Global shell state: navigation and the authoritative connection indicator.
class AppShellViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString appName READ appName CONSTANT)
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
    Q_PROPERTY(QString buildInfo READ buildInfo CONSTANT)
    Q_PROPERTY(QVariantList navigationItems READ navigationItems CONSTANT)
    Q_PROPERTY(QString currentScreen READ currentScreen WRITE setCurrentScreen NOTIFY currentScreenChanged)
    Q_PROPERTY(QString currentScreenTitle READ currentScreenTitle NOTIFY currentScreenChanged)
    Q_PROPERTY(xp60studio::presentation::ConnectionState connectionState READ connectionState NOTIFY connectionChanged)
    Q_PROPERTY(QString connectionLabel READ connectionLabel NOTIFY connectionChanged)
    Q_PROPERTY(bool connectionVerified READ connectionVerified NOTIFY connectionChanged)
    Q_PROPERTY(QString connectionDetail READ connectionDetail NOTIFY connectionChanged)
    // The product-level connection vocabulary. Every surface reads these
    // instead of mapping ConnectionState to its own wording and its own
    // colour: doing that independently in six places is what made the shell
    // state "not connected" ten times on one screen, in ten different words.
    Q_PROPERTY(QString connectionPhase READ connectionPhase NOTIFY connectionChanged)
    Q_PROPERTY(QString connectionTone READ connectionTone NOTIFY connectionChanged)
    Q_PROPERTY(QString connectionShortLabel READ connectionShortLabel NOTIFY connectionChanged)
    Q_PROPERTY(bool connectionNeedsAttention READ connectionNeedsAttention NOTIFY connectionChanged)
    // True only when the shell can usefully offer a jump to Devices ? that is,
    // not while the user is already on Devices.
    Q_PROPERTY(bool connectionActionable READ connectionActionable NOTIFY connectionChanged)
    Q_PROPERTY(QString connectionActionLabel READ connectionActionLabel NOTIFY connectionChanged)
    // Transient traffic, for the shell's activity indicator.
    Q_PROPERTY(bool midiActive READ midiActive NOTIFY connectionChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY demoModeChanged)
    Q_PROPERTY(bool demoMode READ demoMode NOTIFY demoModeChanged)
    Q_PROPERTY(bool demoModeSwitchPending READ demoModeSwitchPending NOTIFY demoModeSwitchPendingChanged)

public:
    struct NavigationItem
    {
        QString key;
        QString label;
        QString glyph;
        bool enabled;
        QString availability; // e.g. "Phase 4"
    };

    explicit AppShellViewModel(DevicesViewModel* devices, QObject* parent = nullptr);

    [[nodiscard]] QString appName() const;
    [[nodiscard]] QString appVersion() const;
    [[nodiscard]] QString buildInfo() const;
    [[nodiscard]] QVariantList navigationItems() const;
    [[nodiscard]] static const std::vector<NavigationItem>& navigationDefinition();

    [[nodiscard]] QString currentScreen() const { return m_currentScreen; }
    void setCurrentScreen(const QString& key);
    [[nodiscard]] QString currentScreenTitle() const;
    Q_INVOKABLE bool navigate(const QString& key);
    Q_INVOKABLE bool isScreenAvailable(const QString& key) const;

    [[nodiscard]] ConnectionState connectionState() const;
    [[nodiscard]] QString connectionLabel() const;
    bool connectionVerified() const { return m_devices && m_devices->connectionVerified(); }
    [[nodiscard]] QString connectionDetail() const;
    [[nodiscard]] QString connectionPhase() const;
    [[nodiscard]] QString connectionTone() const;
    [[nodiscard]] QString connectionShortLabel() const;
    [[nodiscard]] bool connectionNeedsAttention() const;
    [[nodiscard]] bool connectionActionable() const;
    [[nodiscard]] QString connectionActionLabel() const;
    [[nodiscard]] bool midiActive() const;
    [[nodiscard]] QString deviceName() const;

    [[nodiscard]] bool demoMode() const { return m_demoMode; }
    void setDemoMode(bool enabled);

    [[nodiscard]] bool demoModeSwitchPending() const { return m_demoModeSwitchPending; }

    // Consumer path: persist preference and relaunch into the other mode.
    Q_INVOKABLE bool enterDemoMode();
    Q_INVOKABLE bool exitDemoMode();

signals:
    void currentScreenChanged();
    void connectionChanged();
    void demoModeChanged();
    void demoModeSwitchPendingChanged();

private:
    bool requestDemoModeSwitch(bool enabled);

    DevicesViewModel* m_devices;
    QString m_currentScreen = QStringLiteral("devices");
    bool m_demoMode = false;
    bool m_demoModeSwitchPending = false;
};

} // namespace xp60studio::presentation
