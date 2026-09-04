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
    Q_PROPERTY(QString connectionDetail READ connectionDetail NOTIFY connectionChanged)
    Q_PROPERTY(QString deviceName READ deviceName CONSTANT)

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
    [[nodiscard]] QString connectionDetail() const;
    [[nodiscard]] QString deviceName() const;

signals:
    void currentScreenChanged();
    void connectionChanged();

private:
    DevicesViewModel* m_devices;
    QString m_currentScreen = QStringLiteral("devices");
};

} // namespace xp60studio::presentation
