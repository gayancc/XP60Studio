// Renders a screen of the real shell, headless, against a fake XP-60 holding a
// real Patch from the golden fixture, and writes a PNG.
//
// A documentation and mockup-review aid, not a test and not part of the
// application: it exists so the screens in docs/design/screenshots are
// captured from the shipping QML rather than redrawn by hand.
//
//   xp60studio_screenshot <out.png> [screen-id] [width] [height]
//
// Run with QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software.

#include "support/FakeXp60.h"

#include "midi/LoopbackMidiTransport.h"
#include "presentation/AppShellViewModel.h"
#include "presentation/DevicesViewModel.h"
#include "presentation/PatchEditorViewModel.h"
#include "presentation/QmlRegistration.h"
#include "services/DeviceSession.h"
#include "services/PatchTransfer.h"

#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>

#include <chrono>
#include <algorithm>
#include <memory>

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    if (argc < 2) {
        qWarning("usage: %s <out.png> [screen-id] [width] [height]", argv[0]);
        return 2;
    }
    const QString outPath = QString::fromLocal8Bit(argv[1]);
    const QString screenId = argc > 2 ? QString::fromLocal8Bit(argv[2]) : QStringLiteral("editor");
    const int width = argc > 3 ? QString::fromLocal8Bit(argv[3]).toInt() : 1440;
    const int height = argc > 4 ? QString::fromLocal8Bit(argv[4]).toInt() : 900;

    QQuickStyle::setStyle(QStringLiteral("Basic"));
    xp60studio::presentation::registerQmlTypes();

    auto loopback = std::make_unique<xp60studio::midi::LoopbackMidiTransport>();
    loopback->addInput("in-1", "XP-60 IN");
    loopback->addOutput("out-1", "XP-60 OUT");
    auto* transport = loopback.get();

    xp60studio::services::DeviceSession session(std::move(loopback));
    session.setAutomaticTimeoutPolling(false);
    auto pacing = session.pacing();
    pacing.interMessageDelay = std::chrono::milliseconds(0);
    session.setPacing(pacing);

    xp60studio::services::PatchTransfer transfer(session);
    xp60studio::presentation::DevicesViewModel devices(session, &transfer);
    xp60studio::presentation::PatchEditorViewModel editor(session, &transfer);
    xp60studio::presentation::AppShellViewModel shell(&devices);

    xp60studio::testsupport::FakeXp60 fake(xp60studio::testsupport::temporaryAreaWith(4));
    session.connectEndpoints("in-1", "out-1");
    session.fetchTemporaryPatch();
    for (int i = 0; i < 40; ++i) {
        QCoreApplication::processEvents();
        const auto replies = fake.exchange(*transport);
        for (const auto& reply : replies) {
            const auto bytes = reply.encode();
            transport->injectIncoming(xp60studio::midi::MidiByteSpan(bytes.data(), bytes.size()));
        }
        QCoreApplication::processEvents();
        if (replies.empty() && transport->sentMessages().empty() && session.pendingSendCount() == 0) {
            break;
        }
    }

    QQmlApplicationEngine engine;
    engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
    engine.setInitialProperties({
        {QStringLiteral("shell"), QVariant::fromValue(&shell)},
        {QStringLiteral("devices"), QVariant::fromValue(&devices)},
        {QStringLiteral("editor"), QVariant::fromValue(&editor)},
    });
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/XP60Studio/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (!window) {
        qWarning("Root object is not a QQuickWindow");
        return 2;
    }
    // Optional: capture a specific key/velocity range, for documenting states
    // the fixture Patch does not happen to contain.
    if (qEnvironmentVariableIsSet("XP60STUDIO_SHOT_KEYS")) {
        const auto parts = qEnvironmentVariable("XP60STUDIO_SHOT_KEYS").split(QLatin1Char(','));
        if (parts.size() == 2) {
            editor.setKeyRangeUpper(parts[1].toInt());
            editor.setKeyRangeLower(parts[0].toInt());
        }
    }
    if (qEnvironmentVariableIsSet("XP60STUDIO_SHOT_VELOCITY")) {
        const auto parts = qEnvironmentVariable("XP60STUDIO_SHOT_VELOCITY").split(QLatin1Char(','));
        if (parts.size() == 2) {
            editor.setVelocityLower(parts[0].toInt());
            editor.setVelocityUpper(parts[1].toInt());
        }
    }

    window->resize(width, height);
    // Local routing variants for visual checks; never sent to the fake device.
    if (qEnvironmentVariableIsSet("XP60STUDIO_SHOT_OUTPUT")) {
        using xp60studio::xpmodel::CommonParameter;
        using xp60studio::xpmodel::ToneParameter;
        using xp60studio::xpmodel::ToneIndex;
        editor.setCommonRaw(CommonParameter::StructureType12, 0);
        editor.setToneRaw(ToneIndex::tone1(), ToneParameter::OutputAssign,
                          qEnvironmentVariableIntValue("XP60STUDIO_SHOT_OUTPUT"));
    }
    if (qEnvironmentVariableIsSet("XP60STUDIO_SHOT_LIVE")) {
        editor.armWrite();
        editor.startLiveAudition();
        for (int i = 0; i < 40; ++i) {
            QCoreApplication::processEvents();
            const auto replies = fake.exchange(*transport);
            for (const auto& reply : replies) {
                const auto bytes = reply.encode();
                transport->injectIncoming(xp60studio::midi::MidiByteSpan(bytes.data(), bytes.size()));
            }
            QCoreApplication::processEvents();
            if (!transfer.isBusy()) break;
        }
    }
    if (qEnvironmentVariableIsSet("XP60STUDIO_SHOT_SECTION")) {
        // Section selection is independent of catalog browsing.
        editor.setSection(qEnvironmentVariableIntValue("XP60STUDIO_SHOT_SECTION"));
    }
    if (qEnvironmentVariableIsSet("XP60STUDIO_SHOT_EFFECT_PAGE"))
        editor.setEffectPage(qEnvironmentVariableIntValue("XP60STUDIO_SHOT_EFFECT_PAGE"));
    if (qEnvironmentVariableIsSet("XP60STUDIO_SHOT_EFX"))
        editor.editEffect(QStringLiteral("common.efx_type"), qEnvironmentVariableIntValue("XP60STUDIO_SHOT_EFX"));
    if (qEnvironmentVariableIsSet("XP60STUDIO_SHOT_REVERB"))
        editor.editEffect(QStringLiteral("common.reverb_type"), qEnvironmentVariableIntValue("XP60STUDIO_SHOT_REVERB"));
    if (qEnvironmentVariableIsSet("XP60STUDIO_SHOT_WAVES")) {
        auto* button = window->findChild<QObject*>(QStringLiteral("browseWavesButton"));
        if (button) QMetaObject::invokeMethod(button, "clicked");
        editor.waves()->setQuery(qEnvironmentVariable("XP60STUDIO_SHOT_WAVES"));
        editor.waves()->selectRow(0);
    }
    if (qEnvironmentVariableIsSet("XP60STUDIO_SHOT_DISCLOSURE")) {
        editor.setDisclosure(qEnvironmentVariableIntValue("XP60STUDIO_SHOT_DISCLOSURE"));
    }
    if (!shell.navigate(screenId)) {
        qWarning("Screen '%s' is not available", qPrintable(screenId));
        return 3;
    }

    int exitCode = 0;
    // Capture lower panels at the real supported viewport size, rather than
    // pretending a very tall window proves that scrolling controls fit.
    if (qEnvironmentVariableIsSet("XP60STUDIO_SHOT_FOCUS")) {
        QTimer::singleShot(500, &app, [&] {
            auto* scroller = window->findChild<QObject*>(QStringLiteral("editorScroll"));
            auto* target = window->findChild<QQuickItem*>(qEnvironmentVariable("XP60STUDIO_SHOT_FOCUS"));
            auto* flickable = scroller ? scroller->property("contentItem").value<QQuickItem*>() : nullptr;
            if (target && flickable) {
                const auto y = target->mapToItem(flickable, QPointF(0, 0)).y();
                const auto maximum = std::max(qreal(0), flickable->property("contentHeight").toReal() - flickable->height());
                flickable->setProperty("contentY", std::clamp(flickable->property("contentY").toReal() + y, qreal(0), maximum));
            } else {
                qWarning("Could not locate the requested capture panel");
                exitCode = 5;
            }
        });
    }
    QTimer::singleShot(1500, &app, [&] {
        const QImage image = window->grabWindow();
        if (image.isNull() || !image.save(outPath)) {
            qWarning("Could not save screenshot to %s", qPrintable(outPath));
            exitCode = 4;
        } else {
            qInfo("Screenshot written to %s (%dx%d)", qPrintable(outPath), image.width(), image.height());
        }
        QCoreApplication::exit(exitCode);
    });
    return QGuiApplication::exec();
}
