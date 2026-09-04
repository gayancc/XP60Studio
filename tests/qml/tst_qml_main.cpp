#include "midi/LoopbackMidiTransport.h"
#include "presentation/AppShellViewModel.h"
#include "presentation/DevicesViewModel.h"
#include "presentation/QmlRegistration.h"
#include "services/DeviceSession.h"

#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QtQuickTest>

#include <chrono>
#include <memory>

// Provides real presentation objects (backed by the loopback transport) to the
// QML tests as `testDevices`, `testShell` and `testTransport`.
class Setup : public QObject
{
    Q_OBJECT

public slots:
    void applicationAvailable()
    {
        QQuickStyle::setStyle(QStringLiteral("Basic"));
        xp60studio::presentation::registerQmlTypes();
    }

    void qmlEngineAvailable(QQmlEngine* engine)
    {
        engine->addImportPath(QStringLiteral("qrc:/qt/qml"));

        auto loopback = std::make_unique<xp60studio::midi::LoopbackMidiTransport>();
        loopback->addInput("in-1", "XP-60 IN");
        loopback->addOutput("out-1", "XP-60 OUT");
        m_transport = loopback.get();
        m_session = std::make_unique<xp60studio::services::DeviceSession>(std::move(loopback));
        m_session->setAutomaticTimeoutPolling(false);
        auto pacing = m_session->pacing();
        pacing.interMessageDelay = std::chrono::milliseconds(0);
        m_session->setPacing(pacing);
        m_devices = std::make_unique<xp60studio::presentation::DevicesViewModel>(*m_session);
        m_shell = std::make_unique<xp60studio::presentation::AppShellViewModel>(m_devices.get());

        engine->rootContext()->setContextProperty(QStringLiteral("testDevices"), m_devices.get());
        engine->rootContext()->setContextProperty(QStringLiteral("testShell"), m_shell.get());
    }

private:
    xp60studio::midi::LoopbackMidiTransport* m_transport = nullptr;
    std::unique_ptr<xp60studio::services::DeviceSession> m_session;
    std::unique_ptr<xp60studio::presentation::DevicesViewModel> m_devices;
    std::unique_ptr<xp60studio::presentation::AppShellViewModel> m_shell;
};

QUICK_TEST_MAIN_WITH_SETUP(xp60studio_qml, Setup)

#include "tst_qml_main.moc"
