#include "support/FakeXp60.h"

#include "midi/LoopbackMidiTransport.h"
#include "presentation/AppShellViewModel.h"
#include "presentation/BankBuilderViewModel.h"
#include <QTemporaryDir>
#include <QUrl>

#include "library/LibraryDatabase.h"
#include "library/SyxImport.h"
#include "presentation/DevicesViewModel.h"
#include "presentation/DashboardViewModel.h"
#include "presentation/LibraryListModel.h"
#include "presentation/LibraryTransferViewModel.h"
#include "services/LibraryExportService.h"
#include "services/LibraryImportService.h"
#include "presentation/PatchEditorViewModel.h"
#include "presentation/QmlRegistration.h"
#include "services/DeviceSession.h"
#include "services/PatchTransfer.h"

#include <QFile>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QtQuickTest>

#include <chrono>
#include <memory>

namespace {

// One application stack: transport, session and the write engine over it.
struct Stack
{
    xp60studio::midi::LoopbackMidiTransport* transport = nullptr;
    std::unique_ptr<xp60studio::services::DeviceSession> session;
    std::unique_ptr<xp60studio::services::PatchTransfer> transfer;

    Stack()
    {
        auto loopback = std::make_unique<xp60studio::midi::LoopbackMidiTransport>();
        loopback->addInput("in-1", "XP-60 IN");
        loopback->addOutput("out-1", "XP-60 OUT");
        transport = loopback.get();
        session = std::make_unique<xp60studio::services::DeviceSession>(std::move(loopback));
        session->setAutomaticTimeoutPolling(false);
        auto pacing = session->pacing();
        pacing.interMessageDelay = std::chrono::milliseconds(0);
        session->setPacing(pacing);
        transfer = std::make_unique<xp60studio::services::PatchTransfer>(*session);
    }
};

} // namespace

// Provides real presentation objects to the QML tests.
//
// The Devices and Shell tests get a pristine stack (`testDevices`, `testShell`)
// so they can drive connect/fetch/arm from the beginning. The editor gets its
// own stack (`testEditor`) already holding a Patch read from the golden fixture
// through the ordinary fetch path, so the screen is exercised against real
// data without disturbing the other tests. `testHarness.reloadPatch()` puts
// the editor back to that just-fetched state between tests.
class Setup : public QObject
{
    Q_OBJECT

public:
    // Re-reads the Patch, which clears the editor's history and A/B state.
    Q_INVOKABLE void reloadPatch()
    {
        if (m_editorStack->transfer->liveActive()) m_editorStack->transfer->cancel();
        m_editorStack->session->fetchTemporaryPatch();
        pump(*m_editorStack);
    }
    Q_INVOKABLE void pumpEditor() { pump(*m_editorStack); }

    // A writable path for tests that need a real file. Kept in the harness so
    // no test invents a location of its own, and cleaned up with the harness.
    Q_INVOKABLE QUrl temporaryFileUrl(const QString& name)
    {
        if (!m_scratch) {
            m_scratch = std::make_unique<QTemporaryDir>();
        }
        return QUrl::fromLocalFile(m_scratch->filePath(name));
    }

public slots:
    void applicationAvailable()
    {
        QQuickStyle::setStyle(QStringLiteral("Basic"));
        xp60studio::presentation::registerQmlTypes();
    }

    void qmlEngineAvailable(QQmlEngine* engine)
    {
        engine->addImportPath(QStringLiteral("qrc:/qt/qml"));

        m_deviceStack = std::make_unique<Stack>();
        m_devices = std::make_unique<xp60studio::presentation::DevicesViewModel>(*m_deviceStack->session,
                                                                                m_deviceStack->transfer.get());
        m_shell = std::make_unique<xp60studio::presentation::AppShellViewModel>(m_devices.get());

        m_editorStack = std::make_unique<Stack>();
        m_editor = std::make_unique<xp60studio::presentation::PatchEditorViewModel>(*m_editorStack->session,
                                                                                    m_editorStack->transfer.get());
        m_fake = std::make_unique<xp60studio::testsupport::FakeXp60>(xp60studio::testsupport::temporaryAreaWith(4));
        m_editorStack->session->connectEndpoints("in-1", "out-1");
        reloadPatch();

        // A real library, in memory, holding the golden fixture's 128 patches,
        // so the Library screen is exercised against real names and real
        // provenance rather than invented rows.
        m_library = std::make_unique<xp60studio::library::LibraryDatabase>();
        if (m_library->open(QString::fromLatin1(xp60studio::library::LibraryDatabase::kInMemoryPath))) {
            QFile fixture(QStringLiteral(XP60STUDIO_FIXTURE_DIR "/user-bank-amal.syx"));
            if (fixture.open(QIODevice::ReadOnly)) {
                const QByteArray bytes = fixture.readAll();
                const auto* begin = reinterpret_cast<const xp60studio::roland::Byte*>(bytes.constData());
                xp60studio::library::SyxImportOptions options;
                options.sourceName = "user-bank-amal.syx";
                const auto imported = xp60studio::library::importSyxStream(
                    xp60studio::roland::ByteSpan(begin, static_cast<std::size_t>(bytes.size())), options);
                (void)m_library->insertAll(imported.entries);
            }
        }
        m_libraryModel = std::make_unique<xp60studio::presentation::LibraryListModel>();
        m_libraryModel->setDatabase(m_library.get());

        m_libraryImport = std::make_unique<xp60studio::services::LibraryImportService>(*m_library);
        m_libraryExport = std::make_unique<xp60studio::services::LibraryExportService>(*m_library);
        m_libraryTransfer = std::make_unique<xp60studio::presentation::LibraryTransferViewModel>(
            *m_libraryImport, *m_libraryExport);
        engine->rootContext()->setContextProperty(QStringLiteral("testLibrary"), m_libraryModel.get());
        m_dashboard = std::make_unique<xp60studio::presentation::DashboardViewModel>();
        m_dashboard->setDatabase(m_library.get());
        engine->rootContext()->setContextProperty(QStringLiteral("testLibraryTransfer"), m_libraryTransfer.get());
        engine->rootContext()->setContextProperty(QStringLiteral("testDashboard"), m_dashboard.get());
        engine->rootContext()->setContextProperty(QStringLiteral("testDevices"), m_devices.get());
        engine->rootContext()->setContextProperty(QStringLiteral("testShell"), m_shell.get());
        engine->rootContext()->setContextProperty(QStringLiteral("testEditor"), m_editor.get());

        // The Bank Builder browses the same library through its own list
        // model, exactly as the application wires it, so a filter set in a
        // Bank Builder test cannot change what the Library screen test sees.
        m_bankSourceModel = std::make_unique<xp60studio::presentation::LibraryListModel>();
        m_bankSourceModel->setDatabase(m_library.get());
        m_bankBuilder = std::make_unique<xp60studio::presentation::BankBuilderViewModel>();
        m_bankBuilder->setDatabase(m_library.get());
        engine->rootContext()->setContextProperty(QStringLiteral("testBankBuilder"), m_bankBuilder.get());
        engine->rootContext()->setContextProperty(QStringLiteral("testBankLibrary"), m_bankSourceModel.get());
        engine->rootContext()->setContextProperty(QStringLiteral("testHarness"), this);
    }

private:
    // Runs the fake conversation until nothing is left in flight.
    void pump(Stack& stack)
    {
        for (int i = 0; i < 40; ++i) {
            QCoreApplication::processEvents();
            const auto replies = m_fake->exchange(*stack.transport);
            for (const auto& reply : replies) {
                const auto bytes = reply.encode();
                stack.transport->injectIncoming(xp60studio::midi::MidiByteSpan(bytes.data(), bytes.size()));
            }
            QCoreApplication::processEvents();
            if (replies.empty() && stack.transport->sentMessages().empty() && stack.session->pendingSendCount() == 0) {
                return;
            }
        }
    }

    std::unique_ptr<Stack> m_deviceStack;
    std::unique_ptr<Stack> m_editorStack;
    std::unique_ptr<xp60studio::presentation::DevicesViewModel> m_devices;
    std::unique_ptr<xp60studio::presentation::AppShellViewModel> m_shell;
    std::unique_ptr<xp60studio::presentation::PatchEditorViewModel> m_editor;
    std::unique_ptr<xp60studio::testsupport::FakeXp60> m_fake;
    std::unique_ptr<xp60studio::library::LibraryDatabase> m_library;
    std::unique_ptr<xp60studio::presentation::LibraryListModel> m_libraryModel;
    std::unique_ptr<xp60studio::services::LibraryImportService> m_libraryImport;
    std::unique_ptr<xp60studio::services::LibraryExportService> m_libraryExport;
    std::unique_ptr<xp60studio::presentation::LibraryTransferViewModel> m_libraryTransfer;
    std::unique_ptr<xp60studio::presentation::DashboardViewModel> m_dashboard;
    std::unique_ptr<xp60studio::presentation::LibraryListModel> m_bankSourceModel;
    std::unique_ptr<xp60studio::presentation::BankBuilderViewModel> m_bankBuilder;
    std::unique_ptr<QTemporaryDir> m_scratch;
};

QUICK_TEST_MAIN_WITH_SETUP(xp60studio_qml, Setup)

#include "tst_qml_main.moc"
