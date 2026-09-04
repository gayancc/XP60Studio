// XP60Studio application entry point.
//
// Wires the layers together: libremidi transport -> DeviceSession -> view
// models -> QML shell. No protocol or MIDI logic lives in QML.

#include "midi/IMidiTransport.h"
#include "midi/LibremidiTransport.h"
#include "midi/LoopbackMidiTransport.h"
#include "presentation/AppShellViewModel.h"
#include "presentation/DevicesViewModel.h"
#include "presentation/QmlRegistration.h"
#include "services/DeviceSession.h"

#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QUrl>

#include <exception>
#include <memory>

#ifndef XP60STUDIO_VERSION
#define XP60STUDIO_VERSION "0.1.0"
#endif

namespace {

std::unique_ptr<xp60studio::midi::IMidiTransport> createTransport()
{
    try {
        return std::make_unique<xp60studio::midi::LibremidiTransport>();
    } catch (const std::exception& e) {
        qWarning("libremidi backend unavailable (%s); using loopback transport", e.what());
        return std::make_unique<xp60studio::midi::LoopbackMidiTransport>();
    }
}

} // namespace

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("XP60Studio"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("xp60studio.local"));
    QCoreApplication::setApplicationName(QStringLiteral("XP60Studio"));
    QCoreApplication::setApplicationVersion(QStringLiteral(XP60STUDIO_VERSION));

    // Qt Quick Controls are a foundation only; the XP60Studio component
    // library provides the visuals. Basic keeps the platform style out.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    xp60studio::presentation::registerQmlTypes();

    xp60studio::services::DeviceSession session(createTransport());
    xp60studio::presentation::DevicesViewModel devices(session);
    xp60studio::presentation::AppShellViewModel shell(&devices);

    QQmlApplicationEngine engine;
    engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
    engine.setInitialProperties({
        {QStringLiteral("shell"), QVariant::fromValue(&shell)},
        {QStringLiteral("devices"), QVariant::fromValue(&devices)},
    });
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
        Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/XP60Studio/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    // Developer aid: XP60STUDIO_SCREENSHOT=<file.png> renders the shell once,
    // writes the image and exits. Used for documentation and mockup review in
    // headless environments (QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software).
    const QString screenshotPath = qEnvironmentVariable("XP60STUDIO_SCREENSHOT");
    if (!screenshotPath.isEmpty()) {
        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        QTimer::singleShot(1500, &app, [window, screenshotPath] {
            int exitCode = 0;
            if (!window) {
                qWarning("Root object is not a QQuickWindow; no screenshot taken");
                exitCode = 2;
            } else {
                const QImage image = window->grabWindow();
                if (image.isNull() || !image.save(screenshotPath)) {
                    qWarning("Could not save screenshot to %s", qPrintable(screenshotPath));
                    exitCode = 3;
                } else {
                    qInfo("Screenshot written to %s", qPrintable(screenshotPath));
                }
            }
            QCoreApplication::exit(exitCode);
        });
    }
    return QGuiApplication::exec();
}
