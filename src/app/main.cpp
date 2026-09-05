// XP60Studio application entry point.
//
// Wires the layers together: transport -> DeviceSession -> view models -> QML.
// Demo Mode (in-app preference / --demo / XP60STUDIO_DEMO) uses Loopback +
// SimulatedXp60 instead of libremidi so every shipped workflow is explorable
// without hardware MIDI.

#include "midi/IMidiTransport.h"
#include "midi/LibremidiTransport.h"
#include "midi/LoopbackMidiTransport.h"
#include "library/LibraryDatabase.h"
#include "presentation/AppShellViewModel.h"
#include "presentation/BankBuilderViewModel.h"
#include "presentation/DashboardViewModel.h"
#include "presentation/LibraryListModel.h"
#include "presentation/LibraryTransferViewModel.h"
#include "services/LibraryExportService.h"
#include "services/LibraryImportService.h"
#include "presentation/DevicesViewModel.h"
#include "presentation/PatchEditorViewModel.h"
#include "services/PatchWorkspace.h"
#include "presentation/QmlRegistration.h"
#include "services/DeviceSession.h"
#include "services/PatchTransfer.h"
#include "simulation/DemoBootstrap.h"
#include "simulation/DemoFixture.h"
#include "simulation/DemoReplyPump.h"
#include "simulation/SimulatedXp60.h"

#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

#include <chrono>
#include <exception>
#include <memory>
#include <optional>

#ifndef XP60STUDIO_VERSION
#define XP60STUDIO_VERSION "0.1.0"
#endif

namespace {

std::unique_ptr<xp60studio::midi::IMidiTransport> createLiveTransport()
{
    try {
        return std::make_unique<xp60studio::midi::LibremidiTransport>();
    } catch (const std::exception& e) {
        qWarning("libremidi backend unavailable (%s); using loopback transport", e.what());
        return std::make_unique<xp60studio::midi::LoopbackMidiTransport>();
    }
}

// Where the local library lives when the application is not in Demo Mode.
// One file in the platform's application-data directory, alongside the
// connection settings QSettings already keeps there.
QString defaultLibraryPath()
{
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (directory.isEmpty()) {
        // No writable data location: keep the library in memory rather than
        // scattering a database file into the working directory.
        return QString::fromLatin1(xp60studio::library::LibraryDatabase::kInMemoryPath);
    }
    QDir().mkpath(directory);
    return QDir(directory).filePath(QStringLiteral("library.xp60lib"));
}

} // namespace

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/xp60studio.png")));
    QCoreApplication::setOrganizationName(QStringLiteral("XP60Studio"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("xp60studio.local"));
    QCoreApplication::setApplicationName(QStringLiteral("XP60Studio"));
    QCoreApplication::setApplicationVersion(QStringLiteral(XP60STUDIO_VERSION));

    // Qt Quick Controls are a foundation only; the XP60Studio component
    // library provides the visuals. Basic keeps the platform style out.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    xp60studio::presentation::registerQmlTypes();

    const bool demoMode = xp60studio::simulation::isDemoModeRequested(argc, argv);

    std::unique_ptr<xp60studio::midi::IMidiTransport> transportOwner;
    xp60studio::midi::LoopbackMidiTransport* demoLoopback = nullptr;
    std::optional<xp60studio::simulation::SimulatedXp60> simulatedDevice;

    if (demoMode) {
        auto bundle = xp60studio::simulation::createDemoTransport();
        demoLoopback = bundle.raw;
        transportOwner = std::move(bundle.transport);
        auto seed = xp60studio::simulation::temporaryAreaFromFixture(4);
        if (seed.isEmpty()) {
            qWarning("Demo Mode: embedded fixture failed to load; continuing without a seeded Patch");
        } else {
            simulatedDevice.emplace(std::move(seed));
        }
        qInfo("XP60Studio Demo Mode: simulated XP-60 — no OS MIDI ports");
    } else {
        transportOwner = createLiveTransport();
    }

    xp60studio::services::DeviceSession session(std::move(transportOwner));
    xp60studio::services::PatchTransfer transfer(session);
    QSettings connectionSettings;
    xp60studio::presentation::DevicesViewModel devices(session, &transfer);
    if (!demoMode) {
        devices.useConnectionSettings(&connectionSettings);
    }
    // One working Patch, shared by every screen that can show it. Created before
    // the view models because they hold a reference to it, and outliving them
    // because it is the thing they project.
    xp60studio::services::PatchWorkspace workspace;
    xp60studio::presentation::PatchEditorViewModel editor(session, workspace, &transfer);

    // The XP-60 transmits Bank Select and Program Change when a Patch is chosen
    // on its front panel, and choosing a Patch replaces the temporary area
    // (Owner's Manual p.45, p.218-219). That is the most likely way for the
    // application and the instrument to fall out of step, and the only way to
    // notice it without polling.
    QObject::connect(&session, &xp60studio::services::DeviceSession::patchSelectionObserved, &workspace,
                     [&workspace](int channel, int program) {
                         workspace.markStale(program > 0
                             ? QObject::tr("The XP-60 selected Patch %1 on channel %2, so its temporary Patch is no "
                                           "longer the one shown here.").arg(program).arg(channel)
                             : QObject::tr("The XP-60 selected another bank on channel %1, so its temporary Patch is "
                                           "no longer the one shown here.").arg(channel));
                     });
    xp60studio::presentation::AppShellViewModel shell(&devices);
    shell.setDemoMode(demoMode);

    // The local library. Demo Mode keeps it in memory so a demo can never add
    // to, or delete from, the user's real collection.
    xp60studio::library::LibraryDatabase libraryDatabase;
    const QString libraryPath = demoMode
        ? QString::fromLatin1(xp60studio::library::LibraryDatabase::kInMemoryPath)
        : defaultLibraryPath();
    if (!libraryDatabase.open(libraryPath)) {
        // The rest of the application still works; the Library screen shows an
        // empty library rather than the app refusing to start.
        qWarning("Could not open the library at %s: %s", qUtf8Printable(libraryPath),
                 qUtf8Printable(libraryDatabase.lastError()));
    }
    xp60studio::presentation::LibraryListModel libraryModel;
    libraryModel.setDatabase(&libraryDatabase);
    libraryModel.setWorkspace(&workspace);

    // Import and export. The services own the file I/O and the worker thread;
    // the view model is what QML sees.
    xp60studio::presentation::DashboardViewModel dashboard;
    dashboard.setDatabase(&libraryDatabase);

    // The Bank Builder. It gets its own list model on purpose: the source
    // library it browses is filtered and searched independently of the Library
    // screen, and a filter set while building a bank must not silently change
    // what the Library screen is showing.
    xp60studio::presentation::LibraryListModel bankSourceModel;
    bankSourceModel.setDatabase(&libraryDatabase);
    bankSourceModel.setWorkspace(&workspace);
    xp60studio::presentation::BankBuilderViewModel bankBuilder;
    bankBuilder.setDatabase(&libraryDatabase);
    bankBuilder.setTransfer(&transfer);
    // The Library and the Bank Builder show the working Patch's live name and
    // edited state wherever they show that Patch, so a rename in the Editor is
    // visible on a bank destination and in a library row immediately.
    bankBuilder.setWorkspace(&workspace);

    xp60studio::services::LibraryImportService libraryImport(libraryDatabase);
    xp60studio::services::LibraryExportService libraryExport(libraryDatabase);
    xp60studio::presentation::LibraryTransferViewModel libraryTransfer(libraryImport, libraryExport);
    // An import writes rows behind the list model's back, so it has to re-read
    // rather than keep showing counts from before the import.
    QObject::connect(&libraryTransfer, &xp60studio::presentation::LibraryTransferViewModel::libraryChanged,
                     &libraryModel, &xp60studio::presentation::LibraryListModel::refresh);
    QObject::connect(&libraryTransfer, &xp60studio::presentation::LibraryTransferViewModel::libraryChanged,
                     &bankSourceModel, &xp60studio::presentation::LibraryListModel::refresh);
    // An import adds source banks the Bank Builder's source picker has to know
    // about, so its own summaries are re-read too.
    QObject::connect(&libraryTransfer, &xp60studio::presentation::LibraryTransferViewModel::libraryChanged,
                     &bankBuilder, &xp60studio::presentation::BankBuilderViewModel::refresh);
    QObject::connect(&libraryTransfer, &xp60studio::presentation::LibraryTransferViewModel::libraryChanged,
                     &dashboard, &xp60studio::presentation::DashboardViewModel::refresh);

    // Declared after DeviceSession so the pump is destroyed before the
    // session-owned LoopbackMidiTransport it references.
    std::unique_ptr<xp60studio::simulation::DemoReplyPump> replyPump;

    if (demoMode && demoLoopback && simulatedDevice.has_value()) {
        // Demo transfers should feel snappy; real pacing still applies in live mode.
        auto pacing = session.pacing();
        pacing.interMessageDelay = std::chrono::milliseconds(0);
        session.setPacing(pacing);

        replyPump = std::make_unique<xp60studio::simulation::DemoReplyPump>(*simulatedDevice, *demoLoopback);
        replyPump->start(8);

        devices.refreshEndpoints();
        if (!session.connectEndpoints(xp60studio::simulation::kDemoInputId,
                                      xp60studio::simulation::kDemoOutputId)) {
            qWarning("Demo Mode: failed to open simulated MIDI endpoints");
        } else if (!session.fetchTemporaryPatch()) {
            qWarning("Demo Mode: failed to start temporary Patch seed fetch");
        }
    }

    QQmlApplicationEngine engine;
    engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
    engine.setInitialProperties({
        {QStringLiteral("shell"), QVariant::fromValue(&shell)},
        {QStringLiteral("devices"), QVariant::fromValue(&devices)},
        {QStringLiteral("editor"), QVariant::fromValue(&editor)},
        {QStringLiteral("library"), QVariant::fromValue(&libraryModel)},
        {QStringLiteral("libraryTransfer"), QVariant::fromValue(&libraryTransfer)},
        {QStringLiteral("bankBuilder"), QVariant::fromValue(&bankBuilder)},
        {QStringLiteral("bankLibrary"), QVariant::fromValue(&bankSourceModel)},
        {QStringLiteral("dashboard"), QVariant::fromValue(&dashboard)},
    });
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
        Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/XP60Studio/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    // After QML is up, land on Editor when the seeded fetch completed so the
    // first Demo Mode viewport is the musical workspace.
    if (demoMode) {
        QTimer::singleShot(250, &app, [&shell, &session] {
            if (session.patchFetch().state == xp60studio::services::DeviceSession::PatchFetchState::Completed) {
                shell.navigate(QStringLiteral("editor"));
            }
        });
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
