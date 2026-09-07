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

#include "library/LibraryDatabase.h"
#include "library/SyxImport.h"
#include "midi/LoopbackMidiTransport.h"
#include "presentation/AppShellViewModel.h"
#include "presentation/BankBuilderViewModel.h"
#include "presentation/DevicesViewModel.h"
#include "presentation/DashboardViewModel.h"
#include "presentation/ExpansionViewModel.h"
#include "presentation/LibraryListModel.h"
#include "presentation/LibraryTransferViewModel.h"
#include "presentation/PerformanceViewModel.h"
#include "services/LibraryExportService.h"
#include "services/LibraryImportService.h"
#include "presentation/PatchEditorViewModel.h"
#include "presentation/QmlRegistration.h"
#include "services/DeviceSession.h"
#include "services/PatchTransfer.h"
#include "services/PatchWorkspace.h"

#include <QFile>
#include <QFont>
#include <QFontDatabase>
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
#ifdef Q_OS_WIN
    // The offscreen plugin does not enumerate Windows' system font directory
    // reliably. Register the native UI faces explicitly so captures exercise
    // the same typography as the real Windows window, while Silkscreen stays
    // local to BankPanelDisplay.
    QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/segoeui.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/segoeuib.ttf"));
    app.setFont(QFont(QStringLiteral("Segoe UI")));
#endif
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
    xp60studio::services::PatchWorkspace workspace;
    xp60studio::presentation::PatchEditorViewModel editor(session, workspace, &transfer);
    xp60studio::presentation::AppShellViewModel shell(&devices);

    xp60studio::library::LibraryDatabase libraryDatabase;
    if (!libraryDatabase.open(QString::fromLatin1(xp60studio::library::LibraryDatabase::kInMemoryPath))) {
        qWarning("Could not open the in-memory library; the Library screen renders empty");
    }
    // The golden fixture's 128 real User patches, so the Library and Bank
    // Builder captures show real names and real provenance rather than
    // invented rows.
    std::vector<std::int64_t> fixtureIds;
    {
        QFile fixture(QStringLiteral(XP60STUDIO_FIXTURE_DIR "/user-bank-amal.syx"));
        if (fixture.open(QIODevice::ReadOnly)) {
            const QByteArray bytes = fixture.readAll();
            const auto* begin = reinterpret_cast<const xp60studio::roland::Byte*>(bytes.constData());
            xp60studio::library::SyxImportOptions options;
            options.sourceName = "user-bank-amal.syx";
            const auto imported = xp60studio::library::importSyxStream(
                xp60studio::roland::ByteSpan(begin, static_cast<std::size_t>(bytes.size())), options);
            if (const auto ids = libraryDatabase.insertAll(imported.entries)) {
                fixtureIds = *ids;
            }
        }
    }

    xp60studio::presentation::LibraryListModel libraryModel;
    libraryModel.setDatabase(&libraryDatabase);

    xp60studio::testsupport::FakeXp60 fake(xp60studio::testsupport::temporaryAreaWith(4));
    // Offline capture: the shell's disconnected state is a first-class screen
    // and has to be reviewable too, so leave the endpoints closed.
    const bool offline = qEnvironmentVariableIsSet("XP60STUDIO_SHOT_OFFLINE");
    if (!offline) {
        session.connectEndpoints("in-1", "out-1");
        session.fetchTemporaryPatch();
    }
    for (int i = 0; offline ? false : i < 40; ++i) {
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

    // Main.qml requires every view model the shell binds. The harness has to
    // supply them all or the engine refuses to load the scene, which is how
    // this capture tool notices a new required property.
    xp60studio::presentation::DashboardViewModel dashboard;
    dashboard.setDatabase(&libraryDatabase);
    xp60studio::presentation::LibraryListModel bankSourceModel;
    bankSourceModel.setDatabase(&libraryDatabase);
    xp60studio::presentation::BankBuilderViewModel bankBuilder;
    bankBuilder.setDatabase(&libraryDatabase);
    bankBuilder.setTransfer(&transfer);

    // Keep the harness wired like the production composition root. Main.qml
    // deliberately makes these required properties, so adding a screen cannot
    // leave visual-regression captures silently exercising a partial shell.
    xp60studio::presentation::ExpansionViewModel expansion;
    expansion.setDatabase(&libraryDatabase);
    expansion.setWorkspace(&workspace);
    libraryModel.setExpansionProfile(&expansion.profile());
    bankSourceModel.setExpansionProfile(&expansion.profile());
    bankBuilder.setExpansionProfile(&expansion.profile());
    editor.setExpansionProfile(&expansion.profile());

    xp60studio::presentation::PerformanceViewModel performance(session);

    xp60studio::services::LibraryImportService libraryImport(libraryDatabase);
    xp60studio::services::LibraryExportService libraryExport(libraryDatabase);
    xp60studio::presentation::LibraryTransferViewModel libraryTransfer(libraryImport, libraryExport);

    // Construct the QML engine last. C++ destroys locals in reverse order, so
    // every QObject exposed below must outlive the engine and its bindings.
    // Otherwise shutdown produces a flood of misleading null-property errors
    // that can hide a real visual-regression warning.
    QQmlApplicationEngine engine;
    engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
    engine.setInitialProperties({
        {QStringLiteral("shell"), QVariant::fromValue(&shell)},
        {QStringLiteral("devices"), QVariant::fromValue(&devices)},
        {QStringLiteral("editor"), QVariant::fromValue(&editor)},
        {QStringLiteral("library"), QVariant::fromValue(&libraryModel)},
        {QStringLiteral("libraryTransfer"), QVariant::fromValue(&libraryTransfer)},
        {QStringLiteral("bankBuilder"), QVariant::fromValue(&bankBuilder)},
        {QStringLiteral("expansion"), QVariant::fromValue(&expansion)},
        {QStringLiteral("performance"), QVariant::fromValue(&performance)},
        {QStringLiteral("bankLibrary"), QVariant::fromValue(&bankSourceModel)},
        {QStringLiteral("dashboard"), QVariant::fromValue(&dashboard)},
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
    // Bank Builder capture states. Each one is the real view model being
    // driven, so a capture can never show a state the surface cannot reach.
    if (qEnvironmentVariableIsSet("XP60STUDIO_SHOT_BANK_FILL") && !fixtureIds.empty()) {
        // A believable half-built bank: a run through subgroup A and a few in
        // B, taken from the fixture in order.
        const auto plan = qEnvironmentVariable("XP60STUDIO_SHOT_BANK_FILL").split(QLatin1Char(','));
        int patch = 0;
        for (const auto& entry : plan) {
            const auto parts = entry.split(QLatin1Char('-'));
            if (parts.size() != 2) {
                continue;
            }
            for (int slot = parts[0].toInt(); slot <= parts[1].toInt(); ++slot) {
                if (patch >= static_cast<int>(fixtureIds.size())) {
                    break;
                }
                bankBuilder.placePatch(slot, fixtureIds[static_cast<std::size_t>(patch++)]);
            }
        }
        bankBuilder.acknowledge();
    }
    if (qEnvironmentVariableIsSet("XP60STUDIO_SHOT_BANK_SUBGROUP")) {
        bankBuilder.selectSubgroup(qEnvironmentVariableIntValue("XP60STUDIO_SHOT_BANK_SUBGROUP"));
    }
    if (qEnvironmentVariableIsSet("XP60STUDIO_SHOT_BANK_BANK")) {
        bankBuilder.selectBank(qEnvironmentVariableIntValue("XP60STUDIO_SHOT_BANK_BANK"));
    }
    if (qEnvironmentVariableIsSet("XP60STUDIO_SHOT_BANK_NUMBER")) {
        bankBuilder.selectNumber(qEnvironmentVariableIntValue("XP60STUDIO_SHOT_BANK_NUMBER"));
    }

    if (!shell.navigate(screenId)) {
        qWarning("Screen '%s' is not available", qPrintable(screenId));
        return 3;
    }

    // Canvas states are worth capturing: Overview is only half of what the
    // effects surface does, and a focused or isolated capture is the only way
    // to review the other half without a live session.
    if (qEnvironmentVariableIsSet("XP60STUDIO_SHOT_CANVAS_FOCUS")
        || qEnvironmentVariableIsSet("XP60STUDIO_SHOT_CANVAS_ROUTE")) {
        QTimer::singleShot(400, &app, [&] {
            auto* canvas = window->findChild<QQuickItem*>(QStringLiteral("effectsCanvas"));
            if (!canvas) {
                qWarning("Effects canvas not found");
                return;
            }
            if (qEnvironmentVariableIsSet("XP60STUDIO_SHOT_CANVAS_FOCUS"))
                canvas->setProperty("focusedNode", qEnvironmentVariable("XP60STUDIO_SHOT_CANVAS_FOCUS"));
            if (qEnvironmentVariableIsSet("XP60STUDIO_SHOT_CANVAS_ROUTE"))
                canvas->setProperty("isolatedRoute", qEnvironmentVariable("XP60STUDIO_SHOT_CANVAS_ROUTE"));
        });
    }

    if (qEnvironmentVariableIsSet("XP60STUDIO_SHOT_BANK_DRAG") && !fixtureIds.empty()) {
        QTimer::singleShot(500, &app, [&] {
            auto* screen = window->findChild<QQuickItem*>(QStringLiteral("bankBuilderScreen"));
            if (!screen) {
                qWarning("Bank Builder screen not found");
                return;
            }
            const int slot = qEnvironmentVariableIntValue("XP60STUDIO_SHOT_BANK_DRAG");
            const auto record = libraryDatabase.record(fixtureIds.back());
            // The screen stages the drag itself, so the capture goes through
            // the same state a real gesture produces.
            QMetaObject::invokeMethod(screen, "stageDrag",
                Q_ARG(QVariant, QVariant::fromValue<qint64>(fixtureIds.back())),
                Q_ARG(QVariant, record ? QString::fromStdString(record->name) : QStringLiteral("Patch")),
                Q_ARG(QVariant, slot),
                Q_ARG(QVariant, qEnvironmentVariableIsSet("XP60STUDIO_SHOT_BANK_DRAG_FROM")
                                    ? qEnvironmentVariableIntValue("XP60STUDIO_SHOT_BANK_DRAG_FROM")
                                    : -1));
        });
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
